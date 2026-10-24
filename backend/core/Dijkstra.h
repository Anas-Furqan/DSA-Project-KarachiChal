#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/DSTrace.h"
#include "ds/Graph.h"

/*
 * Dijkstra — single-source shortest paths on the road Graph using our MinHeap.
 *
 * Used for: trip routes (pickup -> drop), nearest-driver search (expand from
 * the pickup until the first area with a free driver is popped), live
 * re-routing after traffic/blocks, and the step-by-step dry-run visualizer.
 *
 * Why Dijkstra? All weights (km * traffic) are positive, so the greedy
 * "pop the closest unsettled node" rule is optimal. BFS ignores weights and
 * Bellman-Ford is O(VE) — slower and only needed for negative edges.
 *
 * Blocked roads are skipped. Stale heap entries (lazy deletion) are ignored
 * when popped, so we never need a decrease-key operation.
 *
 * Complexity: O((V + E) log V) — every edge can push once, each push/pop is
 * O(log V) on the heap.
 */

struct PathResult {
    bool found = false;
    double distance = 0.0;        // effective km
    std::vector<int> nodes;       // src ... dst
    std::vector<int> roads;       // roads[i] joins nodes[i] and nodes[i+1]
};

struct NearestResult {
    int node = -1;                // first node that satisfied the predicate
    PathResult path;              // src ... node
};

struct DijkstraRelax {
    int from;
    int to;
    double oldDist;               // < 0 means infinity
    double newDist;
};

struct HeapItem {
    double dist;
    int node;
    bool operator<(const HeapItem& o) const {
        if (dist != o.dist) return dist < o.dist;
        return node < o.node;     // deterministic tie-break
    }
};

struct DijkstraStep {
    int popped;
    double poppedDist;
    bool stale;                         // popped an outdated heap entry
    std::vector<HeapItem> heap;         // heap array after this step
    std::vector<double> dist;           // < 0 means infinity
    std::vector<DijkstraRelax> relaxed;
    std::string explanation;
};

struct DijkstraTrace {
    std::vector<DijkstraStep> steps;
    PathResult result;
};

class Dijkstra {
public:
    // O((V + E) log V). Stops early once dst is popped.
    // Returns found=false for invalid ids or unreachable dst.
    static PathResult shortestPath(const Graph& g, int src, int dst, DSTrace* trace = nullptr);

    // O((V + E) log V) worst case, usually far less: stops at the first
    // popped node for which isTarget(node) is true (that node is the closest).
    static NearestResult findNearest(const Graph& g, int src,
                                     const std::function<bool(int)>& isTarget,
                                     DSTrace* trace = nullptr);

    // O((V + E) log V) time, O(V * steps) memory: full step-by-step record
    // for the dry-run visualizer.
    static DijkstraTrace traceSteps(const Graph& g, int src, int dst);
};
