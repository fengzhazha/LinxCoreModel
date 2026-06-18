#include "co.h"
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define STACK_SIZE 16*1024

enum co_status {
  CO_NEW = 1,
  CO_RUNNING,
  CO_WAITING, // suspend
  CO_DEAD,    // done, but not freed
};

struct co {
  char *name;
  void (*func)(void *); // co_start function
  void *arg;

  enum co_status status;
  struct co *    parent;  // parent coroutine
  jmp_buf        context;
  uint8_t        stack[STACK_SIZE]; // call stack for coroutine
};

// A linked list to maintain coroutine states
// TODO: A coroutine scheduler (e.g. MP) will replace
// the linked list for better performance in future.
struct node_{
  struct co* co;
  struct node_* next;
  struct node_* prev;
};

struct co* current = NULL;
struct node_ *head = NULL;
struct node_ *tail = NULL;

// noinline is mandatory
static __attribute__ ((noinline)) void stack_switch_call_cleanup() {
  current->status = CO_DEAD;
  current = current->parent;
  longjmp(current->context, 1);
  return;
}

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; call *%1;"
      : : "b"((uintptr_t)sp), "d"(entry), "a"(arg) : "memory"
#elif __aarch64__
    "mov sp, %0; mov x0, %2; blr %1;"
    : : "r"((uintptr_t)sp), "r"(entry), "r"(arg) : "memory"
#else
    "Invalid Arch asm"
#endif
  );
  stack_switch_call_cleanup();

}

static inline struct co *next_coroutine() {
  for (struct node_* node = head; node != NULL; node=node->next)
  {
    if(node->co == current) {
      if (node != tail)
        return node->next->co;
      else
        return head->co;
    }
  }
  assert(0 && "Should not be here.");
  return NULL;
}

static bool first_time_co_create = true;

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  if (first_time_co_create) {
    first_time_co_create = false;
    current = malloc(sizeof(struct co));
    current->status = CO_RUNNING;
    current->name = malloc(100);
    strncpy(current->name, "master coroutine", 100);

    head = tail = malloc(sizeof(struct node_));
    head->co = current;
    head->next = head->prev = NULL;
  }


  struct co* new_co = malloc(sizeof(struct co));
  new_co->func = func;
  new_co->arg = arg;
  new_co->status = CO_NEW;
  new_co->parent= NULL;
  new_co->name = malloc(100);
  strncpy(new_co->name, name, 99);


  // Insert new_co to coroutine list
  struct node_* new_node = malloc(sizeof(struct node_));
  new_node->co = new_co;
  new_node->next = new_node->prev = NULL;
  if (tail) {
    new_node->prev = tail;
    tail->next = new_node;
    tail = new_node;
  } else {
    head = tail = new_node;
  }

  return new_co;
}

void co_wait(struct co *co) {
    current->status = CO_WAITING;
    assert(co->parent== NULL && "Multiple waits on same coroutines");
    co->parent= current;

    while(1) {
      if (co->status == CO_DEAD)
        break;
      else
        co_yield();
    }


    for (struct node_* node = head; node != NULL; node=node->next)
    {
      if(node->co == co) {
        if (node == head && node == tail)
          assert(0 && "There is only one coroutine left and we are goint to delete it");
        if (node == head){
          node->next->prev = NULL;
          head = node->next;
        } else
          node->prev->next = node->next;

        if (node == tail) {
          node->prev->next = NULL;
          tail = node->prev;
        } else
          node->next->prev = node->prev;
        free(node);
        break;
      }
    }
    free(co->name);
    free(co);
    current->status = CO_RUNNING;
}

void co_yield() {
  int val = setjmp(current->context);
  if (val == 0) {
    while(1) {
      // switch to another coroutine
      current = next_coroutine();
      if (current->status == CO_NEW) {
        current->status = CO_RUNNING;
        // stack is growing from upper to lower
        stack_switch_call(&current->stack[STACK_SIZE], current->func, (uintptr_t)current->arg);
      } else
        longjmp(current->context, 1);
    }
  } else {
    // from longjmp
    return;
  }
}
