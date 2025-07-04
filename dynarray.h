#ifndef DYNARRAY_H
#define DYNARRAY_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <immintrin.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#define SG_WORD sizeof(size_t)
#define VECTOR_INITIAL_CAPACITY 8

/* put this in some utilities header or something */
#if defined(__GNUC__) || defined(__clang__)

#define FORCE_INLINE __attribute__((always_inline)) inline
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define COLD __attribute__((cold))
#define NO_INLINE __attribute__((noinline))
#elif defined(__MSC_VER)

#define NO_INLINE
#define FORCE_INLINE __forceinline
#define UNLIKELY(x) (x)
#define LIKELY(x) (x)
#define COLD

#else   /*MSVC is the only crappy compiler that doesnt support this*/

#define NO_INLINE __declspec(noinline)
#define FORCE_INLINE __forceinline
#define UNLIKELY(x) (x)
#define LIKELY(x) (x)
#define COLD

#endif

    

typedef enum
{
    VECTOR_NO_ERROR,
    VECTOR_ALLOCATION_ERROR,
    VECTOR_FREED,
} vector_err_flag;

typedef struct
{
    void *data;
    size_t size;        /* NOT IN BYTES, THIS IS THE COUNT BASICALLY */
    size_t capacity;    /* THIS IS IN ELEMENTS NOT BYTES, REMEMBER THIS YOU MONGREL */
    size_t capacity_bytes;
    size_t elem_size;

    vector_err_flag status;
} vector;

vector initialize_vec(size_t elem_size);

void free_vector(vector *vec);

COLD int vector_push_back_resize(vector* vec);

#define VEC_PUSH_BACK(type, vec, element)do\
{\
    if(UNLIKELY(vec.size >= vec.capacity))\
    {\
        vector_push_back_resize(&vec);\
    }\
    ((type *) vec.data)[vec.size++] = element;\
}\
while(0)\

//returns the number itself if number >= top bit
FORCE_INLINE size_t round_next_pow2(size_t number)
{
    if(number < 2)
    {
        return 1;
    }
    else if(number & ((size_t)0x1 << (sizeof(size_t) * 8 - 1)))
    {
        return number;
    }
#if defined(__GNUC__) || defined(__CLANG__)

#if SIZE_MAX > 0xFFFFFFF
    #define builtin_clz(x) __builtin_clzll(x)
#else
    #define builtin_clz(x) __builtin_clz(x)
#endif

    //count the leading bits to figure out the number of bits needed to shift 0x1 to get nearest pow
    return (size_t)0x1 << ((sizeof(size_t) * 8) - (size_t)builtin_clz(--number));
#elif defined(__MSC_VER)

#if SIZE_MAX > 0xFFFFFFFF
    #define bitscan_reverse(x, y) _BitScanReverse64(x, y)
#else
    #define bitscan_reverse(x, y) _BitScanReverse(x, y)
#endif

    unsigned long idx;
    bitscan_reverse(&idx, --number);

    return (size_t)0x1 << (idx + 1);

#else
    number--;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    number |= number >> 32;
#endif
    number++;

    return number;
#endif    
}
NO_INLINE COLD int resize_vector_cold(vector *vec, size_t size);

FORCE_INLINE int resize_vector(vector *vec, size_t size)
{
    //only realloc when size > capacity
    if(UNLIKELY(size > vec->capacity))
    {
        return resize_vector_cold(vec, size);
    }

    vec->size = size;

    return 0;
}

int insert_vector(vector *vec, const void *elements, size_t start, size_t end);

FORCE_INLINE int shrink_to_fit_vector(vector *vec)
{
    if(!vec)
        return -1;

    if(vec->size < vec->capacity)
    {
        void *tmp = realloc(vec->data, vec->size * vec->elem_size);
        
        if(!tmp)
        {
            vec->status = VECTOR_ALLOCATION_ERROR;
            return -1;
        }

        vec->data = tmp;
        vec->capacity = vec->size;
    }

    return 0;
}

FORCE_INLINE int reserve_vector(vector *vec, size_t size)
{
    if(vec->capacity < size)
    {
        void *tmp = realloc(vec->data, size * vec->elem_size);
        if(!tmp)
        {
            vec->status = VECTOR_ALLOCATION_ERROR;
            return -1;
        }

        vec->capacity = size;
        vec->data = tmp;
    }
    return 0;
}


int simd_memory_move_bwd(void *dest, const void *src, size_t bytes);

int simd_memory_move_fwd(void *dest, const void *src, size_t bytes);

//removes one element from the vector
FORCE_INLINE void erase_vector(vector *vec, size_t idx, size_t end)
{
    if (idx >= vec->size || end == 0 || idx + end > vec->size)
        return;

    if(vec->size > idx)
    {
        simd_memory_move_fwd(((uint8_t *) vec->data) + idx * vec->elem_size, 
                             ((uint8_t *) vec->data) + (idx + end) * vec->elem_size, 
                             (vec->size - idx - end) * vec->elem_size);
    }

    vec->size -= end;
}
#define VEC_POP_BACK(vec) vec.size--

#endif

