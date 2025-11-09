#ifndef DS_H
#define DS_H
#include "utils.h"
#include <stdbool.h>

#define DEBUG_QUEUE 1

// simple FIFO queueue
typedef struct queue {
    void *queue;
    u64 stride;
    u64 head, tail;
    u64 count, cap;
    bool allow_resizing;
} queue_t;

queue_t queue_make(u64 stride, u64 capacity);
void queue_destroy(queue_t *queue);
bool queue_enqueue(queue_t *queue, const void *elem);
bool queue_dequeue(queue_t *queue, void *out_elem);
bool queue_peek(const queue_t *queue, void *out_elem);
u64 queue_size(const queue_t *queue);
bool queue_empty(const queue_t *queue);

// priority queue
typedef struct pqueue {
    void *pqueue;
    u64 elem_size;
    u64 count, cap;
    void *_temp;
    int (*cmp)(const void *a, const void *b);
} pqueue_t;

pqueue_t pqueue_make(u64 stride, u64 capacity, int (*cmp)(const void *a, const void *b));
void pqueue_destroy(pqueue_t *pqueue);
bool pqueue_enqueue(pqueue_t *pqueue, const void *elem);
bool pqueue_dequeue(pqueue_t *pqueue, void *out_elem);
bool pqueue_peek(const pqueue_t *pqueue, void *out_elem);
u64 pqueue_size(const pqueue_t *pqueue);
bool pqueue_is_empty(const pqueue_t *pqueue);

#endif // DS_H