#include "graph.hpp"

#include <fstream>
#include <stdexcept>

#include "json.hpp"

using json = nlohmann::json;

void Graph::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    json j = json::parse(in);  // throws json::parse_error on bad input

    nodes_.clear();
    adj_.clear();
    index_.clear();
    edgeCount_ = 0;

    for (const auto& n : j.at("nodes")) {
        Node node;
        node.id = n.at("id").get<std::string>();
        node.type = n.at("type").get<std::string>();
        node.label = n.at("label").get<std::string>();
        node.floor = n.at("floor").get<int>();
        node.x = n.at("x").get<double>();
        node.y = n.at("y").get<double>();
        node.selectable = n.at("selectable").get<bool>();
        if (index_.count(node.id)) throw std::runtime_error("duplicate node id " + node.id);
        index_[node.id] = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(node));
    }
    adj_.assign(nodes_.size(), {});

    for (const auto& e : j.at("edges")) {
        const std::string from = e.at("from").get<std::string>();
        const std::string to = e.at("to").get<std::string>();
        const double w = e.at("w").get<double>();
        const bool stairs = e.at("kind").get<std::string>() == "stairs";

        auto a = index_.find(from);
        auto b = index_.find(to);
        if (a == index_.end() || b == index_.end())
            throw std::runtime_error("edge references unknown node: " + from + " -> " + to);
        if (w < 0)  // Dijkstra is only correct for non-negative weights
            throw std::runtime_error("negative edge weight: " + from + " -> " + to);

        adj_[a->second].push_back({b->second, w, stairs});
        adj_[b->second].push_back({a->second, w, stairs});
        ++edgeCount_;
    }
}

int Graph::indexOf(const std::string& id) const {
    auto it = index_.find(id);
    return it == index_.end() ? -1 : it->second;
}