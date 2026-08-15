#pragma once

#include <stdlib.h>
#include <stdio.h>

// typedef struct {
//     void* content;
//     usize len;
//     usize capacity;
// } TypeName;

#ifndef DA_INIT_SIZE
#define DA_INIT_SIZE 256
#endif

#define da_append(da, data)                                                              \
    do {                                                                                 \
        if ((da).len >= (da).capacity) {                                                 \
            if ((da).capacity == 0)                                                      \
                (da).capacity = DA_INIT_SIZE;                                            \
            else                                                                         \
                (da).capacity *= 2;                                                      \
            (da).content = realloc((da).content, (da).capacity * sizeof(*(da).content)); \
            if ((da).content == NULL) {                                                  \
                perror("dynamic_contentay: realloc");                                    \
                exit(EXIT_FAILURE);                                                      \
            }                                                                            \
        }                                                                                \
        (da).content[(da).len++] = data;                                                 \
    } while(0)                                                                           \
