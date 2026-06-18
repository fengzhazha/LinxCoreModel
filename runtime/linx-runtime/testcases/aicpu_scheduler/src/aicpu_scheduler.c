#ifndef GENERIC_V200_HH
#define GENERIC_V200_HH

#ifndef __linx
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#endif

#include <stdint.h>
#include <string.h>
#include "common.h"
#include "lxsmt.h"
#include "lxatomic.h"
#include "lxgqm.h"
#include "lxdbg.h"

// #define MILESTONE_NO_WHILE
#define MILESTONE_PROFILING

#define TASK_COST 7500

#define NUM_AICORES AI_CORES
#define MAX_CORE_NUM RUN_CORES
#define MAX_NUM_CORE_PER_AICPU 64

#ifdef __linx
#define DLOG(fmt, ...)
#define ELOG(...)
#ifdef LINX6188
#define SRE_LOGI(fmt, ...) SRE_printf(fmt __VA_OPT__(,) __VA_ARGS__)
// #define SRE_LOGD(fmt, ...) SRE_printf(fmt __VA_OPT__(,) __VA_ARGS__)
#define SRE_LOGD(fmt, ...)
#else // LINXV4
#define SRE_LOGI(fmt, ...)
#define SRE_LOGD(fmt, ...)
#endif
#else // not define __linx
#define DLOG(fmt, ...)
#define ELOG(...)
#define SRE_LOGD(fmt, ...)
// #define SRE_LOGI(fmt, ...)
// #define DLOG(fmt, ...) printf("<tid %d> " fmt "\n" , aicpuIdx __VA_OPT__(,) __VA_ARGS__)
// #define ELOG(...) fprintf(stderr, __VA_ARGS__)
// #define SRE_LOGD(fmt, ...) printf(fmt __VA_OPT__(,)  __VA_ARGS__)
#define SRE_LOGI(fmt, ...) printf(fmt __VA_OPT__(,)  __VA_ARGS__)
#endif

#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)

// #include "llama_layer_4x2048x1x128.cpp"
// #include "llama_layer_1x1024x1x128.cpp"
#include "llama.h"
//#include "csr_test.h"

const int NODE_LIST_SIZE = sizeof(_node) / sizeof(_node[0]);
const int OFST_LIST_SIZE = sizeof(_offset) / sizeof(_offset[0]);
const int FREE_LIST_SIZE = sizeof(_free_list) / sizeof(_free_list[0]);
const int PRED_LIST_SIZE = sizeof(_pred) / sizeof(_pred[0]);

typedef struct {
  uint64_t process_ns;
  uint64_t pre_bubble_ns;
  uint64_t ready_ns;
  uint64_t task_id;
} Task;

#define N_TASK 16
typedef struct {
  int max_tasks;
  int issue_lat_ns;
  int resp_lat_ns;
  uint64_t last_ready_ns;

  Task tasks[N_TASK];
  int head;
  int tail;

} FakeAICore;

void aicore_init(FakeAICore* core, int max_tasks, int issue_lat_ns, int resp_lat_ns) {
  core->max_tasks = max_tasks;
  core->issue_lat_ns = issue_lat_ns;
  core->resp_lat_ns = resp_lat_ns;
  core->last_ready_ns = 0;
  core->head = 0;
  core->tail = 0;
}

void aicore_push(FakeAICore* core, uint64_t task_id, uint64_t current_ns, int task_ns) {
  uint64_t start_ns = MAX(core->last_ready_ns, current_ns + core->issue_lat_ns);
  uint64_t ready_ns = start_ns + task_ns;
  uint64_t collect_ns = ready_ns + core->resp_lat_ns;
  core->tasks[core->tail].process_ns = ready_ns - start_ns;
  if (core->last_ready_ns == 0) {
      core->tasks[core->tail].pre_bubble_ns = 0;
  } else {
      core->tasks[core->tail].pre_bubble_ns = start_ns - core->last_ready_ns;
  }
  core->tasks[core->tail].ready_ns = collect_ns;
  core->tasks[core->tail].task_id = task_id;
  core->tail = (core->tail + 1) % N_TASK;
  core->last_ready_ns = ready_ns;
}

