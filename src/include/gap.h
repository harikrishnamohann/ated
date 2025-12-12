#pragma once

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include "itypes.h"

#define _RESIZE_FAC 1.6

// [start]abcd[c]_______________[ce]efg[end]
typedef struct {
  u32* start;  // pointer to the start of buffer
  u32 end;  // offset to end of the buffer
  u32 c;  // offset to start of gap or cursor
  u32 ce;  // offset to end of gap
  u32 capacity;  // total capacity of gap buffer. can grow
} GapBuffer;

// expands to give the width of gap in gap buffer
#define GAP_WIDTH(gap) ((gap)->ce - (gap)->c + 1)

// length of gap buffer without accounting for the gap
#define GAP_LEN(gap) ((gap)->c + ((gap)->end - (gap)->ce))

// buffer_index = logical_index - gap->c + gap->ce + 1
// 
// buf_index => used to index the actual gap buffer.
#define GAP_GET_BUFFER_INDEX(gap, logical_index) (((logical_index) >= (gap)->c) ? (logical_index) - (gap)->c + (gap)->ce + 1 : (logical_index))

// logical_index => used to index the gap buffer as if there were no gap.
#define GAP_GET_LOGICAL_INDEX(gap, buffer_index) (((buffer_index) > (gap)->ce) ? (buffer_index) + (gap)->c - (gap)->ce - 1 : (buffer_index))

// initialize gap buffer of capacity = size
static GapBuffer gap_init(u32 size) {
  GapBuffer gap = {0};
  gap.start = (u32*)malloc(sizeof(u32) * size);
  if (gap.start == NULL) {
    perror("failed to initialize gap buffer.");
    exit(-1);
  }
  gap.capacity = size;
  gap.ce = gap.end = size - 1;
  return gap;
}

// frees gap buffer
static void gap_free(GapBuffer* gap) {
  free(gap->start);
  gap->start = NULL;
  gap->end = gap->c = gap->ce = gap->capacity = 0;
}

// grow operation of gap buffer
static void gap_grow(GapBuffer* gap) {
  isize ce_offset = gap->end - gap->ce;
  gap->capacity *= _RESIZE_FAC;
  gap->start = (u32*)realloc(gap->start, sizeof(u32) * gap->capacity);
  if (!gap->start) {
    perror("realloc failure");
    exit(-1);
  }

  gap->end = gap->capacity - 1;
  if (ce_offset > 0) {
    memmove(gap->start + gap->end - ce_offset + 1, gap->start + gap->ce + 1, sizeof(u32) * ce_offset);
  }
  gap->ce = gap->end - ce_offset;
}

// insert operation of gap buffer.
static void gap_insert(GapBuffer* gap, u32 ch) {
  if (gap->c >= gap->ce) {
    gap_grow(gap);
  }
  *(gap->start + gap->c) = ch;
  gap->c++;
}

// remove from left operation
static void gap_removel(GapBuffer* gap) { if (gap->c > 0) gap->c--; }

// remove from right operation
static void gap_remover(GapBuffer* gap) { if (gap->ce < gap->end) gap->ce++; }

// moves the gap max `n_ch` times to the left
static void gap_left(GapBuffer* gap, u32 times) {
  while (times > 0 && gap->c > 0) {
    *(gap->start + gap->ce) = *(gap->start + gap->c - 1);
    gap->ce--;
    gap->c--;
    times--;
  }
}

// moves the gap max `n_ch` times to the right
static void gap_right(GapBuffer* gap, u32 times) {
  while (times > 0 && gap->ce < gap->end) {
    *(gap->start + gap->c) = *(gap->start + gap->ce + 1);
    gap->ce++;
    gap->c++;
    times--;
  }
}

// move gap to specified pos
static void gap_move(GapBuffer* gap, u32 pos) {
  if (gap->c > pos) {
    gap_left(gap, gap->c - pos);
  } else if (gap->c < pos) {
    gap_right(gap, pos - gap->c);
  }
}

// access the gap buffer using logical_indexing
static u32 gap_get(const GapBuffer* gap, u32 logical_index) {
  if (logical_index < GAP_LEN(gap))
    return gap->start[GAP_GET_BUFFER_INDEX(gap, logical_index)];
  return 0;
}

// modify ch at index
static void gap_set(GapBuffer* gap, u32 logical_index, u32 ch) {
  if (logical_index < GAP_LEN(gap))
    gap->start[GAP_GET_BUFFER_INDEX(gap, logical_index)] = ch;
}

#undef _RESIZE_FAC
