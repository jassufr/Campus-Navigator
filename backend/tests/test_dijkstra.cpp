#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "dijkstra.hpp"
#include "graph.hpp"

using namespace std;

static int failures = 0;
#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ++failures;                                                        \
            cerr << "FAIL: " << msg << "   [" << #cond << "]\n";                \
        }                                                                      \
    } while (0)

// Independent oracle: plain Bellman-Ford, O(V*E).
static vector<double> bellmanFord(const Graph& g, int s) {
    const double INF = numeric_limits<double>::infinity();
    vector<double> d(g.size(), INF);
    d[s] = 0;
    for (int pass = 0; pass < g.size() - 1; ++pass) {
        bool changed = false;
        for (int u = 0; u < g.size(); ++u) {
            if (d[u] == INF) continue;
            for (const Edge& e : g.neighbors(u))
                if (d[u] + e.w < d[e.to]) {
                    d[e.to] = d[u] + e.w;
                    changed = true;
                }
        }
        if (!changed) break;
    }
    return d;
}

static double edgeWeight(const Graph& g, int a, int b) {
    for (const Edge& e : g.neighbors(a))
        if (e.to == b) return e.w;
    return -1;
}

static void printRoute(const Graph& g, const string& from, const string& to) {
    PathResult r = shortestPath(g, g.indexOf(from), g.indexOf(to));
    cout << from << " -> " << to << ": " << fixed << setprecision(1) << r.distance
         << " m, stairs: " << r.stairsCount << "\n   ";
    for (size_t i = 0; i < r.path.size(); ++i)
        cout << (i ? " > " : "") << g.node(r.path[i]).id;
    cout << "\n";
}

int main(int argc, char** argv) {
    const string path = argc > 1 ? argv[1] : "../data/building.json";
    Graph g;
    try {
        g.load(path);
    } catch (const exception& ex) {
        cerr << "load failed: " << ex.what() << "\n";
        return 2;
    }
    cout << "loaded " << g.size() << " nodes, " << g.edgeCount() << " edges\n";

    auto id = [&](const string& s) {
        int i = g.indexOf(s);
        if (i < 0) {
            cerr << "unknown id " << s << "\n";
            exit(2);
        }
        return i;
    };
    const int n = g.size();

    // 1. Trivial and invalid cases
    {
        PathResult r = shortestPath(g, id("A001"), id("A001"));
        CHECK(r.found && r.distance == 0 && r.path.size() == 1, "src == dst");
        CHECK(g.indexOf("Z999") == -1, "unknown id returns -1");
        bool threw = false;
        try { shortestPath(g, -1, 0); } catch (const out_of_range&) { threw = true; }
        CHECK(threw, "invalid index throws");
    }

    // 2. Oracle: Dijkstra must equal Bellman-Ford for every target
    for (const string& s : {"A001", "B322", "ENTRY_C_A_F2", "J_A_M_F1"}) {
        const int si = id(s);
        vector<double> bf = bellmanFord(g, si);
        for (int t = 0; t < n; ++t) {
            PathResult r = shortestPath(g, si, t);
            CHECK(r.found, s << " -> " << g.node(t).id << " not found");
            CHECK(fabs(r.distance - bf[t]) < 1e-9,
                  s << " -> " << g.node(t).id << ": dijkstra " << r.distance
                    << " vs bellman-ford " << bf[t]);
        }
    }

    // 3. Property tests on random pairs
    mt19937 rng(42);
    uniform_int_distribution<int> pick(0, n - 1);
    for (int trial = 0; trial < 300; ++trial) {
        int a = pick(rng), b = pick(rng), c = pick(rng);
        PathResult ab = shortestPath(g, a, b);
        PathResult ba = shortestPath(g, b, a);
        PathResult bc = shortestPath(g, b, c);
        PathResult ac = shortestPath(g, a, c);
        const string tag = g.node(a).id + "/" + g.node(b).id + "/" + g.node(c).id;

        CHECK(fabs(ab.distance - ba.distance) < 1e-9, "symmetry " << tag);
        CHECK(ac.distance <= ab.distance + bc.distance + 1e-9, "triangle inequality " << tag);

        // Path validity: endpoints, adjacency, and weights summing to the distance
        CHECK(ab.path.front() == a && ab.path.back() == b, "path endpoints " << tag);
        double sum = 0;
        for (size_t i = 1; i < ab.path.size(); ++i) {
            double w = edgeWeight(g, ab.path[i - 1], ab.path[i]);
            CHECK(w >= 0, "non-adjacent consecutive nodes " << tag);
            sum += w;
        }
        CHECK(fabs(sum - ab.distance) < 1e-6, "path weight sum " << tag);

        // Each stair edge changes exactly one floor, and a detour never pays off
        CHECK(ab.stairsCount == abs(g.node(a).floor - g.node(b).floor),
              "stair count equals floor difference " << tag);
    }

    // 4. Routes to eyeball
    cout << "\nSample routes (sanity-check these against the real building):\n";
    printRoute(g, "A006", "A306");
    printRoute(g, "A001", "B022");
    printRoute(g, "B322", "ENTRY_C_A_F0");
    printRoute(g, "A315", "A014");

    if (failures == 0) {
        cout << "\nALL TESTS PASSED\n";
        return 0;
    }
    cout << "\n" << failures << " CHECK(S) FAILED\n";
    return 1;
}