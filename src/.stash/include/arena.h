#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "itypes.h"

typedef struct arena {
    u64 fixed_chunk_size; // holds total capacity of the chunk.
    u64 buf_used_size;    // total used size in the chunk.
    u8 *buf;              // stores the actual chunk.
    struct arena *next;   // to face arena overflow.
} Arena;

#ifdef ARENA_IMPLEMENTATION

Arena *arena_init(uint64_t fixed_chunk_size)
{
    if (fixed_chunk_size <= 0) {
        fprintf(stderr, "chunk size must be a positive value\n");
        return NULL;
    }

    uint8_t *buf = malloc(sizeof(uint8_t) * fixed_chunk_size);
    if (buf == NULL) {
        perror("malloc");
        return NULL;
    }

    Arena *arena = malloc(sizeof(Arena));
    if (arena == NULL) {
        perror("malloc");
        return NULL;
    }

    arena->fixed_chunk_size = fixed_chunk_size;
    arena->buf_used_size = 0;
    arena->buf = buf;
    arena->next = NULL;
    return arena;
}

// Returns required size of memory from the arena to use.
// Returns NULL if the requested size is more than its capacity.
void *arena_alloc(Arena *arena, uint64_t size)
{
    if (size > arena->fixed_chunk_size) {
        fprintf(stderr, "ERROR! Arena chunk overflow\n");
        return NULL;
    }

    Arena *curr = arena;
    while(curr->buf_used_size + size > curr->fixed_chunk_size) {
        if(curr->next == NULL) {
            curr->next = arena_init(curr->fixed_chunk_size);
        }
        curr = curr->next;
    }

    void *ref = curr->buf + curr->buf_used_size;
    curr->buf_used_size += size;
    return ref;
}

// Resets the allocated sizes to 0. will not free any memory.
static inline void arena_reset(Arena *arena)
{
    for (Arena* curr = arena; curr != NULL; curr = curr->next) {
        curr->buf_used_size = 0;
    }
}

void arena_free(Arena *arena)
{
    for (Arena* curr = arena, *next = NULL; curr != NULL; curr = next) {
        free(curr->buf);
        curr->buf_used_size = 0;
        curr->fixed_chunk_size = 0;
        next = curr->next;
        free(curr);
    }
}
#endif /* ARENA_IMPLEMENTATION */
