#include "ds/Graph.h"

const std::vector<AdjEntry> Graph::kEmpty;

int Graph::addNode(const std::string& name, double lat, double lng) {
    int id = nodeCount();
    nodes_.push_back(GraphNode{id, name, lat, lng});
    adj_.push_back(std::vector<AdjEntry>());
    return id;
}

int Graph::addEdge(int u, int v, double km) {
    if (!isValidNode(u) || !isValidNode(v) || u == v) return -1;
    if (!(km > 0.0)) return -1;              // also rejects NaN
    if (findRoad(u, v) != -1) return -1;     // no parallel roads

    int id = roadCount();
    roads_.push_back(Road{id, u, v, km, 1.0, false});
    adj_[u].push_back(AdjEntry{v, id});
    adj_[v].push_back(AdjEntry{u, id});
    return id;
}

const std::vector<AdjEntry>& Graph::neighbors(int u) const {
    if (!isValidNode(u)) return kEmpty;
    return adj_[u];
}

int Graph::findRoad(int u, int v) const {
    if (!isValidNode(u) || !isValidNode(v)) return -1;
    for (const AdjEntry& e : adj_[u]) {
        if (e.neighbor == v) return e.roadId;
    }
    return -1;
}

bool Graph::setTraffic(int u, int v, double factor) {
    if (!(factor >= 1.0 && factor <= 3.0)) return false;
    int r = findRoad(u, v);
    if (r == -1) return false;
    roads_[r].trafficFactor = factor;
    return true;
}

bool Graph::setBlocked(int u, int v, bool blocked) {
    int r = findRoad(u, v);
    if (r == -1) return false;
    roads_[r].blocked = blocked;
    return true;
}

double Graph::effectiveWeight(int roadId) const {
    if (!isValidRoad(roadId)) return 0.0;
    return roads_[roadId].baseKm * roads_[roadId].trafficFactor;
}

bool Graph::isUsable(int roadId) const {
    return isValidRoad(roadId) && !roads_[roadId].blocked;
}

void Graph::clear() {
    nodes_.clear();
    adj_.clear();
    roads_.clear();
}
