#pragma once

#include <stdio.h>
#include <stdlib.h>
#include "itypes.h"

#ifndef Q_TYPE
#define Q_TYPE u64
#define Q_ERR U64_MAX
#endif

#ifdef Q_ERR
typedef struct {
    Q_TYPE *buf;
    u64 front;
    u64 rear;
    u64 capacity;
} Queue;
#endif

#ifdef Q_IMPLEMENTATION

#define Q_INITIAL_CAPACITY 64
#define Q_UNDEFINED U64_MAX
#define Q_ISFULL(q) (((q)->rear + 1) % (q)->capacity == (q)->front)
#define Q_ISEMPTY(q) ((q)->rear == Q_UNDEFINED)
#define Q_GROWTH_FAC 2

Queue queue_init()
{
    Queue que = (Queue) {
        .buf = NULL,
        .front = Q_UNDEFINED,
        .rear = Q_UNDEFINED,
        .capacity = Q_INITIAL_CAPACITY,
    };
    que.buf = malloc(sizeof(Q_TYPE) * Q_INITIAL_CAPACITY);
    if (que.buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return que;
}

void enqueue(Queue *que, Q_TYPE val)
{
    if (Q_ISFULL(que)) {
        u64 old_front_offset_end = que->capacity - que->front;
        que->capacity = que->capacity * Q_GROWTH_FAC;
        que->buf = realloc(que->buf, que->capacity * sizeof(Q_TYPE));
        if (que->buf == NULL) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        if (que->rear < que->front) {
            u64 new_front = que->capacity - old_front_offset_end;
            u64 old_front = que->front;
            for (u64 i = 0; i < old_front_offset_end; i++) {
                que->buf[new_front + i] = que->buf[old_front + i];
            }
            que->front = new_front;
        }
    }

    if (Q_ISEMPTY(que)) { 
        que->front = que->rear = 0;
    } else if (que->rear == que->capacity - 1) {
        que->rear = 0;
    } else {
        que->rear++;
    }
    que->buf[que->rear] = val;
}

Q_TYPE dequeue(Queue *que)
/* case 1: queue is empty (front == Q_UNDEFINED)
case 2: val is the last element (front == rear)
case 3: front can circle back to 0 (front == capacity - 1)
case 4: normal case */
{
    if (Q_ISEMPTY(que)) {
        return Q_ERR;
    }
    Q_TYPE val = que->buf[que->front];
    if (que->front == que->rear) {
        que->front = que->rear = Q_UNDEFINED;
    } else if (que->front == que->capacity - 1) {
        que->front = 0;
    } else {
        que->front++;
    }
    return val;
}

void qfree(Queue* que) {
    que->front = que->rear = que->capacity = 0;
    free(que->buf);
    que->buf = NULL;
}

#endif
