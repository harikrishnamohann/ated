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
  #define __SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) + 1024)
#endif

#define _END(i) (-((i) + 1))

enum {
  __vecerr_malloc_failure,
  __vecerr_index_out_of_bounds,
  __vecerr_value_uninitialized,
};

static void __default_err_handler(int err, void* args) {
  switch(err) {
    case __vecerr_malloc_failure: perror("memory allocation for vector failed"); exit(-1);
    case __vecerr_index_out_of_bounds : fprintf(stderr, "err! can't reach an out of bound index from a Vector\n"); exit(1);
    case __vecerr_value_uninitialized : fprintf(stderr, "err! can't access values from an empty Vector\n"); exit(1);

  }
}

#define __DEF_TYPE(TYPE, PREFIX) \
typedef struct __##PREFIX##Vec { \
  TYPE* _elements; \
  usize _capacity; \
  usize len; \
  err_handler_t error; \
} PREFIX##Vec;

#define __DEF_GROW(TYPE, PREFIX) \
static void __##PREFIX##Vec_grow(PREFIX##Vec* vec) {\
  __SET_REALLOC_SIZE(vec->_capacity);\
  TYPE* new_elements = realloc(vec->_elements, sizeof(TYPE) * vec->_capacity);\
  if (new_elements == NULL) {\
    vec->error(__vecerr_malloc_failure, NULL);\
    return;\
  }\
  vec->_elements = new_elements;\
}

#define __DEF_INSERT(TYPE, PREFIX) \
static void PREFIX##Vec_insert(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->len + pos + 1 : pos; \
  if (i > vec->len) {\
    vec->error(__vecerr_index_out_of_bounds, NULL);\
    return;\
  }\
  if (vec->len >= vec->_capacity) __##PREFIX##Vec_grow(vec); \
  memmove(&vec->_elements[i + 1], &vec->_elements[i], sizeof(TYPE) * (vec->len - i)); \
  vec->_elements[i] = val; \
  vec->len++; \
} 

#define __DEF_REMOVE(TYPE, PREFIX) \
static TYPE PREFIX##Vec_remove(PREFIX##Vec* vec, isize pos) { \
  if (vec->len == 0) {\
    vec->error(__vecerr_value_uninitialized, NULL);\
    return (TYPE){0};\
  }\
  usize i = pos < 0 ? vec->len + pos: pos; \
  if (i >= vec->len) {\
    vec->error(__vecerr_index_out_of_bounds, NULL);\
    return (TYPE){0};\
  }\
  TYPE val = vec->_elements[i]; \
  memmove(&vec->_elements[i], &vec->_elements[i + 1], sizeof(TYPE) * (vec->len - i - 1)); \
  vec->len--; \
  return val; \
}

#define __DEF_GET(TYPE, PREFIX) \
static TYPE PREFIX##Vec_get(const PREFIX##Vec* vec, isize pos) { \
  usize i = pos < 0 ? vec->len + pos: pos; \
  if (vec->len == 0) {\
    vec->error(__vecerr_value_uninitialized, NULL);\
    return (TYPE){0};\
  }\
  if (i >= vec->len) {\
    vec->error(__vecerr_index_out_of_bounds, NULL);\
    return (TYPE){0};\
  }\
  return vec->_elements[i]; \
}

#define __DEF_SET(TYPE, PREFIX) \
static void PREFIX##Vec_set(PREFIX##Vec* vec, TYPE val, isize pos) { \
  usize i = pos < 0 ? vec->len + pos: pos;\
  if (vec->len == 0) {\
    vec->error(__vecerr_value_uninitialized, NULL);\
    return;\
  }\
  if (i >= vec->len) {\
    vec->error(__vecerr_index_out_of_bounds, NULL);\
    return;\
  }\
  vec->_elements[i] = val;\
}

#define __DEF_FREE(TYPE, PREFIX) \
static void PREFIX##Vec_free(PREFIX##Vec* arr) { \
  free(arr->_elements);\
  *arr = (PREFIX##Vec){0};\
}

#define __DEF_INIT(TYPE, PREFIX)\
static PREFIX##Vec PREFIX##Vec_init(usize capacity, err_handler_t func) {\
  PREFIX##Vec vec = {\
    ._elements = malloc(sizeof(TYPE) * capacity),\
    ._capacity = capacity,\
    .len = 0,\
  };\
  vec.error = (func) ? func : __default_err_handler;\
  if (vec._elements == NULL) {\
    fprintf(stderr, "failed to initialize vector\n");\
    exit(-1);\
  }\
  return vec;\
}

#define __DEF_RESET(TYPE, PREFIX) void PREFIX##Vec_reset(PREFIX##Vec* vec) { vec->len = 0; }

#define VECTOR(type, prefix) \
  __DEF_TYPE(type, prefix) \
  __DEF_GROW(type, prefix) \
  __DEF_INSERT(type, prefix) \
  __DEF_REMOVE(type, prefix) \
  __DEF_GET(type, prefix) \
  __DEF_SET(type, prefix) \
  __DEF_RESET(type, prefix) \
  __DEF_INIT(type, prefix) \
  __DEF_FREE(type, prefix) \

