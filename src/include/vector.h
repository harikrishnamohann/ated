/*
  dynamic array (vector) implementation for generic type.
  methods:
    - insert
    - remove
    - set
    - get
    - free

  Configuration:
    You can specify either GROWTH_FACTOR or GROWTH_STEP before including
    both specifies the array resize rate

  array can be indexed backwords using VEND(n) macro
  example: <type>Vec_get(&vec, VEND(0)); // returns the last element in array
*/

#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "itypes.h"

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
} PREFIX##Vec;

#define __DEF_GROW(TYPE, PREFIX) \
void __##PREFIX##Vec_grow(PREFIX##Vec* vec) {\
  __SET_REALLOC_SIZE(vec->_capacity);\
  TYPE* new_elements = realloc(vec->_elements, sizeof(TYPE) * vec->_capacity);\
  if (new_elements == NULL) {\
    vec->error(err_malloc_failure);\
    return;\
  }\
  vec->_elements = new_elements;\
}

#define __DEF_INSERT(TYPE, PREFIX) \
void PREFIX##Vec_insert(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->length + pos + 1 : pos; \
  if (i > vec->length) {\
    vec->error(err_index_out_of_bounds);\
    return;\
  }\
  if (vec->length >= vec->_capacity) __##PREFIX##Vec_grow(vec); \
  memmove(&vec->_elements[i + 1], &vec->_elements[i], sizeof(TYPE) * (vec->length - i)); \
  vec->_elements[i] = val; \
  vec->length++; \
} 

#define __DEF_REMOVE(TYPE, PREFIX) \
TYPE PREFIX##Vec_remove(PREFIX##Vec* vec, isize pos) { \
  if (vec->length == 0) {\
    vec->error(err_index_out_of_bounds);\
    return (TYPE){0};\
  }\
  usize i = pos < 0 ? vec->length + pos: pos; \
  if (i >= vec->length) {\
    vec->error(err_index_out_of_bounds);\
    return (TYPE){0};\
  }\
  TYPE val = vec->_elements[i]; \
  memmove(&vec->_elements[i], &vec->_elements[i + 1], sizeof(TYPE) * (vec->length - i - 1)); \
  vec->length--; \
  return val; \
}

#define __DEF_GET(TYPE, PREFIX) \
TYPE PREFIX##Vec_get(const PREFIX##Vec* vec, isize pos) { \
  usize i = pos < 0 ? vec->length + pos: pos; \
  if (i >= vec->length) {\
    vec->error(err_index_out_of_bounds);\
    return (TYPE){0};\
  }\
  return vec->_elements[i]; \
}

#define __DEF_SET(TYPE, PREFIX) \
void PREFIX##Vec_set(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->length + pos: pos;\
  if (i >= vec->length) {\
    vec->error(err_index_out_of_bounds);\
    return;\
  }\
  vec->_elements[i] = val;\
}

#define __DEF_FREE(TYPE, PREFIX) \
void PREFIX##Vec_free(PREFIX##Vec* arr) { \
  free(arr->_elements);\
  *arr = (PREFIX##Vec){0};\
}

#define __DEF_NEW(TYPE, PREFIX)\
PREFIX##Vec PREFIX##Vec_init(usize capacity, err_handler_t func) {\
  PREFIX##Vec vec = {\
    ._elements = malloc(sizeof(TYPE) * capacity),\
    ._capacity = capacity,\
    .length = 0,\
  };\
  vec.error = (func) ? func : __default_err_handler;\
  if (vec._elements == NULL) {\
    fprintf(stderr, "failed to initialize vector\n");\
    exit(-1);\
  }\
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

