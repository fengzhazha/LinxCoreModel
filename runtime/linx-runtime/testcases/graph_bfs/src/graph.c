#include "graph.h"

void graph_init(graph_t *g, const int *node, int v_num, const int *offset, int e_num, int *pred) {
  g->node = node;
  g->offset = offset;
  g->pred = pred;
  g->v_num = v_num;
  g->e_num = e_num;
  return;
}