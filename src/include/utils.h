#pragma once

#include "itypes.h"
#include <time.h>

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// to make ctrl + ; key maps more readable
#ifndef CTRL
#define CTRL(x) ((x) & 0x1F)
#endif

static inline u64 ABS(i64 n) { return n < 0 ?  -(n) : n; }
static inline i64 DELTA(i64 a, i64 b) { return b - a; }
static inline isize MIN(isize a, isize b) { return a < b ? a : b; }
static inline isize MAX(isize a, isize b) { return a > b ? a : b; }

// can be used when you need to limit x between a min and max value (inclusive)
static i64 clamp(i64 x, i64 min, i64 max) {
  if (min > max) {
    max ^= min;
    min ^= max;
    max ^= min;
  }
  if (x < min) x = min;
  else if (x > max) x = max;
  return x;
}

// current_time and prev_time should be of type: struct timespec
static inline f32 elapsed_seconds(struct timespec* since) {
  struct timespec curr;
  clock_gettime(CLOCK_MONOTONIC, &curr);
  return (f32)(curr.tv_sec - since->tv_sec) + (f32)(curr.tv_nsec - since->tv_nsec) / 1e9;
}

#define SEC(s) (s)
#define MSEC(ms) ((f32)(ms) / 1000.0)

#define BYTE(n) (n)
#define KB(n) ((n) * 1024ULL)
#define MB(n) ((n) * 1024ULL * 1024ULL)
#define GB(n) ((n) * 1024ULL * 1024ULL * 1024ULL)
