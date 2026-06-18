#include "graph_bfs.h"
#include "graph.h"
#include "lxsmt.h"
#include "lxgqm.h"
#include "lxatomic.h"

// graph data
#include "csr_test.h"

graph_t g;
gqm_t res_q;

typedef struct {
    int node_idx;
    char reserved[LXSMT_MAX_ARG_SIZE - 4];
} bfs_smt_args;

typedef struct {
    int node_idx;
    char reserved[4];
} bfs_smt_rval;

int bfs_smt_kernel(void *args) {
  bfs_smt_args *_args = (bfs_smt_args *)args;
  int node_idx = _args->node_idx;
  
  for (int i = g.node[node_idx]; i < g.node[node_idx + 1]; i++) {
    int pred = atomic_load_add_4(&g.pred[i], -1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);
    if (pred > 0)
        continue;
    item_t rval = g.offset[i];
    gqm_push(&res_q, rval, GQM_AWAKEN);
  }

  return 0;
}

void run_bfs_smt(void) {
  lxsmt_init();
  INIT_REGION_START();

  graph_init(&g, _node, _node_num, _offset, _edge_num, _pred);

  int retval = gqm_init(&res_q, 1024);
  if (retval != GQM_SUACCELSS) {
    lxsmt_destroy();
    return;
  }

  item_t rval = 1;
  gqm_push(&res_q, rval, GQM_AWAKEN);

  int processed = -1;
  do {
    int64_t pop_data;
    retval = gqm_pop(&res_q, (item_t*)&pop_data, GQM_NO_CTRL);
    if (retval != GQM_SUACCELSS)
      continue;
    processed++;

    if (pop_data == -1)
      continue;
    thread_t *tidp;
    bfs_smt_args args = {
      .node_idx = pop_data
    };
    int err = LXSMT_ERR_NOIDLE;
    while (err == LXSMT_ERR_NOIDLE) {
      err = lxsmt_create(bfs_smt_kernel, (arg_t *)&args, &tidp);
    }
  } while (processed < _node_num);

  INIT_REGION_END();
  lxsmt_destroy();
  return;
}


/*****************************
 *
 * Graph BFS with N workers
 *
******************************/

gqm_t task_q;
int total_node_num;

void nworker_start(void) {
  int next_node = 0;
  int cur_node  = 0;
  while (1) {
    // if (next_node == 0) {
      item_t pop_data;
      int retval = gqm_pop(&task_q, &pop_data, GQM_NO_CTRL);
      cur_node = retval == GQM_SUACCELSS ? pop_data : 0;
    // } else {
    //   cur_node = next_node;
    //   next_node = 0;
    // }
    if (cur_node != 0) {
      // usleep(gn->cost);
      // std::cout << "Node " << gn->id << " done!\n";
      for (int i = g.node[cur_node]; i < g.node[cur_node + 1]; i++) {
        int pred_num = atomic_load_add_4(&g.pred[g.offset[i]], -1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);
        if (pred_num <= 1) {
          // if (next_node == 0) {
          //   next_node = g.offset[i];
          // } else {
            gqm_push(&task_q, (item_t)g.offset[i], GQM_AWAKEN);
          // }
        }
      }
      atomic_load_add_4(&total_node_num, -1, ATOMIC_ACQUIRE | ATOMIC_RELEASE);
    }
    if (total_node_num == 0)
      break;
  }
  return;
}

void run_bfs_nworker(void) {
  INIT_REGION_START();

  graph_init(&g, _node, _node_num, _offset, _edge_num, _pred);
  gqm_init(&task_q, 1023);
  gqm_push(&task_q, (item_t)1, GQM_AWAKEN);
  total_node_num = _node_num;
  
  INIT_REGION_END();

  nworker_start();

  return;
}


/*****************************
 *
 * Graph BFS with pipeline
 *
******************************/

gqm_t pipe0, pipe1, pipe2, pipe3;

void bfs_stage0(void) {
  while (1) {
    item_t pop_data;
    int retval = gqm_pop(&task_q, &pop_data, GQM_AWAKEN);
    if (retval == GQM_SUACCELSS) {
      int node_idx = pop_data;
      int start = g.node[node_idx];
      int end = g.node[node_idx + 1];
      item_t push_data = ((uint64_t)start << 32) | (uint64_t)end;
      gqm_push(&pipe0, push_data, GQM_AWAKEN);
    }
    // exit condition
  }
}

void bfs_stage1(void) {
  while (1) {

  }
}

void bfs_stage2(void) {

}

void bfs_stage3(void) {

}

void run_bfs_pipeline(int max_core_num) {
  INIT_REGION_START();

  graph_init(&g, _node, _node_num, _offset, _edge_num, _pred);
  gqm_init(&task_q, 1023);
  gqm_push(&task_q, (item_t)1, GQM_AWAKEN);
  total_node_num = _node_num;
  
  INIT_REGION_END();

  switch (_coreId )

  return;
}