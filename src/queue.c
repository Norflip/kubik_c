#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FIFO QUEUE IMPLEMENTATION
queue_t queue_make(u64 stride, u64 capacity) {
    queue_t queue;
    memset(&queue, 0, sizeof(queue_t));
    queue.cap = 2;
    while (queue.cap < capacity)
        queue.cap <<= 1;
    queue.queue = malloc(queue.cap * stride);
    if (!queue.queue) {
        fprintf(stderr, "queue_create: failed to allocate memory for queue\n");
        exit(EXIT_FAILURE);
    }

    queue.stride = stride;
    queue.count = 0;
    queue.head = 0;
    queue.tail = 0;
    queue.allow_resizing = false;
    return queue;
}

void queue_destroy(queue_t *queue) {
    free(queue->queue);
    memset(queue, 0, sizeof(queue_t));
}

bool queue_enqueue(queue_t *queue, const void *elem) {
    if (queue->allow_resizing) {
        if (queue->count >= queue->cap) {
            u64 old_cap = queue->cap;
            queue->cap <<= 1;
            void *new_queue = realloc(queue->queue, queue->cap * queue->stride);
            if (!new_queue) {
                fprintf(stderr, "queue_enqueue: failed to resize queue\n");
                return false;
            }

            queue->queue = new_queue;
            // if head is ahead of tail, we need to move the wrapped-around elements
            if (queue->head > queue->tail) {
                size_t wrapped_count = old_cap - queue->head;
                memmove((void *)((uintptr_t)queue->queue + (old_cap * queue->stride)), (void *)((uintptr_t)queue->queue + (queue->head * queue->stride)), wrapped_count * queue->stride);
                queue->head = 0;
                queue->tail = old_cap;
            }
        }
    } else {
        if (queue->count >= queue->cap) {
            fprintf(stderr, "queue_enqueue: queue is full %llu\n", queue->cap);
            return false;
        }
    }

    memcpy((void *)((uintptr_t)queue->queue + (queue->tail * queue->stride)), elem, queue->stride);
    queue->tail = (queue->tail + 1) % queue->cap;
    queue->count++;
    return true;
}

bool queue_dequeue(queue_t *queue, void *out_elem) {
    if (queue->count <= 0) {
        return false;
    }
    memcpy(out_elem, (void *)((uintptr_t)queue->queue + (queue->head * queue->stride)), queue->stride);
    queue->head = (queue->head + 1) % queue->cap;
    queue->count--;
    return true;
}

bool queue_peek(const queue_t *queue, void *out_elem) {
    if (queue->count == 0) {
        return false;
    }
    memcpy(out_elem, (void *)((uintptr_t)queue->queue + (queue->head * queue->stride)), queue->stride);
    return true;
}

u64 queue_size(const queue_t *queue) {
    return queue->count;
}

bool queue_empty(const queue_t *queue) {
    return queue->count == 0;
}

// PRIORITY queueUE IMPLEMENTATION
static inline void swap_work(struct pqueue *pqueue, void *a, void *b) {
    memcpy(pqueue->_temp, a, pqueue->elem_size);
    memcpy(a, b, pqueue->elem_size);
    memcpy(b, pqueue->_temp, pqueue->elem_size);
}

static inline void *get_elem_ptr(pqueue_t *pqueue, size_t index) {
    return (void *)((uintptr_t)pqueue->pqueue + (index * pqueue->elem_size));
}

static void pqueueify_up(struct pqueue *pqueue, size_t index) {
    while (index > 0) {
        size_t parent_idx = (index - 1) / 2;
        if (pqueue->cmp(get_elem_ptr(pqueue, index), get_elem_ptr(pqueue, parent_idx)) >= 0)
            break;

        swap_work(pqueue, get_elem_ptr(pqueue, index), get_elem_ptr(pqueue, parent_idx));
        index = parent_idx;
    }
}

static void pqueueify_down(struct pqueue *pqueue, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left < pqueue->count && pqueue->cmp(get_elem_ptr(pqueue, left), get_elem_ptr(pqueue, smallest)) < 0)
            smallest = left;

        if (right < pqueue->count && pqueue->cmp(get_elem_ptr(pqueue, right), get_elem_ptr(pqueue, smallest)) < 0)
            smallest = right;

        if (smallest == index)
            break;

        swap_work(pqueue, get_elem_ptr(pqueue, index), get_elem_ptr(pqueue, smallest));
        index = smallest;
    }
}

pqueue_t pqueue_make(u64 stride, u64 capacity, int (*cmp)(const void *a, const void *b)) {
    pqueue_t pqueue;
    pqueue.cap = 2;
    while (pqueue.cap < capacity)
        pqueue.cap <<= 1;

    pqueue.pqueue = malloc(pqueue.cap * stride + stride);
    if (!pqueue.pqueue) {
        fprintf(stderr, "pqueue_create: failed to allocate memory for pqueue and temp storage\n");
        exit(EXIT_FAILURE);
    }

    pqueue._temp = (void *)((uintptr_t)pqueue.pqueue + pqueue.cap * stride);
    pqueue.cmp = cmp;
    pqueue.elem_size = stride;
    pqueue.count = 0;
    return pqueue;
}

void pqueue_destroy(pqueue_t *pqueue) {
    free(pqueue->pqueue);
    free(pqueue->_temp);
    memset(pqueue, 0, sizeof(pqueue_t));
}

bool pqueue_enqueue(pqueue_t *pqueue, const void *elem) {
    if (pqueue->count >= pqueue->cap) {
        pqueue->cap <<= 1;
        void *new_pqueue = realloc(pqueue->pqueue, pqueue->cap * pqueue->elem_size);
        if (!new_pqueue) {
            fprintf(stderr, "Failed to resize work queueue\n");
            return false;
        }
        pqueue->pqueue = new_pqueue;
    }

    memcpy(get_elem_ptr(pqueue, pqueue->count), elem, pqueue->elem_size);
    pqueue->count++;
    pqueueify_up(pqueue, pqueue->count - 1);
    return true;
}

bool pqueue_dequeue(pqueue_t *pqueue, void *out_elem) {
    if (pqueue->count == 0) {
        return false;
    }
    memcpy(out_elem, get_elem_ptr(pqueue, 0), pqueue->elem_size);
    pqueue->count--;
    if (pqueue->count > 0) {
        memcpy(get_elem_ptr(pqueue, 0), get_elem_ptr(pqueue, pqueue->count), pqueue->elem_size);
        pqueueify_down(pqueue, 0);
    }
    return true;
}

bool pqueue_peek(const pqueue_t *pqueue, void *out_elem) {
    if (pqueue->count == 0) {
        return false;
    }
    memcpy(out_elem, get_elem_ptr((pqueue_t *)pqueue, 0), pqueue->elem_size);
    return true;
}

u64 pqueue_size(const pqueue_t *pqueue) {
    return pqueue->count;
}

bool pqueue_is_empty(const pqueue_t *pqueue) {
    return pqueue->count == 0;
}
