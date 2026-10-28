#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct Node {
    std::string id;      // e.g. "A306", "ENTRY_C_A_F2", "J_A_M_F1"
    std::string type;    // "room" | "junction" | "stairs" | "entry"
    std::string label;
    int floor = 0;
    double x = 0, y = 0; // map coordinates (for the frontend, not used by Dijkstra)
    bool selectable = false;
};

struct Edge {
    int to;
    double w;      // meters
    bool stairs;   // true if this edge changes floor
};

class Graph {
public:
    // Throws std::runtime_error on a missing file, bad JSON, unknown ids or negative weights.
    void load(const std::string& path);

    int size() const { return static_cast<int>(nodes_.size()); }
    std::size_t edgeCount() const { return edgeCount_; }  // undirected edges
    const Node& node(int i) const { return nodes_[i]; }
    const std::vector<Edge>& neighbors(int i) const { return adj_[i]; }
    int indexOf(const std::string& id) const;             // -1 if unknown

private:
    std::vector<Node> nodes_;
    std::vector<std::vector<Edge>> adj_;
    std::unordered_map<std::string, int> index_;
    std::size_t edgeCount_ = 0;
};