#ifndef __GRAPH_H
#define __GRAPH_H

// graph is stored with CSR format
typedef struct {
    const int *node;
    const int *offset;
    int *pred;
    int v_num;
    int e_num;
} graph_t;

void graph_init(graph_t *g, const int *node, int v_num, const int *offset, int e_num, int *pred);

#endif // __DDGRT_GRAPH_H
