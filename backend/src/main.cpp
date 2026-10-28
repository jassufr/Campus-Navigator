#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "httplib.h"
#include "json.hpp"

#include "dijkstra.hpp"
#include "graph.hpp"

using json = nlohmann::json;

namespace {

constexpr double WALK_SPEED_MPS = 1.2;  // assumption, used only for the time estimate

std::string normalize(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

double round1(double v) { return std::round(v * 10.0) / 10.0; }

void sendJson(httplib::Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

void sendError(httplib::Response& res, int status, const std::string& msg) {
    sendJson(res, status, {{"error", msg}});
}

}  // namespace

int main(int argc, char** argv) {
    // Usage: ./server [project_root] [port]      (defaults: .. and 8080)
    const std::string root = argc > 1 ? argv[1] : "..";
    const int port = argc > 2 ? std::atoi(argv[2]) : 8080;

    Graph graph;
    try {
        graph.load(root + "/data/building.json");
    } catch (const std::exception& ex) {
        std::cerr << "Failed to load " << root << "/data/building.json: " << ex.what() << "\n"
                  << "Did you run: python3 tools/generate_building.py ?\n";
        return 1;
    }
    std::cout << "Graph loaded: " << graph.size() << " nodes, " << graph.edgeCount()
              << " edges\n";

    httplib::Server svr;

    // During development, never let the browser cache stale JS/CSS/JSON.
    svr.set_default_headers({{"Cache-Control", "no-store"}});

    // Static files: the graph data and the frontend.
    if (!svr.set_mount_point("/data", root + "/data")) {
        std::cerr << "Cannot mount " << root << "/data\n";
        return 1;
    }
    if (!svr.set_mount_point("/", root + "/frontend")) {
        std::cerr << "Cannot mount " << root << "/frontend\n";
        return 1;
    }

    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, 200, {{"status", "ok"}, {"nodes", graph.size()}, {"edges", graph.edgeCount()}});
    });

    // The Graph is read-only after load() and shortestPath() keeps all its state in
    // local variables, so concurrent requests handled by httplib's thread pool are safe.
    svr.Get("/api/route", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("from") || !req.has_param("to")) {
            sendError(res, 400, "Both 'from' and 'to' query parameters are required.");
            return;
        }
        const std::string fromId = normalize(req.get_param_value("from"));
        const std::string toId = normalize(req.get_param_value("to"));

        const int src = graph.indexOf(fromId);
        const int dst = graph.indexOf(toId);
        if (src < 0) { sendError(res, 404, "Unknown location: " + fromId); return; }
        if (dst < 0) { sendError(res, 404, "Unknown location: " + toId); return; }
        if (!graph.node(src).selectable) { sendError(res, 400, fromId + " is not a room or entry."); return; }
        if (!graph.node(dst).selectable) { sendError(res, 400, toId + " is not a room or entry."); return; }

        PathResult r;
        try {
            r = shortestPath(graph, src, dst);
        } catch (const std::exception& ex) {
            sendError(res, 500, std::string("Internal error: ") + ex.what());
            return;
        }
        if (!r.found) {
            sendError(res, 404, "No route between " + fromId + " and " + toId + ".");
            return;
        }

        json path = json::array();
        for (int idx : r.path) {
            const Node& n = graph.node(idx);
            path.push_back({{"id", n.id},
                            {"type", n.type},
                            {"label", n.label},
                            {"floor", n.floor},
                            {"x", n.x},
                            {"y", n.y}});
        }
        sendJson(res, 200,
                 {{"from", fromId},
                  {"to", toId},
                  {"distance_m", round1(r.distance)},
                  {"stairs", r.stairsCount},
                  {"est_seconds", static_cast<int>(std::round(r.distance / WALK_SPEED_MPS))},
                  {"path", path}});
    });

    svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, std::exception_ptr) {
        sendError(res, 500, "Unexpected server error.");
    });

    std::cout << "Serving on http://localhost:" << port << "  (Ctrl+C to stop)\n";
    if (!svr.listen("127.0.0.1", port)) {
        std::cerr << "Could not listen on port " << port << " (already in use?)\n";
        return 1;
    }
    return 0;
}