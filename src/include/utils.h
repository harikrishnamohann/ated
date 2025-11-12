#pragma once

#include "itypes.h"
#include <time.h>

// find min and max value
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// to make ctrl + ; key maps more readable
#define CTRL(x) ((x) & 0x1F)

static inline u64 ABS(i64 n) { return n < 0 ?  -n : n; }
static inline u64 ABSDIFF(i64 a, i64 b) { return (b > a) ? b - a : a - b; }

// current_time and prev_time should be of type: struct timespec
static inline f32 elapsed_seconds(struct timespec* since) {
  struct timespec curr;
  clock_gettime(CLOCK_MONOTONIC, &curr);
  return (f32)(curr.tv_sec - since->tv_sec) + (f32)(curr.tv_nsec - since->tv_nsec) / 1e9;
}

#define SEC(s) (s)
#define MSEC(ms) ((f32)(ms) / 1000.0)

#define KB(n) ((n) * 1024ULL)
#define MB(n) ((n) * 1024ULL * 1024ULL)
#define GB(n) ((n) * 1024ULL * 1024ULL * 1024ULL)
