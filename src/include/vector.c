/*
  This library provides macros to create a vector (a dynamic array) of any type.
*/
#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "itypes.h"

#if defined(GROWTH_FACTOR)
  #define SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) * GROWTH_FACTOR)
#elif defined(GROWTH_STEP)
  #define SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) + GROWTH_STEP)
#else
  #define SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) + 512)
#endif

#define VEND(i) (-((i) + 1))

typedef struct __u32Vec {
  u32* _elements;
  usize _capacity;
  usize length;  
  void (*insert) (struct __u32Vec* vec, u32 val, isize pos);
  u32 (*remove) (struct __u32Vec* vec, isize pos);
  u32 (*get) (const struct __u32Vec* vec, isize pos);
  void (*set) (struct __u32Vec* vec, u32 val, isize pos);
  void (*drop)(struct __u32Vec* vec);
} u32Vec;

void __u32Vec_grow(u32Vec* vec) {
  SET_REALLOC_SIZE(vec->_capacity);
  vec->_elements = realloc(vec->_elements, sizeof(u32) * vec->_capacity);
  if (vec->_elements == NULL) {
    perror("failed to grow u32Vec");
    exit(-1);
  }
}

void __u32Vec_insert(u32Vec* vec, u32 val, isize pos) {
  usize i = pos < 0 ? vec->length + pos + 1 : pos;
  if (i > vec->length) {
    fprintf(stderr, "index out of bounds encountered for u32Vec\n");
    exit(1);
  };
  if (vec->length >= vec->_capacity) __u32Vec_grow(vec);
  memmove(&vec->_elements[i + 1], &vec->_elements[i], sizeof(u32) * (vec->length - i));
  vec->_elements[i] = val;
  vec->length++;
}

u32 __u32Vec_remove(u32Vec* vec, isize pos) {
  usize i = pos < 0 ? vec->length + pos: pos;
  if (i > vec->length) {
    fprintf(stderr, "index out of bounds encountered for u32Vec\n");
    exit(1);
  };
  u32 val = vec->_elements[i];
  memmove(&vec->_elements[i], &vec->_elements[i + 1], sizeof(u32) * (vec->length - i - 1));
  vec->length--;
  return val;
}

u32 __u32Vec_get(const u32Vec* vec, isize pos) {
  usize i = pos < 0 ? vec->length + pos: pos;
  if (i >= vec->length) {
    fprintf(stderr, "index out of bounds encountered for u32Vec\n");
    exit(1);
  };
  return vec->_elements[i];
}

void __u32Vec_set(u32Vec* vec, u32 val, isize pos) {
  usize i = pos < 0 ? vec->length + pos: pos;
  if (i > vec->length) {
    fprintf(stderr, "index out of bounds encountered for u32Vec\n");
    exit(1);
  };
  vec->_elements[i] = val;
}

void __u32Vec_free(u32Vec* arr) {
  free(arr->_elements);
  *arr = (u32Vec){0};
}

u32Vec new_u32Vec(usize capacity) {
  u32Vec vec = {
    ._elements = malloc(sizeof(u32) * capacity),
    ._capacity = capacity,
    .length = 0,
    .get = __u32Vec_get,
    .set = __u32Vec_set,
    .insert = __u32Vec_insert,
    .remove = __u32Vec_remove,
    .drop = __u32Vec_free,
  };
  if (vec._elements == NULL) {
    perror("failed to initialize u32Vec");
    exit(-1);
  }
  return vec;
}

#undef SET_REALLOC_SIZE
