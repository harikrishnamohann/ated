#pragma once

#include "rust_itypes.h"
#include <assert.h>
#include <stdlib.h>

#define INDX_SIZE 256

struct _maps {
  usize start;
  usize len;
};

typedef struct {
  void* locker;
  struct {
    struct _maps* maps;
    u32 mapped;
    u32 capacity;
  } index;
  usize capacity;
} Bank;

Bank bank_init(usize size) {
  Bank sbi = {
    .capacity = size,
    .locker = (void*)malloc(size),
    .index = {
      .mapped = 0,
      .capacity = INDX_SIZE,
      .maps = (struct _maps*)malloc(sizeof(struct _maps) * INDX_SIZE)
    }
  };
  assert(sbi.locker != NULL);
  assert(sbi.locker != NULL);
  return sbi;
}

usize bank_alloc(Bank* jp, usize size) {
  return 0;
}
