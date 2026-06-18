#ifndef __LINDART_LXSMT_H
#define __LINDART_LXSMT_H

#include <stdint.h>
#include <assert.h>
#include "lxatomic.h"

#define LXSMT_OK 0
#define LXSMT_ERR 1
#define LXSMT_ERR_NOIDLE 2

#define LXSMT_MAX_ARG_SIZE 64
#define LXSMT_MAX_CORE_NUM 8

#define INIT_REGION_START() \
  int _coreId = lxsmt_get_coreid();  \
  if (_coreId != 0) {             \
      lxsmt_sync(&_init_region);    \
  } else {

#define INIT_REGION_END() unlock(&_init_region); \
  }

extern lock_t _init_region;

typedef int (*kernel_t)(void *args);

typedef enum {
  THREAD_NEW = 0,
  THREAD_READY = 1,
  THREAD_RUNNING = 2,
  THREAD_BLOCKED = 3,
  THREAD_TERMINATED = 4
} thread_state;

typedef struct {
  thread_state state;
  int binding;
  kernel_t kernel;
  void* args;
} thread_t;

typedef struct {
  uint8_t arg[LXSMT_MAX_ARG_SIZE];
} arg_t;

void lxsmt_init(void);
int  lxsmt_create(kernel_t kernel, arg_t *args, thread_t **tidp);
void lxsmt_join(thread_t *tidp);
void lxsmt_destroy(void);
void lxsmt_wait_for_kernel(void);

int lxsmt_get_coreid(void);

void lxsmt_sync(__volatile__ lock_t *lock);

#endif