size_t aicore_size(FakeAICore* core) {
  return core->tail >= core->head ? (core->tail - core->head) : ((core->tail + N_TASK) - core->head);
}

bool aicore_canPush(FakeAICore* core) { return aicore_size(core) < (size_t)core->max_tasks; }

bool aicore_canPop(FakeAICore* core, uint64_t current_ns) {
  if (aicore_size(core) == 0)
    return false;
  if (core->tasks[core->head].ready_ns > current_ns) {
    return false;
  }
  return true;
}

uint64_t aicore_nextReadyNs(FakeAICore* core) { return core->tasks[core->head].ready_ns; }

uint64_t aicore_pop(FakeAICore* core) {
  uint64_t ret = core->tasks[core->head].task_id;
  core->head = (core->head + 1) % N_TASK;
  return ret;
}

#define MAX_LOCAL_QUEUE_SIZE 512
typedef struct {
  uint32_t item[MAX_LOCAL_QUEUE_SIZE];
  size_t size;
  int head;
  int tail;
} local_queue_t;

void lq_init(local_queue_t *q) {
  q->head = 0;
  q->tail = 0;
  q->size = 0;
}

void lq_push(local_queue_t *q, uint32_t data) {
  q->item[q->tail] = data;
  q->tail = (q->tail + 1) % MAX_LOCAL_QUEUE_SIZE;
  q->size++;
}

int lq_pop(local_queue_t *q, uint32_t *data) {
  if (q->size == 0)
    return -1;
  int old_head = q->head;
  q->head = (q->head + 1) % MAX_LOCAL_QUEUE_SIZE;
  *data = q->item[old_head];
  q->size --;
  return 0;
}

size_t lq_size(local_queue_t* q) {
  return q->size;
}

THREAD_SHARE gqm_t global_ready_queue;

void initialize_global_ready_queue(void) {
  gqm_init(&global_ready_queue, 1023);
  for (int i = 0; i < FREE_LIST_SIZE; ++i) {
    gqm_push(&global_ready_queue, _free_list[i], GQM_NO_CTRL);
    DUMP_VAR(0, _free_list[i]);
  }
}

THREAD_SHARE char CanPopSignal[MAX_CORE_NUM] = {0};

int ResolveFakeDep(gqm_t *gqm, int subgraphId) {
  for (int i = _node[subgraphId]; i < _node[subgraphId+1]; i++) {
    int pred_num = atomic_load_add_4(&_pred[_offset[i]], -1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);
    if (pred_num == 1) {
      gqm_push(gqm, _offset[i], GQM_AWAKEN);
      memset(&CanPopSignal, 0, sizeof(CanPopSignal));
      DUMP_VAR(0, _offset[i]);
    }
  }
  return _node[subgraphId+1] - _node[subgraphId];
}

#define MEASURE_TIME

// Global finished tasks.
THREAD_SHARE int64_t finished_tasks = 0;

typedef struct {
  uint64_t ns;
  uint64_t start_ns;
  uint64_t end_ns;
  int loop_iter;
  int issued;
  int empty_issued;
  int failed_issued;
  int empty_failed_issued;
  int cleared_dep;
  int push;
  int pop;
  int cleared_task;
  uint64_t total_AICORE_ns;
  uint64_t total_pre_bubble_ns;
  uint64_t total_post_bubble_ns;
  uint64_t total_fakedep_ns;
  uint64_t total_dispatch_ns;
  uint64_t total_resolve_ns;
  uint64_t total_process_ns;
} SimResult;

THREAD_SHARE SimResult sim_results[MAX_CORE_NUM];

typedef struct {
  int AICores;
  int AICPUs;
  int max_infly_task_per_aicore;
  int task_distribute_ns;
  int task_collect_ns;
  SimResult results[MAX_CORE_NUM];
} Config;

inline void hint1(void) {
  __asm__ __volatile__ (
                  "bstart 1f\n"
                  "b.stdlp\n"
                  "bnext.fall\n"
                  "bstop 2f\n"
                  ".pushSection .text.body\n"
                  "1:\n"
                  "ori zero, 1\n"
                  "2:\n"
                  ".popSection\n"
                  );
}

