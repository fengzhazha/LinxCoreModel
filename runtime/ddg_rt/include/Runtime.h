#pragma once

#include <vector>
#include <stdint.h>
#include "Graph.h"
#include "concurrentqueue.h"

class Runtime;

typedef int (*kernel_t)(Runtime& rt, int tpid, GraphNode* node);
typedef long long int64;

enum STATUS {
  IDLE,
  BUSY
};

class Task {
public:
    GraphNode* node;
    kernel_t kernel;
};

class Runtime {
public:
    int worker_num;
    int task_pool_size;
    Task* task_pool;
    int* task_num;
    Task** task_q;
    STATUS worker_status[4];  // FIXME: Hardcoded 4 workers
#ifndef LINXV4
    std::atomic<int64> task_q_head;
    std::atomic<int64> task_q_tail;
    std::atomic<int64> total_task_num;
    moodycamel::ConcurrentQueue<void*> bcq[4]; // FIXME: Hardcoded 4 queues for workers
#else
    int64 task_q_head;
    int64 task_q_tail;
    int64 total_task_num;
    void* gqm_q;
#endif
    // for debug
    int max_queue_size;

    Runtime(int worker_num, int task_pool_size);
    ~Runtime();
    void push_task(void *task, unsigned qid);
    void *pop_task(unsigned qid);
    int get_task_q_size();
    void run_scalar();
    void run_smt_multipred(int64 total_task_num);
    bool are_workers_all_idle();
    // void run_simt_gqm(Task* t);
    // void run_simt_atomic(Task* t);
    // void run_simt_gqm_multipred(Task* t);
    // void run_simt_atomic_multipred(Task* t);
    Task* new_task(int tpid, kernel_t k, GraphNode* node);
};
