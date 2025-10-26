#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "include/rust_itypes.h"
#include <string.h>

// [start]abcd[c]_______________[ce]efg[end]
typedef struct {
  u8* start;  // pointer to the start of buffer
  usize end;  // offset to end of the buffer
  usize c;  // offset to start of gap or cursor
  usize ce;  // offset to end of gap
  usize capacity;  // total capacity of gap buffer. can grow
} GapBuffer;

#define GAP_RESIZE_FACTOR 1024
#define GAPBUF_GAP_WIDTH(gap) (gap->ce - gap->c + 1)
#define GAPBUF_LEN(gap) (gap->c + (gap->end - gap->ce))
// buffer_index = logical_index - gap->c + gap->ce + 1
#define GAPBUF_GET_BUFFER_INDEX(gap, logical_index) (((logical_index) >= gap->c) ? (logical_index) - gap->c + gap->ce + 1 : (logical_index))
#define GAPBUF_GET_LOGICAL_INDEX(gap, buffer_index) (((buffer_index) > gap->ce) ? (buffer_index) + gap->c - gap->ce - 1 : (buffer_index))

// initialize gap buffer of capacity = size
GapBuffer gap_init(usize size) {
  GapBuffer gap = {0};
  gap.start = malloc(sizeof(u8) * size);
  if (!gap.start) {
    perror("failed to initialize gap buffer.");
    exit(-1);
  }
  memset(gap.start, 0, size);
  gap.capacity = size;
  gap.ce = gap.end = size - 1;
  return gap;
}

// frees gap buffer
void gap_free(GapBuffer* gap) {
  free(gap->start);
  gap->start = NULL;
  gap->end = gap->c = gap->ce = gap->capacity = 0;
}

// grow operation of gap buffer
void gap_grow(GapBuffer* gap) {
  isize ce_offset = gap->end - gap->ce;
  gap->capacity += GAP_RESIZE_FACTOR;
  gap->start = realloc(gap->start, gap->capacity);
  if (!gap->start) {
    perror("realloc failure");
    exit(-1);
  }

  gap->end = gap->capacity - 1;
  gap->ce = gap->end - ce_offset;
  if (gap->end > gap->ce) {  // copy characters after ce to the end if ce < end
    for (usize i = 0; i <= ce_offset; i++) {
      *(gap->start + gap->end - ce_offset + i) = *(gap->start + gap->ce + i); 
    }
  }
}

// insert operation of gap buffer.
void gap_insertc(GapBuffer* gap, u8 ch) {
  if (gap->c >= gap->ce) {
    gap_grow(gap);
  }
  *(gap->start + gap->c) = ch;
  gap->c++;
}

// remove operation
void gap_removec(GapBuffer* gap) {
  if (gap->c > 0) {
    gap->c--;
  }
}

// moves the gap max `n_ch` times to the left
void gap_left(GapBuffer* gap) {
  if (gap->c > 0) {
    *(gap->start + gap->ce) = *(gap->start + gap->c - 1);
    gap->ce--;
    gap->c--;
  }
}

// moves the gap max `n_ch` times to the right
void gap_right(GapBuffer* gap) {
  if (gap->ce < gap->end) {
    *(gap->start + gap->c) = *(gap->start + gap->ce + 1);
    gap->ce++;
    gap->c++;
  }
}

u8 gap_getch(const GapBuffer* gap, usize logical_index) {
  if (logical_index < GAPBUF_LEN(gap))
    return gap->start[GAPBUF_GET_BUFFER_INDEX(gap, logical_index)];
  return 0;
}

#undef GAP_RESIZE_FACTOR
