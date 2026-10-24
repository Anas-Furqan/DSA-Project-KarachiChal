#pragma once
#include <functional>
#include <vector>

#include "core/DSTrace.h"
#include "ds/Graph.h"

/*
 * BFS — breadth-first search on the road Graph using our LinkedQueue.
 *
 * Used for: (a) surge zones — count free drivers within 2 hops of an area;
 * (b) reachability checks after a road is blocked.
 *
 * Why BFS here (not Dijkstra)? Both questions are about hops / connectivity,
 * not distance, so weights do not matter. BFS visits nodes level by level
 * with a FIFO queue in O(V + E) — no heap and no log factor.
 *
 * Blocked roads are treated as missing edges.
 */
class BFS {
public:
    // O(V + E). hops[u] = number of roads from src to u, or -1 if u is
    // unreachable or further than maxHops. maxHops < 0 means unlimited.
    static std::vector<int> hopsFrom(const Graph& g, int src, int maxHops, DSTrace* trace = nullptr);

    // O(V + E). False for invalid ids.
    static bool isReachable(const Graph& g, int src, int dst, DSTrace* trace = nullptr);

    // O(V + E): sum of countAt(u) over every u within maxHops of src.
    static int sumWithinHops(const Graph& g, int src, int maxHops,
                             const std::function<int(int)>& countAt);
};