inline void hint2(void) {
  __asm__ __volatile__ (
                  "bstart 1f\n"
                  "b.stdlp\n"
                  "bnext.fall\n"
                  "bstop 2f\n"
                  ".pushSection .text.body\n"
                  "1:\n"
                  "ori zero, 2\n"
                  "2:\n"
                  ".popSection\n"
                  );
}

inline void hint3(void) {
  __asm__ __volatile__ (
                  "bstart 1f\n"
                  "b.stdlp\n"
                  "bnext.fall\n"
                  "bstop 2f\n"
                  ".pushSection .text.body\n"
                  "1:\n"
                  "ori zero, 3\n"
                  "2:\n"
                  ".popSection\n"
                  );
}

inline void hint4(void) {
  __asm__ __volatile__ (
                  "bstart 1f\n"
                  "b.stdlp\n"
                  "bnext.fall\n"
                  "bstop 2f\n"
                  ".pushSection .text.body\n"
                  "1:\n"
                  "ori zero, 4\n"
                  "2:\n"
                  ".popSection\n"
                  );
}

uint64_t get_cycle(void) {
#ifdef __linx
#ifdef LINXV4
  uint64_t cycle;
  __asm__ __volatile__ (
    "C.BSTART\n"
    "ssrget 0x11, -> %0\n"
    : "=r"(cycle)
  );
  return cycle;
#else // LINX6188
  return (U64)__builtin_linx_get_system_reg(SSR_CYCLE_CNT);
#endif
#else // undefine __linx
  return clock();
#endif
}

