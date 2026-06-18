#include <cassert>
#include <thread>
#include <unistd.h>
#include <assert.h>

#include "Runtime.h"
#include "Graph.h"

const int TASK_QUEUE_SIZE = 16384;

bool is_pow2(int n) {
    int sum = 0;
    while (n != 0) {
        sum += n & 0x1;
        n >>= 1;
    }
    return sum == 1;
}

Runtime::Runtime(int worker_num, int task_pool_size) {
    assert(is_pow2(task_pool_size) && "Task pool size must be power of 2.");
    this->worker_num = worker_num;
    this->task_pool_size = task_pool_size;
    this->task_pool = new Task[task_pool_size * worker_num];
    this->task_num  = new int[worker_num]();
    this->task_q = new Task*[TASK_QUEUE_SIZE];
    this->task_q_head = 0;
    this->task_q_tail = 0;
    this->worker_status[worker_num] = {IDLE};
#ifdef LINXV4
    this->gqm_q = new Task*[TASK_QUEUE_SIZE];
    int64 rval;
    __asm__ __volatile__ (
        "qmt.i %[gqm], %[qsize], -> t\n"
        "ori t#1, 0, -> %[rval]\n"
        : [rval]"=r"(rval)
        : [gqm]"r"(this->gqm_q), [qsize]"r"(TASK_QUEUE_SIZE)
        :
    );
    assert(((rval >> 62) & 0x3) == 0 && "GQM initialization failed!\n");
#endif
}

Runtime::~Runtime() {
    delete task_pool;
    delete task_num;
    delete task_q;
}

int Runtime::get_task_q_size() {
    return (task_q_tail >= task_q_head) ? task_q_tail - task_q_head : TASK_QUEUE_SIZE - (task_q_head - task_q_tail);
}

bool Runtime::are_workers_all_idle() {
    for(int i=0; i < 4; i++){
        // queue empty or not
        if(bcq[i].size_approx() || worker_status[i] == BUSY)
            return false;
    }
    return true;
}

Task* Runtime::new_task(int tpid, kernel_t k, GraphNode* node) {
    Task* task_slot = &task_pool[tpid * task_pool_size + task_num[tpid]];
    task_slot->kernel = k;
    task_slot->node = node;
    task_num[tpid] = (task_num[tpid] + 1) % task_pool_size;
    return task_slot;
}

void Runtime::push_task(void *t, unsigned qid) {
#ifndef LINXV4
    bcq[qid].enqueue(t);
#else
    assert(0);
    // int64 rval;
    // __asm__ __volatile__ (
    //     "qpush.e %[gqm], %[t], -> t\n"
    //     "ori t#1, 0, -> %[rval]\n"
    //     : [rval]"=r"(rval)
    //     : [gqm]"r"(this->gqm_q), [t]"r"(task)
    //     :
    // );
#endif
}

void *Runtime::pop_task(unsigned qid) {
#ifndef LINXV4
    while (1) {
        void* t;
        bool found = bcq[qid].try_dequeue(t);
        if (found){
            worker_status[qid] = BUSY;
            return t;
        }
        worker_status[qid] = IDLE;
        if (are_workers_all_idle())
            // spin until all workers are idle
            return nullptr;
    }
#else
    assert(0);
    // int64 rval;
    // Task *task;
    // __asm__ __volatile__ (
    //     "qpop.e %[gqm], -> tx2\n"
    //     "ori t#1, 0, -> %[rval]\n"
    //     "ori t#2, 0, -> %[t]\n"
    //     : [rval]"=r"(rval), [t]"=r"(task)
    //     : [gqm]"r"(this->gqm_q)
    //     :
    // );
    // if (((rval >> 62) & 0x3) == 1) {
    //     return nullptr;
    // } else {
    //     return task;
    // }
#endif
}

void worker0(Runtime& rt, int tpid) {
    while (1) {
        Task* cur_task = static_cast<Task*>(rt.pop_task(0));
        if (cur_task == nullptr)
            break;

        GraphNode* gn = cur_task->node;
        rt.push_task(gn, 1);
       // std::cout <<  "Push Node " << gn->id << " to Queue 1" << std::endl;
    }
}

void worker1(Runtime& rt, int tpid) {
    while(1) {
        GraphNode* gn = static_cast<GraphNode*>(rt.pop_task(1));
        if (gn == nullptr) 
            break;
        for (int i = 0; i < gn->neighbor_num; i++) {
            GraphNode* cn = gn->neighbors[i];
            rt.push_task(cn, 2);
            // std::cout <<  "Push Node " << cn->id << " to Queue 2" << std::endl;
        }
    }
}

void worker2(Runtime& rt, int tpid) {
    while(1) {
        GraphNode* cn = static_cast<GraphNode*>(rt.pop_task(2));
        if (cn == nullptr)
            break;
        int64 pred_num = --cn->pred_num;
        if (pred_num == 0) {
            rt.push_task(cn, 3);
            // std::cout <<  "Push Node " << cn->id << " to Queue 3" << std::endl;
        }
        
    }
}

void worker3(Runtime& rt, int tpid) {
    while(1) {
        GraphNode* cn = static_cast<GraphNode*>(rt.pop_task(3));
        if (cn == nullptr)
            break;
        Task* nt = rt.new_task(tpid, nullptr, cn);
        rt.push_task(nt, 0);
        // std::cout <<  "Push Node " << cn->id << " to Queue 0" << std::endl;
    }
}

void Runtime::run_smt_multipred(int64 total_task_num) {
#ifndef LINXV4
    this->total_task_num = total_task_num;
    std::vector<std::thread> thread_pool;
    thread_pool.push_back(std::thread(worker0, std::ref(*this), 0));
    thread_pool.push_back(std::thread(worker1, std::ref(*this), 1));
    thread_pool.push_back(std::thread(worker2, std::ref(*this), 2));
    thread_pool.push_back(std::thread(worker3, std::ref(*this), 3));

    for (int i = 0; i < worker_num; i++)
        thread_pool[i].join();
#else 
    assert(0);
    // this->total_task_num = total_task_num;
    // int64 core_id;
    // __asm__ __volatile__ (
    //     "ssrget 0x21, -> %[cid]\n"
    //     : [cid]"=r"(core_id)
    //     :
    //     :
    // );
    // graph_bfs_thread(std::ref(*this), core_id);
#endif
}

