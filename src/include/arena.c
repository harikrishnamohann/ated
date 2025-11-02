#pragma once

#include <assert.h>
#include <malloc.h>
#include "itypes.h"

#ifndef _BYTE_CONVERSION_
#define _BYTE_CONVERSION_
#define KB(n) ((n) * 1024ULL)
#define MB(n) ((n) * 1024ULL * 1024ULL)
#endif

typedef struct _arena {
  usize capacity;
  usize occupied;
  byte *buf;
  struct _arena *nxt;
} Arena;

Arena *arena_init(usize capacity) {
  assert(capacity);
  byte *new_buffer = malloc(sizeof(byte ) * capacity);
  Arena *arena = malloc(sizeof(Arena));
  assert(new_buffer);
  assert(arena);

  arena->capacity = capacity;
  arena->occupied = 0;
  arena->buf = new_buffer;
  arena->nxt = NULL;
  return arena;
}

void *arena_alloc(Arena *arena, usize size) {
  assert(arena);
  assert(size);
  Arena *curr = arena;
  while(curr->occupied + size > curr->capacity) {
    if(curr->nxt == NULL) {
      usize new_capacity = size > curr->capacity ? size : curr->capacity;
      curr->nxt = arena_init(new_capacity);
    }
    curr = curr->nxt;
  }
  void *mem = curr->buf + curr->occupied;
  curr->occupied += size;
  return mem;
}

// resets the allocated sizes to 0; doesn't actually frees any memory.
void arena_reset(Arena *arena) {
  Arena *current = arena;
  while(current != NULL) {
    current->occupied = 0;
    current = current->nxt;
  }
}

// Deallocates the entire arena.
void arena_free(Arena *arena) {
  Arena *current = arena;
  Arena *next;
  while(current != NULL) {
    free(current->buf);
    current->occupied = current->capacity = 0;
    next = current->nxt;
    free(current);
    current = next;
  }
}
