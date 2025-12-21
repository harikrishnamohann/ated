#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "itypes.h"

#define GROWTH_FAC 1.5
#define __SET_REALLOC_SIZE(capacity) ((capacity) = (capacity) * GROWTH_FAC)

#define _END(i) (-((i) + 1))

enum {
  __daerr_malloc_failure,
  __daerr_index_out_of_bounds,
  __daerr_value_uninitialized,
};

static void __default_err_handler(int err, void* args) {
  switch(err) {
    case __daerr_malloc_failure: perror("memory allocation for dynamic arr failed"); exit(-1);
    case __daerr_index_out_of_bounds : fprintf(stderr, "err! can't reach an out of bound index from a dynamic arr\n"); exit(1);
    case __daerr_value_uninitialized : fprintf(stderr, "err! can't access values from an empty dynamic arr\n"); exit(1);

  }
}

typedef struct u32Da {
  u32* _elements;
  usize _capacity;
  usize len;
  err_handler_t error;
} u32Da;

static void __u32Da_grow(u32Da* da) {
  __SET_REALLOC_SIZE(da->_capacity);
  u32* new_elements = realloc(da->_elements, sizeof(u32) * da->_capacity);
  if (new_elements == NULL) {
    da->error(__daerr_malloc_failure, NULL);
    return;
  }
  da->_elements = new_elements;
}

/// Inserts value at position (supports negative indexing; -1 is end).
static void u32Da_insert(u32Da* da, u32 val, isize pos) { 
  usize i = pos < 0 ? da->len + pos + 1 : pos; 
  if (i > da->len) {
    da->error(__daerr_index_out_of_bounds, NULL);
    return;
  }
  if (da->len >= da->_capacity) __u32Da_grow(da); 
  memmove(&da->_elements[i + 1], &da->_elements[i], sizeof(u32) * (da->len - i)); 
  da->_elements[i] = val; 
  da->len++; 
} 

/// Removes and returns value at position (supports negative indexing).
static u32 u32Da_remove(u32Da* da, isize pos) { 
  if (da->len == 0) {
    da->error(__daerr_value_uninitialized, NULL);
    return (u32){0};
  }
  usize i = pos < 0 ? da->len + pos: pos; 
  if (i >= da->len) {
    da->error(__daerr_index_out_of_bounds, NULL);
    return (u32){0};
  }
  u32 val = da->_elements[i]; 
  memmove(&da->_elements[i], &da->_elements[i + 1], sizeof(u32) * (da->len - i - 1)); 
  da->len--; 
  return val; 
}

/// Returns value at position without removing (supports negative indexing).
static u32 u32Da_get(const u32Da* da, isize pos) { 
  usize i = pos < 0 ? da->len + pos: pos; 
  if (da->len == 0) {
    da->error(__daerr_value_uninitialized, NULL);
    return (u32){0};
  }
  if (i >= da->len) {
    da->error(__daerr_index_out_of_bounds, NULL);
    return (u32){0};
  }
  return da->_elements[i]; 
}

/// Updates value at position (supports negative indexing).
static void u32Da_set(u32Da* da, u32 val, isize pos) {
  usize i = pos < 0 ? da->len + pos: pos;
  if (da->len == 0) {
    da->error(__daerr_value_uninitialized, NULL);
    return;
  }
  if (i >= da->len) {
    da->error(__daerr_index_out_of_bounds, NULL);
    return;
  }
  da->_elements[i] = val;
}

/// Frees internal buffer and zeros out the structure.
static void u32Da_free(u32Da* arr) { 
  free(arr->_elements);
  *arr = (u32Da){0};
}

/// Initializes a new dynamic array with given capacity and error handler.
static u32Da u32Da_init(usize capacity, err_handler_t func) {
  u32Da da = {
    ._elements = malloc(sizeof(u32) * capacity),
    ._capacity = capacity,
    .len = 0,
  };
  da.error = (func) ? func : __default_err_handler;
  if (da._elements == NULL) {
    fprintf(stderr, "failed to initialize dynamic arr\n");
    exit(-1);
  }
  return da;
}

/// Resets length to 0 without deallocating memory.
static inline void u32Da_reset(u32Da* da) { da->len = 0; }
