#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "dijkstra.hpp"
#include "graph.hpp"
#include "json.hpp"

using namespace std;

struct Row { string a, b; double route, straight, ratio; };

int main(int argc, char** argv) {
    const string path = argc > 1 ? argv[1] : "../data/building.json";
    Graph g;
    double scale = 0;
    try {
        g.load(path);
        ifstream in(path);
        scale = nlohmann::json::parse(in).at("meta").at("scale_m_per_px").get<double>();
    } catch (const exception& e) {
        cerr << "load failed: " << e.what() << "\n";
        return 2;
    }

    vector<int> sel;
    for (int i = 0; i < g.size(); ++i)
        if (g.node(i).selectable) sel.push_back(i);

    vector<Row> rows;
    double longest = 0, sum = 0;
    string la, lb;
    long pairs = 0, unreachable = 0;

    for (size_t i = 0; i < sel.size(); ++i) {
        for (size_t j = i + 1; j < sel.size(); ++j) {
            PathResult r = shortestPath(g, sel[i], sel[j]);
            if (!r.found) { ++unreachable; continue; }
            ++pairs;
            sum += r.distance;
            const Node& A = g.node(sel[i]);
            const Node& B = g.node(sel[j]);
            if (r.distance > longest) { longest = r.distance; la = A.id; lb = B.id; }
            if (A.floor == B.floor) {
                double straight = hypot(A.x - B.x, A.y - B.y) * scale;
                if (straight >= 10.0)  // ignore near-neighbours, where ratios are noise
                    rows.push_back({A.id, B.id, r.distance, straight, r.distance / straight});
            }
        }
    }

    cout << fixed << setprecision(1);
    cout << "locations: " << sel.size() << ", pairs: " << pairs
         << ", unreachable: " << unreachable << "\n";
    cout << "average route: " << sum / pairs << " m\n";
    cout << "longest route: " << la << " -> " << lb << " = " << longest << " m\n\n";

    sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) { return x.ratio > y.ratio; });
    cout << "Worst same-floor detour ratios (review these against the real building):\n";
    for (size_t i = 0; i < min<size_t>(10, rows.size()); ++i)
        cout << "  " << rows[i].a << " -> " << rows[i].b << ": route " << rows[i].route
             << " m vs straight " << rows[i].straight << " m (x" << setprecision(2)
             << rows[i].ratio << setprecision(1) << ")\n";
    return unreachable == 0 ? 0 : 1;
}