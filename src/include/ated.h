#pragma once

#include <time.h>
#include "rust_itypes.h"

#define ALLOC_STEP 1024

// find min and max value
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// current_time and prev_time should be of type: struct timespec
static f32 delta_time(struct timespec* curr, struct timespec* prev) {
  return (f32)(curr->tv_sec - prev->tv_sec) + (curr->tv_nsec - prev->tv_nsec) / 1e9;
}

// to make ctrl + ; key maps more readable
#define CTRL(x) ((x) & 0x1F)

