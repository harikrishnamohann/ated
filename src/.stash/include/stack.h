#pragma once
#include <stdlib.h>
#include <stdio.h>
#include "itypes.h"
#include "utils.h"

#ifndef STK_TYPE
#define STK_TYPE u64
#define STK_ERR U64_MAX
#endif

#ifdef STK_ERR
typedef struct {
    STK_TYPE *buf;
    u64 top;
    u64 capacity;
} Stack;
#endif

#ifdef STK_IMPLEMENTATION

#define STK_INITIAL_CAPACITY 64
#define STK_GROWTH_FAC 2
#define STK_EMPTY U64_MAX
#define STK_ISEMPTY(stk) ((stk)->top == STK_EMPTY)

static Stack stack_init(void)
{
    Stack stk = {
        .buf = NULL,
        .top = STK_EMPTY,
        .capacity = STK_INITIAL_CAPACITY,
    };

    stk.buf = malloc(stk.capacity * sizeof(STK_TYPE));
    if (stk.buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return stk;
}

void push(Stack *stk, STK_TYPE val)
{
    if (STK_ISEMPTY(stk))
        stk->top = 0;
    else
        stk->top++;

    if (stk->top >= stk->capacity) {
        stk->capacity = stk->capacity * STK_GROWTH_FAC;
        void *tmp = realloc(stk->buf, stk->capacity * sizeof(STK_TYPE));
        if (tmp == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        stk->buf = tmp;
    }

    stk->buf[stk->top] = val;
    debug("PUSH[%lu]\n", val);
}

STK_TYPE pop(Stack *stk)
{
    if (STK_ISEMPTY(stk))
        return STK_ERR;

    STK_TYPE val = stk->buf[stk->top];
    if (stk->top == 0)
        stk->top = STK_EMPTY;
    else
        stk->top--;
    debug("POP[%lu]\n", val);
    return val;
}

STK_TYPE peek(Stack *stk)
{
    if (STK_ISEMPTY(stk)) return STK_ERR;
    return stk->buf[stk->top];
}

void stk_free(Stack *stk)
{
    free(stk->buf);
    stk->buf = NULL;
    stk->top = stk->capacity = 0;
}
#endif /* STACK_IMPLEMENTATION */

