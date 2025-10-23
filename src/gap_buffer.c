#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define GAP_RESIZE_FACTOR 1024
#define GAP_WIDTH(gap) (gap->ce - gap->c)
#define GAP_CH_COUNT(gap) (gap->end - GAP_WIDTH(gap))

// [start]abcd[c]_______________[ce]efg[end]
typedef struct {
  uint8_t* start;  // pointer to the start of buffer
  int32_t end;  // offset to end of the buffer
  int32_t c;  // offset to start of gap or cursor
  int32_t ce;  // offset to end of gap
  int32_t capacity;  // total capacity of gap buffer. can grow
} GapBuffer;

// initialize gap buffer of capacity = size
GapBuffer gap_init(uint32_t size) {
  GapBuffer gap = {0};
  gap.start = malloc(sizeof(uint8_t) * size);
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
  int32_t ce_offset = gap->end - gap->ce;
  gap->capacity += GAP_RESIZE_FACTOR;
  gap->start = realloc(gap->start, gap->capacity);
  if (!gap->start) {
    perror("realloc failure");
    exit(-1);
  }

  gap->end = gap->capacity - 1;
  gap->ce = gap->end - ce_offset;
  if (gap->end > gap->ce) {  // copy characters after ce to the end if ce < end
    for (uint32_t i = 0; i <= ce_offset; i++) {
      *(gap->start + gap->end - ce_offset + i) = *(gap->start + gap->ce + i); 
    }
  }
}

// insert operation of gap buffer.
void gap_insertc(GapBuffer* gap, uint8_t ch) {
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
void gap_left(GapBuffer* gap, uint32_t n_ch) {
  for (uint32_t i = 0; gap->c > 0 && i < n_ch; i++) {
    *(gap->start + gap->ce) = *(gap->start + gap->c - 1);
    gap->ce--;
    gap->c--;
  }
}

// moves the gap max `n_ch` times to the right
void gap_right(GapBuffer* gap, uint32_t n_ch) {
  for (uint32_t i = 0; gap->ce < gap->end && i < n_ch; i++) {
    *(gap->start + gap->c) = *(gap->start + gap->ce + 1);
    gap->ce++;
    gap->c++;
  }
}

uint8_t gap_getch(const GapBuffer* gap, uint32_t index) {
  if (index < GAP_CH_COUNT(gap)) {
    if (index >= gap->c) {
      index = gap->ce + (index - gap->c) + 1;
    }
    return gap->start[index];
  }
  return 0;
}

#undef GAP_RESIZE_FACTOR
