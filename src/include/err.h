#pragma once

typedef enum {
  err_malloc_failure = -1,
  err_index_out_of_bounds = 1
} err_t;

typedef void (*err_handler_t) (err_t err);
