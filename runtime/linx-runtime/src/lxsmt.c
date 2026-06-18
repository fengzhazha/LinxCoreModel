#include <assert.h>
#include <string.h>

#include "lxsmt.h"
#include "lxatomic.h"
#include "lxgqm.h"

typedef enum {
  CORE_UNWORKABLE = 0,
  CORE_IDLE = 1,
  CORE_BUSY = 1,
} core_state;

typedef struct {
  int core_states[LXSMT_MAX_CORE_NUM];   // recording core state
  gqm_t idle_queue;                            // a queue recording idle cores
  gqm_t core_msgqs[LXSMT_MAX_CORE_NUM];       // message queue for each core
  thread_t thread_pool[LXSMT_MAX_CORE_NUM];
  arg_t arg_pool[LXSMT_MAX_CORE_NUM];
} smt_ctx;

typedef struct {
  thread_t *tidp;
} core_msg;

THREAD_SHARE smt_ctx _ctx;

lock_t _init_region = {
  .flag = 1
};

int lxsmt_get_coreid(void) {
#ifdef __linx
#ifdef LINXV4
  int core_id;
  __asm__ __volatile__ (
    "C.BSTART\n"
    "ssrget 0x21, -> %0\n"
    : "=r"(core_id)
  );
  return core_id;
#else // LINX6188
  return SRE_GetCoreID();
#endif
#else // __linx
  assert(0 && "Not supported in general architecture!");
  return 0;
#endif
}

void lxsmt_init(void) {
  int coreId = lxsmt_get_coreid();
  int retval = gqm_init(&_ctx.core_msgqs[coreId], 128);
  _ctx.core_states[coreId] = CORE_UNWORKABLE;
  if (retval != GQM_SUACCELSS) {
    assert(0 && "Message queue init failed!\n");
  }
  if (coreId == 0) {
    gqm_init(&_ctx.idle_queue, 64);
    for (int i = 1; i < LXSMT_MAX_CORE_NUM; i++) {
      gqm_push(&_ctx.idle_queue, (item_t)i, GQM_AWAKEN);
    }
  }
  return;
}

void lxsmt_destroy(void) {
  int coreId = lxsmt_get_coreid();
  gqm_t *cmsgq = &_ctx.core_msgqs[coreId];

  // tell all slave threads to stop waiting for new messages
  if (coreId == 0) {
    for (int i = 1; i < LXSMT_MAX_CORE_NUM; i++) {
      int retval = gqm_push(cmsgq, (item_t)NULL, GQM_AWAKEN);
      if (retval != GQM_SUACCELSS) {
          assert(0 && "Sending exit message to failed!\n");
      }
    }
    // destroy the idle queue
    gqm_destroy(&_ctx.idle_queue);
  }

  // destroy the message queue
  gqm_destroy(cmsgq);
  return;
}

void lxsmt_wait_for_kernel(void) {
  int coreId = lxsmt_get_coreid();
  gqm_t *cmsgq = &_ctx.core_msgqs[coreId];
  _ctx.core_states[coreId] = CORE_IDLE;

  thread_t* tidp = NULL;
  while (1) {
    int retval = gqm_pop(cmsgq, (item_t*)tidp, GQM_NO_CTRL);
    if (retval != GQM_SUACCELSS)
      continue;
    _ctx.core_states[coreId] = CORE_BUSY;

    if (tidp == NULL) {
      _ctx.core_states[coreId] = CORE_UNWORKABLE;
      break;
    }

    tidp->state = THREAD_RUNNING;
    tidp->kernel(tidp->args);
    tidp->state = THREAD_TERMINATED;
    gqm_push(&_ctx.idle_queue, (item_t)coreId, GQM_AWAKEN);

    _ctx.core_states[coreId] = CORE_IDLE;
  }
  return;
}

// not thread safe, should only be called from the main thread
int lxsmt_create(kernel_t kernel, arg_t *args, thread_t **tidp) {
  uint64_t idle_core;
  int retval = gqm_pop(&_ctx.idle_queue, &idle_core, GQM_NO_CTRL);
  if (retval != GQM_SUACCELSS) {
    return LXSMT_ERR_NOIDLE;
  }
  
  *tidp = &_ctx.thread_pool[idle_core];
  memcpy(&_ctx.arg_pool[idle_core], args, sizeof(arg_t));

  (*tidp)->kernel = kernel;
  (*tidp)->args = &_ctx.arg_pool[idle_core];
  (*tidp)->state = THREAD_NEW;

  retval = gqm_push(&_ctx.core_msgqs[idle_core], (item_t)(*tidp), GQM_AWAKEN);
  if (retval != GQM_SUACCELSS) {
    return LXSMT_ERR;
  }
  (*tidp)->binding = idle_core;
  return LXSMT_OK;
}

void lxsmt_join(thread_t *tidp) {
  while (tidp->state != THREAD_TERMINATED)
    ;
  return;
}

void lxsmt_sync(__volatile__ lock_t *lock) {
  while (lock->flag == 1)
    ;
}