uint64_t TestAICoreSubgraphTopoParallelizeFakeAICore(int aicpuIdx,
                                                     Config *config,
                                                     int num_core_per_aicpu) {

  // Finished queue is private to myself.
  // std::deque<int> finished_queue;
  SRE_LOGD("Start work\n");
  assert(num_core_per_aicpu <= MAX_NUM_CORE_PER_AICPU && "Too much core per aicpu!");
  FakeAICore cores[MAX_NUM_CORE_PER_AICPU];
  for (int i = 0; i < num_core_per_aicpu; ++i) {
    aicore_init(&cores[i], config->max_infly_task_per_aicore,
                config->task_distribute_ns, config->task_collect_ns);
  }

  // Initialize the ready core deque.
  local_queue_t ready_cores;
  lq_init(&ready_cores);
  for (int i = 0; i < num_core_per_aicpu * config->max_infly_task_per_aicore;
       ++i) {
    lq_push(&ready_cores, i % num_core_per_aicpu);
  }
  for (int i = 0; i < num_core_per_aicpu * config->max_infly_task_per_aicore;
       ++i) {
    lq_push(&ready_cores, i % num_core_per_aicpu);
  }

  uint64_t total_loop = 0;

  int64_t total_tasks = _node_num;

  uint64_t t_balance, t_enter, t_issue, t_dispatch, t_resolve,
      t_complete;
  uint64_t total_AICORE_ns = 0;
  uint64_t total_pre_bubble_ns = 0;
  uint64_t total_post_bubble_ns = 0;
  uint64_t total_fakedep_ns = 0;
  uint64_t total_dispatch_ns = 0;
  uint64_t total_resolve_ns = 0;
  uint64_t total_process_ns = 0;
  uint64_t current_ns = 0;

  // int global_ready_queue_pull = 0;
  int global_ready_queue_pull_tasks = 0;
  // int global_ready_queue_push = 0;
  int global_ready_queue_push_tasks = 0;
  int issued = 0;
  int failed_issued = 0;
  int empty_issued = 0;
  int empty_failed_issued = 0;
  int cleared_dep = 0;
  int cleared_task = 0;

  SRE_LOGD("Start aicore=%d aicpu=%d aicore/aicpu=%d\n", config->AICores,
       config->AICPUs, num_core_per_aicpu);

  // const int numAICPUs = config->AICPUs;

  uint64_t t_start = get_cycle();
  t_complete = t_start;
#ifndef MILESTONE_NO_WHILE
  while (finished_tasks < total_tasks) {
    total_loop++;

#ifdef MEASURE_TIME
    t_issue = get_cycle();
    current_ns = t_issue;


#else
    current_ns = get_cycle();
#endif
    hint1();   //wake up
    uint32_t dispatch_start = get_cycle();
    while (lq_size(&ready_cores) != 0) {
      uint32_t coreId;
      uint64_t taskId;
      if (CanPopSignal[aicpuIdx] == 0) {
        int retval2 = gqm_pop(&global_ready_queue, &taskId, GQM_NO_CTRL);
        if (retval2 != GQM_SUACCELSS) {
          CanPopSignal[aicpuIdx] = 1;
          failed_issued++;
          if (aicore_size(&cores[coreId]) == 0) {
              empty_failed_issued++;
          }
          break;
        }
      } else {
        break;
      }
      int retval1 = lq_pop(&ready_cores, &coreId);
      if (aicore_size(&cores[coreId]) == 0) {
          empty_issued++;
      }
      aicore_push(&cores[coreId], taskId, current_ns, TASK_COST);
      DUMP_VAR(1, taskId);
      issued++;
      hint2(); //issue task
    }
    uint32_t dispatch_end = get_cycle();
    total_dispatch_ns += dispatch_end - dispatch_start;

#ifdef MEASURE_TIME
    t_dispatch = get_cycle();
    current_ns = t_dispatch;
#else
    current_ns = get_cycle();
#endif

    // Parallelize the finish_queue read.
    // t_resolve = get_cycle();
    hint3();
    int local_finished_tasks = 0;
    uint64_t resolve_start = get_cycle();
    for (int i = 0; i < num_core_per_aicpu; ++i) {
      if (aicore_canPop(&cores[i], current_ns)) {
        total_AICORE_ns += cores[i].tasks[cores[i].head].process_ns;
        total_pre_bubble_ns += cores[i].tasks[cores[i].head].pre_bubble_ns;
        uint64_t cur_ready_ns = cores[i].tasks[cores[i].head].ready_ns;
        int taskId = aicore_pop(&cores[i]);
        DUMP_VAR(2, taskId);
        // finished_queue.push_back(cores[i].pop());
        uint64_t fakedep_start = get_cycle();
        cleared_dep += ResolveFakeDep(&global_ready_queue, taskId);
        uint64_t fakedep_end = get_cycle();
        lq_push(&ready_cores, i);
        local_finished_tasks++;
        uint64_t b_resolve_end = get_cycle();
	total_post_bubble_ns += b_resolve_end - cur_ready_ns;
        total_fakedep_ns += fakedep_end - fakedep_start;
      }
    }
    uint64_t resolve_end = get_cycle();
    total_resolve_ns += resolve_end - resolve_start;
    if (local_finished_tasks) {
      cleared_task += local_finished_tasks;
      atomic_load_add_8(&finished_tasks, local_finished_tasks, 0);
    }


#ifdef MEASURE_TIME
    t_complete = get_cycle();
    total_process_ns += t_complete - t_issue;
    hint4();
    // total_fakedep_ns += t_enter - t_balance;
    // total_dispatch_ns += t_dispatch - t_issue;
    // total_resolve_ns += t_complete - t_dispatch;
    // total_complete_ns += t_complete - t_resolve;
#endif
  }
#endif

  uint64_t t_end = get_cycle();

  uint64_t total_ns = t_end - t_start;
  sim_results[aicpuIdx].start_ns = t_start;
  sim_results[aicpuIdx].end_ns = t_end;
  sim_results[aicpuIdx].ns = total_ns;
  sim_results[aicpuIdx].loop_iter = total_loop;
  sim_results[aicpuIdx].issued = issued;    // pop num
  sim_results[aicpuIdx].failed_issued = failed_issued;    // pop num
  sim_results[aicpuIdx].empty_issued = empty_issued;    // pop num
  sim_results[aicpuIdx].empty_failed_issued = empty_failed_issued;
  sim_results[aicpuIdx].cleared_dep = cleared_dep;
  // sim_results[aicpuIdx].push = global_ready_queue_push_tasks;
  // sim_results[aicpuIdx].pop = global_ready_queue_pull_tasks;
  sim_results[aicpuIdx].cleared_task = cleared_task; // push num
  sim_results[aicpuIdx].total_AICORE_ns = total_AICORE_ns/num_core_per_aicpu;
  sim_results[aicpuIdx].total_pre_bubble_ns = total_pre_bubble_ns/num_core_per_aicpu;
  sim_results[aicpuIdx].total_post_bubble_ns = total_post_bubble_ns/num_core_per_aicpu;
  sim_results[aicpuIdx].total_fakedep_ns = total_fakedep_ns;
  sim_results[aicpuIdx].total_dispatch_ns = total_dispatch_ns;
  sim_results[aicpuIdx].total_resolve_ns = total_resolve_ns;
  sim_results[aicpuIdx].total_process_ns = total_process_ns;

  // DLOG("what???");

  return total_loop;
}

