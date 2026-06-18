#ifndef __linx

#include "concurrent_queue.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

// queue implementation start

const uint64_t g_invalid_value = (uint64_t)-1;

void queue_init(queue_t *q) {
  node_t *dummy = malloc(sizeof(node_t));
  dummy->value = 0;
  dummy->next = NULL;

  q->head = dummy;
  q->tail = dummy;
  q->size = 0;
  pthread_mutex_init(&q->head_lock, NULL);
  pthread_mutex_init(&q->tail_lock, NULL);
}

void queue_destroy(queue_t *q) {
  while (q->head != q->tail) {
    node_t *new_head = q->head->next;
    if (new_head == NULL) {
      return;
    }

    node_t *old_head = q->head;
    free(old_head);
    q->head = new_head;
    q->size--;
  }
}

void queue_enqueue(queue_t *q, uint64_t value) {
  node_t *new_node = malloc(sizeof(node_t));
  new_node->value = value;
  new_node->next = NULL;
  assert(new_node);

  pthread_mutex_lock(&q->tail_lock);
  q->tail->next = new_node;
  q->tail = new_node;
  q->size++;
  pthread_mutex_unlock(&q->tail_lock);
}

uint64_t queue_dequeue(queue_t *q) {
  pthread_mutex_lock(&q->head_lock);
  node_t *new_head = q->head->next;
  if (new_head == NULL) {
    pthread_mutex_unlock(&q->head_lock);
    return g_invalid_value;
  }

  node_t *old_head = q->head;
  q->head = new_head;
  uint64_t ret_value = new_head->value;
  q->size--;
  pthread_mutex_unlock(&q->head_lock);
  free(old_head);

  return ret_value;
}

uint32_t queue_size(queue_t *q) {
  return q->size;
}

#endif