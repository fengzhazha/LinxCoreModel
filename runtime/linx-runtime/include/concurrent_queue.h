#ifndef __linx

#ifndef __CONCURRENT_QUEUE
#define __CONCURRENT_QUEUE

#include <stdint.h>
#include <pthread.h>

typedef struct __node_t {
  uint64_t value;
  struct __node_t *next;
} node_t;

typedef struct __queue_t {
  node_t *head;
  node_t *tail;
  pthread_mutex_t head_lock;
  pthread_mutex_t tail_lock;
  uint32_t size;
} queue_t;

void queue_init(queue_t *q);

void queue_destroy(queue_t *q);

void queue_enqueue(queue_t *q, uint64_t value);

uint64_t queue_dequeue(queue_t *q);

uint32_t queue_size(queue_t *q);

#endif // __CONCURRENT_QUEUE

#endif // __linx