THREAD_SHARE int original_pred[sizeof(_pred) / sizeof(_pred[0])];
// 0 initial value
// 1 original copy saved.
// 2 new experiment set -- can run
// 3 experiment done
const int global_experiment_state_init = 0;
const int global_experiment_state_setup = 1;
const int global_experiment_state_start = 2;
THREAD_SHARE volatile uint32_t global_experiment_state = global_experiment_state_init;
THREAD_SHARE static const int lead_aicpu_idx = 0;
THREAD_SHARE volatile uint64_t sync_cores = 0;

static const int num_running_threads = MAX_CORE_NUM;

void runGenericAICPUExperiment(int aicpuIdx, Config *config) {
  DLOG("runGenericAICPUExperiment Start for thread %d", aicpuIdx);
  // Reset the in_degrees and ready_queue
  if (aicpuIdx == lead_aicpu_idx) {
    // memcpy(_pred, original_pred, sizeof(_pred));
    // memset(sim_results, 0, sizeof(SimResult) * config->AICPUs);
    initialize_global_ready_queue();
    SRE_LOGD("Global ready queue init done!\n");
    // while (!compare_and_swap_8((uint64_t*)&sync_cores, num_running_threads, 0))
    //   ;
    while (!compare_and_swap_4(
      (volatile uint32_t*)&global_experiment_state,
      global_experiment_state_init,
      global_experiment_state_start)) {
    }

    DLOG("Reset in_degrees and ready_queue %lu\n", gqm_size(&global_ready_queue));
  } else {
    SRE_LOGD("Waiting for sync\n");
    while(global_experiment_state != global_experiment_state_start)
    ;
    // while (!compare_and_swap_4(
    //   (uint32_t*)&global_experiment_state,
    //   global_experiment_state_start,
    //   global_experiment_state_start)) {
    // }
  }

  SRE_LOGD("Sync point before running!\n");

  // Start running.
  if (aicpuIdx >= lead_aicpu_idx && aicpuIdx < config->AICPUs + lead_aicpu_idx) {

    // Assume no remainder.
    int num_aicore_per_aicpu = config->AICores / config->AICPUs;

    // Get the current time before the operation
    TestAICoreSubgraphTopoParallelizeFakeAICore(
      aicpuIdx, config, num_aicore_per_aicpu);
  }

  atomic_load_add_8((int64_t*)&sync_cores, 1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);
  SRE_LOGD("Increased sync_cores: %d\n", sync_cores);
  if (aicpuIdx == lead_aicpu_idx) {
    // Spin until every core is finished.
    DLOG("Waiting for all threads finsih - Start");
    while (atomic_load_add_8((int64_t*)&sync_cores, 0, ATOMIC_ACQUIRE | ATOMIC_RELEASE) != num_running_threads) {
      ;
    }
    DLOG("Waiting for all threads finsih - End");

#ifdef MILESTONE_PROFILING
    // Remember the result.
    for (int idx = lead_aicpu_idx; idx < config->AICPUs + lead_aicpu_idx; ++idx) {
      memcpy(&config->results[idx], &sim_results[idx], sizeof(SimResult));
    }
    DLOG("Write results done!");
#endif

    // Clear the counter.
    finished_tasks = 0;
    sync_cores = 0;
    // Set the state back to setup state so that every one can move forward.
    while (!compare_and_swap_4(
        (uint32_t*)&global_experiment_state,
        global_experiment_state_start,
        global_experiment_state_setup)) {
    }
  } else {
    DLOG("Waiting for lead cpu - Start");
    while(global_experiment_state != global_experiment_state_setup)
      ;
    // while (!compare_and_swap_4(
    //     (uint32_t*)&global_experiment_state,
    //     global_experiment_state_setup,
    //     global_experiment_state_setup)) {
    // }
    DLOG("Waiting for lead cpu - End");
  }

  SRE_LOGD("Sync point after running!\n");

  // atomic_load_add_8((int64_t*)&sync_cores, 1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);

  if (aicpuIdx == lead_aicpu_idx) {
    gqm_destroy(&global_ready_queue);
  }
}

