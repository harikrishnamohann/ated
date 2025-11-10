#pragma once

#include "itypes.h"

// find min and max value
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// to find the center of offseted length
#define CENTER(length, offset) ((((length) - (offset)) / 2))

// to make ctrl + ; key maps more readable
#define CTRL(x) ((x) & 0x1F)

// to convert kilo-bytes and mega-bytes to bytes
#ifndef _BYTE_CONVERSION_
#define _BYTE_CONVERSION_
#define KB(n) ((n) * 1024ULL)
#define MB(n) ((n) * 1024ULL * 1024ULL)
#endif

static inline u64 ABS(i64 n) { return n < 0 ?  -n : n; }
static inline u64 ABSDIFF(i64 a, i64 b) { i64 r = b - a; return (r < 0) ? -r : r; }

/*
################################
## About measuring delta time ##
################################
#include <time.h>

current_time and prev_time should be of type: struct timespec
static f32 delta_time(struct timespec* curr, struct timespec* prev) {
  return (f32)(curr->tv_sec - prev->tv_sec) + (curr->tv_nsec - prev->tv_nsec) / 1e9;
}

## example:
struct timespec prev_time, curr_time;
clock_gettime(CLOCK_MONOTONIC, &prev_time);

wait for user input or do some action here.

clock_gettime(CLOCK_MONOTONIC, &curr_time);
if (delta_time(&curr_time, &prev_time) > THRESHOLD) {

  do response for exeeding THRESHOLD limit here.

}
prev_time = curr_time;
*/

