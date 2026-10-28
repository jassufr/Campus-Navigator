#include "dijkstra.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

PathResult shortestPath(const Graph& g, int src, int dst) {
    const int n = g.size();
    if (src < 0 || src >= n || dst < 0 || dst >= n)
        throw std::out_of_range("node index out of range");

    const double INF = std::numeric_limits<double>::infinity();
    std::vector<double> dist(n, INF);
    std::vector<int> prev(n, -1);

    using Item = std::pair<double, int>;  // (distance, node)
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;

    dist[src] = 0;
    pq.push({0.0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u]) continue;  // stale entry (lazy deletion)
        if (u == dst) break;        // dst is finalized; safe because weights >= 0
        for (const Edge& e : g.neighbors(u)) {
            const double nd = d + e.w;
            if (nd < dist[e.to]) {
                dist[e.to] = nd;
                prev[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }

    PathResult r;
    if (dist[dst] == INF) return r;  // found == false

    r.found = true;
    r.distance = dist[dst];
    for (int v = dst; v != -1; v = prev[v]) r.path.push_back(v);
    std::reverse(r.path.begin(), r.path.end());

    for (std::size_t i = 1; i < r.path.size(); ++i)
        if (g.node(r.path[i - 1]).floor != g.node(r.path[i]).floor) ++r.stairsCount;

    return r;
}