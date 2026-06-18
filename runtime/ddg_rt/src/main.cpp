#include "Runtime.h"
#include "Graph.h"
#include "multi_pred_llama_subgraph.h"

#include <iostream>
#include <unistd.h>
#include <chrono>

int graph_bfs(Runtime& rt, int tpid, GraphNode* gn) {
    for (int i = 0; i < gn->neighbor_num; i++) {
        gn->neighbors[i]->pred_num--;
        if (gn->neighbors[i]->pred_num == 0)
            rt.push_task(rt.new_task(tpid, graph_bfs, gn->neighbors[i]), 0);
    }

#ifndef LINXV4
    std::cout << "Processing node with ID " << gn->id << std::endl;
#endif

    return 0;
}

void graph_bfs_thread(Runtime& rt, int tpid);

Runtime *rt;

int main(int argc, char* argv[]) {

#ifndef LINXV4
    // rt.run_scalar();
    int worker_num = 4;
    rt = new Runtime(worker_num, 16384);
    Graph g;
    g.add_nodes(nodes_id, nodes_cost, node_num);
    g.add_edges(edges_s, edges_e, edge_num);

    for (int i = 0; i < node_num; i++) {
        if (g.nodes[i]->pred_num == 0) {
            Task* t = rt->new_task(0, nullptr, g.nodes[i]);
            rt->push_task(t, 0);
        }
    }

    auto tstart = std::chrono::system_clock::now();

    rt->run_smt_multipred(node_num);
    
    auto tend = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(tend - tstart);
    std::cout << "[ " << worker_num << " threads ]" << "Processing " << node_num << " nodes in " << duration.count() << " ms!" << std::endl;
    // std::cout << "Max task queue size needed: " << rt.max_queue_size << std::endl;
#else
    rt.run_smt_multipred(node_num);
#endif

    return 0;
}