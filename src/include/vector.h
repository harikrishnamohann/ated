/*
  dynamic array (vector) implementation for generic type.
  This library is implemented using an object oriented design approach.

  A vector object can be created as follows
    <prefix>Vec vec = new_<prefix>Vec(10, NULL); you can give an error handler instead of NULL

  It should be freed after use by calling
    vec.drop(&vec);

  NOTE: Define any of the GROWTH_METHOD below before including
    #define GROWTH_FACTOR 2    // Multiply capacity by 2
    #define GROWTH_STEP 100    // Add 100 elements
    // Default: +512 elements
    
  Vector Structure:
    States:
      - vec.type* _elements
      - vec.usize _capacity
      - vec.usize length
      - vec.error : ptr to store error handler procedure as specified in `err.h`
    Methods:
      - vec.insert(vec_ptr, value, pos)  : Insert value at pos
      - vec.remove(vec_ptr, pos)         : Remove and return value at pos
      - vec.get(vec_ptr, pos)            : Get value at pos
      - vec.set(vec_ptr, value, pos)     : Set value at pos
      - vec.drop(vec_ptr)                : Free all memory
    
  NOTE: vector can be indexed backwards by using putting VEND(index) in place of pos
  for example: int num = vec.get(&vec, VEND(0))
    
  Example usage:

    #define GROWTH_FACTOR 1.5
    #include "vector.h"

    VECTOR(u32, u32)  // Creates u32Vec type
    
    int main() {
      u32Vec vec = new_u32Vec(10, NULL);  // capacity=10, default error handler

      vec.insert(&vec, 42, 0);             // Insert 42 at index 0
      vec.insert(&vec, 100, VEND(0));      // Insert before last element
      u32 val = vec.get(&vec, 0);          // Get element at index 0
      vec.set(&vec, 50, VEND(0));          // Set last element to 50
      u32 removed = vec.remove(&vec, 0);   // Remove and return first element

      vec.drop(&vec);                      // Free memory
      return 0;
    }
*/

#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"

#if defined(GROWTH_FACTOR)
  #define __SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) * GROWTH_FACTOR)
#elif defined(GROWTH_STEP)
  #define __SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) + GROWTH_STEP)
#else
  #define __SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) + 512)
#endif

#define VEND(i) (-((i) + 1))

static void __default_err_handler(err_t err) {
  switch(err) {
    case err_malloc_failure: perror("memory allocation failed"); exit(-1);
    case err_index_out_of_bounds : fprintf(stderr, "err! you tried to reach an out of bound index in u32Vec\n"); exit(1);
  }
}

#define __DEF_TYPE(TYPE, PREFIX) \
typedef struct __##PREFIX##Vec { \
  TYPE* _elements; \
  usize _capacity; \
  usize length; \
  err_handler_t error; \
  void (*insert) (struct __##PREFIX##Vec* vec, TYPE val, isize pos); \
  TYPE (*remove) (struct __##PREFIX##Vec* vec, isize pos); \
  TYPE (*get) (const struct __##PREFIX##Vec* vec, isize pos); \
  void (*set) (struct __##PREFIX##Vec* vec, TYPE val, isize pos); \
  void (*drop)(struct __##PREFIX##Vec* vec); \
} PREFIX##Vec;



#define __DEF_GROW(TYPE, PREFIX) \
void __##PREFIX##Vec_grow(PREFIX##Vec* vec) {\
  __SET_REALLOC_SIZE(vec->_capacity);\
  vec->_elements = realloc(vec->_elements, sizeof(TYPE) * vec->_capacity);\
  vec->error(err_malloc_failure);\
}

#define __DEF_INSERT(TYPE, PREFIX) \
void __##PREFIX##Vec_insert(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->length + pos + 1 : pos; \
  if (i > vec->length) vec->error(err_index_out_of_bounds); \
  if (vec->length >= vec->_capacity) __##PREFIX##Vec_grow(vec); \
  memmove(&vec->_elements[i + 1], &vec->_elements[i], sizeof(TYPE) * (vec->length - i)); \
  vec->_elements[i] = val; \
  vec->length++; \
} 


#define __DEF_REMOVE(TYPE, PREFIX) \
TYPE __##PREFIX##Vec_remove(PREFIX##Vec* vec, isize pos) { \
  usize i = pos < 0 ? vec->length + pos: pos; \
  if (i > vec->length) vec->error(err_index_out_of_bounds); \
  TYPE val = vec->_elements[i]; \
  memmove(&vec->_elements[i], &vec->_elements[i + 1], sizeof(TYPE) * (vec->length - i - 1)); \
  vec->length--; \
  return val; \
}

#define __DEF_GET(TYPE, PREFIX) \
TYPE __##PREFIX##Vec_get(const PREFIX##Vec* vec, isize pos) { \
  usize i = pos < 0 ? vec->length + pos: pos; \
  if (i > vec->length) vec->error(err_index_out_of_bounds); \
  return vec->_elements[i]; \
}

#define __DEF_SET(TYPE, PREFIX) \
void __##PREFIX##Vec_set(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->length + pos: pos;\
  if (i > vec->length) vec->error(err_index_out_of_bounds);\
  vec->_elements[i] = val;\
}

#define __DEF_FREE(TYPE, PREFIX) \
void __##PREFIX##Vec_free(PREFIX##Vec* arr) { \
  free(arr->_elements);\
  *arr = (PREFIX##Vec){0};\
}

#define __DEF_NEW(TYPE, PREFIX)\
PREFIX##Vec new_##PREFIX##Vec(usize capacity, err_handler_t func) {\
  PREFIX##Vec vec = {\
    ._elements = malloc(sizeof(TYPE) * capacity),\
    ._capacity = capacity,\
    .length = 0,\
    .get = __##PREFIX##Vec_get,\
    .set = __##PREFIX##Vec_set,\
    .insert = __##PREFIX##Vec_insert,\
    .remove = __##PREFIX##Vec_remove,\
    .drop = __##PREFIX##Vec_free,\
  };\
  vec.error = (func) ? func : __default_err_handler;\
  if (vec._elements == NULL) vec.error(err_malloc_failure);\
  return vec;\
}

#define VECTOR(type, prefix) \
  __DEF_TYPE(type, prefix) \
  __DEF_GROW(type, prefix) \
  __DEF_INSERT(type, prefix) \
  __DEF_REMOVE(type, prefix) \
  __DEF_GET(type, prefix) \
  __DEF_SET(type, prefix) \
  __DEF_FREE(type, prefix) \
  __DEF_NEW(type, prefix)
