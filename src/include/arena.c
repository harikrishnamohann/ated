/*
  A versatile arena allocator implementation.
  Not properly tested neither matured.
  It should learn and adapt from it's master.
*/
#pragma once

#include <assert.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include "itypes.h"

#define KB(n) ((n) * 1024ULL)
#define MB(n) ((n) * 1024ULL * 1024ULL)

struct __amapping {
  byte* start;
  usize len;
  struct __amapping* nxt;
};

typedef struct Arena {
  byte* buf;
  usize capacity;
  struct __amapping* mapping;
  struct Arena* nxt;
} Arena;

// initializes the arena chunk with capacity
Arena *arena_init(usize capacity) {
    assert(capacity);
    byte *new_buffer = malloc(capacity);
    Arena *arena = malloc(sizeof(Arena));
    assert(new_buffer);
    assert(arena);
    arena->capacity = capacity;
    arena->mapping = NULL;
    arena->buf = new_buffer;
    arena->nxt = NULL;
    return arena;
}

static struct __amapping* __amap_init(void* start, usize len) {
  struct __amapping* amap = malloc(sizeof(struct __amapping));
  assert(amap);
  amap->len = len;
  amap->start = start;
  amap->nxt = NULL;
  return amap;
}

static void __amap_free(Arena* arena) {
  struct __amapping* curr = arena->mapping;
  while(curr != NULL) {
    struct __amapping* tmp = curr;
    curr = curr->nxt;
    free(tmp);
  }
  arena->mapping = NULL;
}

static struct __amapping* __amap_assign(Arena* arena, usize size) {
  if (arena->mapping == NULL) { // map is uninitialized
    arena->mapping = __amap_init(arena->buf, size);
    return arena->mapping;
  } else if (arena->mapping->start - arena->buf >= size) { // map is initialized, but first block has been dropped previously.
    struct __amapping* tmp = __amap_init(arena->buf, size);
    tmp->nxt = arena->mapping;
    arena->mapping = tmp;
    return tmp;
  }

  struct __amapping* prev = arena->mapping, *curr = arena->mapping->nxt;
  while (curr != NULL) {
    usize gap = curr->start - (prev->start + prev->len);
    if (gap >= size) { // there is free space between two blocks
      struct __amapping* tmp = __amap_init(prev->start + prev->len, size);
      tmp->nxt = curr;
      prev->nxt = tmp;
      return tmp;
    }
    prev = curr;
    curr = curr->nxt;
  }

  if ((arena->buf + arena->capacity) - (prev->start + prev->len) >= size) { // allocating as last block
    prev->nxt = __amap_init(prev->start + prev->len, size);
    return prev->nxt;
  }
  return NULL; // arena have no free space to perform allocation
}

// Returns required size of memory from the arena to use.
void *arena_alloc(Arena *arena, usize size) {
  assert(arena);
  assert(size);
  
  Arena *curr = arena, *prev = NULL;
  struct __amapping* mapped = NULL;

  while (curr != NULL && !mapped) {
    mapped = __amap_assign(curr, size);
    prev = curr;
    curr = curr->nxt;
  } 

  if (!mapped && curr == NULL) { // couldn't find any gap
    usize new_capacity = size > arena->capacity ? size : arena->capacity;
    prev->nxt = arena_init(new_capacity);
    curr = prev->nxt;
    mapped = __amap_assign(curr, size);
  }
  return mapped ? (void*)mapped->start : NULL;
}

// resets the arena without freeing the allocated space
void arena_reset(Arena *arena) {
  Arena *current = arena;
  while(current != NULL) {
    __amap_free(current);
    current = current->nxt;
  }
}

// to free individual blocks of arena. the dropped block may be
// used in a future alloc request
void arena_drop(Arena *arena, void *addr) {
  assert(arena);
  assert(addr);

  Arena *curr = arena;

  while (curr != NULL) {
    struct __amapping *map = curr->mapping, *prev = NULL;

    while (map != NULL) {
      if (map->start == (byte*)addr) {
        if (prev == NULL) {
          curr->mapping = map->nxt;  // First node
        } else {
          prev->nxt = map->nxt;      // Other nodes
        }
        free(map);
        return;
      }
      prev = map;
      map = map->nxt;
    }
      
    curr = curr->nxt;
  }
}

void* arena_realloc(Arena* arena, void *addr, usize newsiz) {
  assert(arena);
  assert(addr);
  assert(newsiz);

  Arena* curr = arena;
  struct __amapping* target = NULL;

  while (curr != NULL) {
    target = curr->mapping;
    while (target != NULL) {
      if (target->start == (byte*)addr) goto target_found;
      target = target->nxt;
    }
    curr = curr->nxt;
  }
target_found:
  assert(target);
  byte* new = arena_alloc(arena, newsiz);
  for (usize i = 0; i < target->len && i < newsiz; i++) new[i] = target->start[i];
  arena_drop(arena, target->start);
  return (void*)new;
}

// Deallocates the entire arena.
void arena_free(Arena *arena) {
  Arena *curr = arena;
  while(curr != NULL) {
    __amap_free(curr);
    free(curr->buf);
    curr->capacity = 0;
    Arena* next = curr->nxt;
    free(curr);
    curr = next;
  }
}
