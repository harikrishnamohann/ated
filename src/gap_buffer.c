#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// [start]abcd[c]_______________[ce]efg[end]
struct gap_buf {
  char* start;  // pointer to the start of buffer
  char* end;  // pointer to end of the buffer
  char* c;  // start of gap
  char* ce;  // end of gap
  char* view;  // characters are displayed from here (for vertical scrolling)
  size_t capacity;  // total capacity of gap buffer. can grow
};

// initialize gap buffer of capacity = size
struct gap_buf gap_init(size_t size) {
  struct gap_buf gap;
  gap.capacity = size;
  gap.start = malloc(sizeof(char) * size);
  if (!gap.start) {
    perror("failed to initialize gap buffer.");
  }
  memset(gap.start, 0, size);
  gap.end = gap.start + size - 1;
  gap.c = gap.start;
  gap.ce = gap.end;
  gap.view = gap.start;
  return gap;
}

// frees gap buffer
void gap_free(struct gap_buf* gap) {
  free(gap->start);
  gap->start = NULL;
  gap->end = NULL;
  gap->c = NULL;
  gap->ce = NULL;
  gap->capacity = 0;
}

// grow operation of gap buffer
void gap_grow(struct gap_buf* gap) {
  // after realloc, memory addresses will change, hence the need to
  // recalculate the pointers of gap buffer. Offsets are calculated
  // to correctly setup c and ce pointers after realloc.
  uint32_t ce_offset = gap->end - gap->ce;
  uint32_t c_offset = gap->c - gap->start;
  uint32_t view_offset = gap->view - gap->start;

  gap->capacity *= 2;  // capacity is doubled
  gap->start = realloc(gap->start, gap->capacity);
  if (!gap->start) {
    perror("realloc failure");
  }

  gap->end = gap->start + gap->capacity - 1;

  gap->ce = gap->end - (gap->capacity / 2) - ce_offset;
  // this position is where ce pointed before realloc
  if (ce_offset > 0) {  // copy characters after ce to the end if ce < end
    for (int i = 0; i <= ce_offset; i++) {
      *(gap->end - ce_offset + i) = *(gap->ce + i); 
    }
  }
  gap->view = gap->start + view_offset;
  gap->ce = gap->end - ce_offset;  // this is the correct ce position
  gap->c = gap->start + c_offset;
}

// insert operation of gap buffer.
void gap_insert_ch(struct gap_buf* gap, char ch) {
  if (gap->c >= gap->ce) {
    gap_grow(gap);
  }
  *gap->c++ = ch;
}

// remove operation
void gap_remove_ch(struct gap_buf* gap) {
  if (gap->c > gap->start) {
    gap->c--;
  }
}

// moves the gap max `n_ch` times to the left
void gap_left(struct gap_buf* gap, int n_ch) {
  int offset = gap->c - gap->start;
  if (offset > 0) {
    for (int i = 0; offset > 0 && i < n_ch; i++) {
      *gap->ce = *(gap->c - 1);
      gap->ce--;
      gap->c--;
      offset--;
    }
  }
}

// moves the gap max `n_ch` times to the right
void gap_right(struct gap_buf* gap, int n_ch) {
  int offset = gap->end - gap->ce;
  if (offset > 0) {
    for (int i = 0; i < n_ch && offset > 0; i++) {
      *gap->c = *(gap->ce + 1);
      gap->ce++;
      gap->c++;
      offset--;
    }
  }
}
