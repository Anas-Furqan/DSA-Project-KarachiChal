#pragma once
#include <string>
#include <vector>

/*
 * Graph — undirected weighted graph stored as an ADJACENCY LIST.
 *
 * Models the Karachi road network: every area is a node, every road an
 * undirected edge. Each road keeps its base length, a traffic factor and a
 * blocked flag, so the effective weight can change at runtime.
 *
 * Why an adjacency list (not a matrix)?
 *   The city graph is sparse (~30 nodes, ~50 roads). A matrix costs O(V^2)
 *   memory and O(V) to list neighbours; a list costs O(V + E) memory and
 *   O(deg) to list neighbours, which is exactly what Dijkstra and BFS need.
 *
 * Roads are stored once in `roads_`; both endpoints' adjacency entries point
 * to the same road id, so a traffic change is seen from both directions.
 */

struct GraphNode {
    int id;
    std::string name;
    double lat;
    double lng;
};

struct Road {
    int id;
    int from;
    int to;
    double baseKm;
    double trafficFactor;   // 1.0 = normal, up to 3.0 = heavy
    bool blocked;
};

struct AdjEntry {
    int neighbor;   // node on the other side of the road
    int roadId;     // index into roads_
};

class Graph {
public:
    // O(1) amortized. Returns the new node id.
    int addNode(const std::string& name, double lat, double lng);

    // O(1) amortized. Returns road id, or -1 if endpoints invalid, u == v,
    // km <= 0, or the road already exists (duplicate check is O(deg)).
    int addEdge(int u, int v, double km);

    // O(1)
    int nodeCount() const { return (int)nodes_.size(); }
    // O(1)
    int roadCount() const { return (int)roads_.size(); }
    // O(1)
    bool isValidNode(int u) const { return u >= 0 && u < nodeCount(); }
    // O(1)
    bool isValidRoad(int r) const { return r >= 0 && r < roadCount(); }

    // O(1). Caller must pass a valid id.
    const GraphNode& node(int u) const { return nodes_[u]; }
    // O(1). Caller must pass a valid id.
    const Road& road(int r) const { return roads_[r]; }

    // O(1) to return; iterating it is O(deg(u)). Empty list for invalid u.
    const std::vector<AdjEntry>& neighbors(int u) const;

    // O(deg(u)). Returns road id between u and v, or -1.
    int findRoad(int u, int v) const;

    // O(deg(u)) because the road is located through u's list.
    // Factor must be in [1.0, 3.0]. Returns false on bad input.
    bool setTraffic(int u, int v, double factor);

    // O(deg(u)). Idempotent: blocking a blocked road just returns true.
    bool setBlocked(int u, int v, bool blocked);

    // O(1). baseKm * trafficFactor.
    double effectiveWeight(int roadId) const;

    // O(1). True if road exists and is not blocked.
    bool isUsable(int roadId) const;

    // O(V + E)
    void clear();

private:
    std::vector<GraphNode> nodes_;
    std::vector<std::vector<AdjEntry>> adj_;
    std::vector<Road> roads_;
    static const std::vector<AdjEntry> kEmpty;
};
