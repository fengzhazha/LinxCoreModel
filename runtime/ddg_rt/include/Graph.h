#pragma once

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#include <map>
#include <atomic>

typedef long long int64;
class GraphNode {
public:
    int64 cost;
    int64 neighbor_num;
    GraphNode** neighbors;
#ifndef LINXV4
    std::atomic<int64> pred_num;
#else
    int64 pred_num;
#endif
    int64 id;

    GraphNode(int id, int cost) {
        this->id = id;
        this->cost = cost; 
        pred_num = 0;
        neighbor_num = 0;
    }
    ~GraphNode() {}
    void add_neighbor(GraphNode *nb) {
        neighbors[neighbor_num] = nb;
        neighbor_num++;
        nb->pred_num++;
    }
};

class Graph {
public:
    std::vector<GraphNode*> nodes;

    Graph() {}
    ~Graph() {
        // for (int i = 0; i < nodes.size(); i++)
        //     delete nodes[i];
    }

    void add_node(GraphNode* n) {
        nodes.push_back(n);
    }

    void add_edge(GraphNode* s, GraphNode* e) {
        s->add_neighbor(e);
    }

    void add_nodes(int *ids, int *costs, int node_num) {
        for (int i = 0; i < node_num; i++) {
            add_node(new GraphNode(ids[i], costs[i]));
        }
    }

    void add_edges(int *s, int *e, int edge_num) {
        for (int i = 0; i < edge_num; i++) {
            nodes[s[i]]->neighbor_num++;
        }
        for (auto n : nodes) {
            n->neighbors = new GraphNode*[n->neighbor_num];
            n->neighbor_num = 0;
        }
        for (int i = 0; i < edge_num; i++) {
            add_edge(nodes[s[i]], nodes[e[i]]);
        }
    }

    int load(std::string txt) {
        // int num_node, num_edge;
        // std::vector<int> nodes_id, nodes_cost;
        // std::vector<std::vector<int>> edges;
        // std::map<int, int> id2idx;

        // std::ifstream fin(txt);
        // std::string str_line;
        // int line_idx = 0;
        // while (getline(fin, str_line)) {
        //     std::istringstream iss{str_line};
        //     if (line_idx == 0) {
        //         std::string str_node_num, str_edge_num;
        //         std::getline(iss, str_node_num, ',');
        //         std::getline(iss, str_edge_num, ',');
        //         num_node = std::atoi(str_node_num.c_str());
        //         num_edge = std::atoi(str_edge_num.c_str());
        //     } else if (line_idx <= num_node) {
        //         std::string str_id, str_cost;
        //         std::getline(iss, str_id, ',');
        //         std::getline(iss, str_cost, ',');
        //         int id = std::atoi(str_id.c_str());
        //         int cost = std::atoi(str_cost.c_str());
        //         nodes_id.push_back(id);
        //         nodes_cost.push_back(cost);
        //         id2idx.insert(std::make_pair(id, line_idx - 1));
        //     } else if (line_idx <= num_node + num_edge) {
        //         std::string start_idx, end_idx;
        //         std::getline(iss, start_idx, ',');
        //         std::getline(iss, end_idx, ',');
        //         int s = std::atoi(start_idx.c_str());
        //         int e = std::atoi(end_idx.c_str());
        //         edges.push_back({id2idx[s], id2idx[e]});
        //     }
        //     line_idx ++;
        // }
        // add_nodes(nodes_id, nodes_cost);
        // add_edges(edges);
        return 0;
    }
};