void runGenericAICPU(int aicpuIdx) {
  
  int cpuId = aicpuIdx;
  DLOG("Hello from old thread cpuId=%d", cpuId);

  // backup the predecessor info
  if (cpuId == lead_aicpu_idx) {
    SRE_LOGI("NumCPU:%d,NumAIcore:%d,NumNode:%d\n", MAX_CORE_NUM, NUM_AICORES, _node_num);
    // memcpy(original_pred, _pred, sizeof(_pred));
    // while (!compare_and_swap_4(
    //     (uint32_t*)&global_experiment_state,
    //     global_experiment_state_init,
    //     global_experiment_state_setup)) {
    // }
    DLOG("Saved original in_degrees and ready_queue");
  } else {
    // while (!compare_and_swap_4(
    //     (uint32_t*)&global_experiment_state,
    //     global_experiment_state_setup,
    //     global_experiment_state_setup)) {
    // }
    DLOG("Set up experiment environment");
  }

  // sync barrier

  // atomic_load_add_8((int64_t*)&sync_cores, 1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);

  // int num_aicpus[] = {1, 2, 4};
  int num_aicpus[] = {MAX_CORE_NUM};
  // int max_infly_task[] = {1, 2, 4};
  int max_infly_task[] = {2};
  // int task_comm_lats[] = {20, 40, 80, 160, 320};
  int task_comm_lats[] = {30};
  int num_aicores[] = {NUM_AICORES};
  Config all_configs[1];
  int cfgNum = 0;
  for (int i = 0; i < (int)(sizeof(max_infly_task) / sizeof(int)); i++) {
    int infly_task = max_infly_task[i];
    for (int j = 0; j < (int)(sizeof(task_comm_lats) / sizeof(int)); j++) {
      int task_comm_lat = task_comm_lats[j];
      for (int k = 0; k < (int)(sizeof(num_aicpus) / sizeof(int)); k++) {
        int aicpus = num_aicpus[k];
        for (int m = 0; m < (int)(sizeof(num_aicores) / sizeof(int)); m++) {
          int aicores = num_aicores[m];
          if (aicpuIdx == lead_aicpu_idx)
            SRE_LOGD("Testing with config: %d, %d, %d, %d\n", infly_task, task_comm_lat, aicpus, aicores);
          Config *config = &all_configs[cfgNum++];
          config->AICores = aicores;
          config->AICPUs = MIN(aicpus, aicores);
          config->max_infly_task_per_aicore = infly_task;
          config->task_collect_ns = task_comm_lat;
          config->task_distribute_ns = task_comm_lat;
          runGenericAICPUExperiment(cpuId, config);
          // if (cpuId == lead_aicpu_idx) {
          //   all_configs.push_back(config);
          // }
        }
      }
    }
  }

  // Write the result.
#ifdef MILESTONE_PROFILING
  if (cpuId == lead_aicpu_idx) {
    SRE_LOGI("Sim Results:\n");
    for (int i = 0; i < cfgNum; i++) {
      Config config = all_configs[i];
      SRE_LOGI("Config: %d, %d, %d, %d\n", config.AICPUs, config.AICores, config.task_collect_ns, config.max_infly_task_per_aicore);
      for (int j = 0; j < config.AICPUs; j++) {
        SimResult result = config.results[j];
        //SRE_LOGI("Core %d: %ld, %ld, %d, %d, %d, %d, %ld, %ld, %ld, %ld\n", j, 
        //result.start_ns, result.end_ns, result.loop_iter, result.issued, result.cleared_dep, result.cleared_task, 
        //result.total_dispatch_ns, result.total_resolve_ns, result.total_process_ns, result.total_fakedep_ns);
        //float usage = (float)result.total_AICORE_ns/(result.end_ns - result.start_ns);
        //SRE_LOGI("Core %d:\n", j);
        //SRE_LOGI("----AICore Utilization: %.5f:\n", usage);
        //SRE_LOGI("---------AICORE Total time: %ld\n", result.total_AICORE_ns);
        //SRE_LOGI("---------AICORE Pre-bubble Total time: %ld\n", result.total_pre_bubble_ns);
        //SRE_LOGI("---------AICORE Post-bubble Total time: %ld\n", result.total_post_bubble_ns);
        //SRE_LOGI("----AICPU Stage:\n");
        //SRE_LOGI("---------Total time: %ld\n", result.end_ns - result.start_ns);
        //SRE_LOGI("---------Process time: %ld\n", result.total_process_ns);
        //SRE_LOGI("---------Resolve time: %ld\n", result.total_resolve_ns);
        //SRE_LOGI("---------Dispatch time: %ld\n", result.total_dispatch_ns);
        //SRE_LOGI("----Task Status:\n");
        //SRE_LOGI("---------Loop Iteration: %d\n", result.loop_iter);
        //SRE_LOGI("---------Suaccelss Issued: %d\n", result.issued);
        //SRE_LOGI("-----------Empty Suaccelss Issued: %d\n", result.empty_issued);
        //SRE_LOGI("---------Failed Issued: %d\n", result.failed_issued);
        //SRE_LOGI("-----------Empty Failed Issued: %d\n", result.empty_failed_issued);
        //SRE_LOGI("---------Cleared Task: %d\n", result.cleared_task);
        SRE_LOGI("Core %d: %ld, %ld, %ld, %ld, %ld, %ld, %d, %d, %d, %d, %d, %d \n", j, result.start_ns, result.end_ns,
               result.total_AICORE_ns, result.total_process_ns, result.total_resolve_ns, result.total_dispatch_ns,
               result.loop_iter, result.issued, result.empty_issued, result.failed_issued, result.empty_failed_issued,
               result.cleared_task);
      }
      SRE_LOGI("\n");
    }
  }
#endif
}

