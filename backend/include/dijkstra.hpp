#pragma once
#include <vector>

#include "graph.hpp"

struct PathResult {
    bool found = false;
    double distance = 0;      // total cost in meters (stairs count as stair_cost_m each)
    std::vector<int> path;    // node indices, src ... dst
    int stairsCount = 0;      // number of floor changes along the path
};

// Throws std::out_of_range if src or dst is not a valid node index.
PathResult shortestPath(const Graph& g, int src, int dst);