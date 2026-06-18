#ifndef __CSR_TEST_H
#define __CSR_TEST_H

THREAD_LOCAL const int _node_num = 9;
THREAD_LOCAL const int _edge_num = 9;

THREAD_LOCAL const int _node[12] = {0, 0, 2, 4, 6, 8, 10, 12, 12, 12, 12, 12};
THREAD_LOCAL const int _offset[12] = {2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 9, 10};
THREAD_SHARE int _pred[12] = {0, 0, 1, 1, 1, 2, 1, 1, 2, 2, 1, 0};
THREAD_LOCAL int _free_list[1] = {1};

#endif // __CSR_TEST_H