void *runGenericAICPU_thread(void *arg) {
  int aicpuIdx = *(int*)arg;
  runGenericAICPU(aicpuIdx);
  return NULL;
}

#ifdef LINX6188
int Linx_main(void) {
#else
int main(void) {
#endif

#ifdef __linx
#ifdef LINXV4
  __asm__ __volatile__ (
    "C.BSTART\n"
    "ssrget 0x21, -> t\n"
    "slli t#1, 23, -> t\n"
    "add sp, t#1, -> sp\n"
  );

  int cpuId = lxsmt_get_coreid();
  runGenericAICPU(cpuId);
  return 0;
#else // LINX6188
  int cpuId = lxsmt_get_coreid();
  runGenericAICPU(cpuId);
  SRE_LOGI("<tid %d> Done!\n", cpuId);
  return 0;
#endif
#else
  // runGenericAICPU(0);
  pthread_t thread_pool[MAX_CORE_NUM];
  int arg_pool[MAX_CORE_NUM];

  for (int i = 0; i < MAX_CORE_NUM; i++) {
    arg_pool[i] = i;
    pthread_create(&thread_pool[i], NULL, runGenericAICPU_thread, (void*)&arg_pool[i]);
  }

  for (int i = 0; i < MAX_CORE_NUM; i++) {
    pthread_join(thread_pool[i], NULL);
  }

  return 0;
#endif
}
#endif
