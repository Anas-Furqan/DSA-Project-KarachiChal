#pragma once

#include "core/DSTrace.h"
#include "ds/Graph.h"

using namespace std;

class BFS {
public:
    // hops[u] = roads from src to u, -1 if not reached. maxHops < 0 means no limit.
    static int hopsFrom(const Graph& g, int src, int maxHops, int hops[], DSTrace* trace = NULL);
    static bool isReachable(const Graph& g, int src, int dst, DSTrace* trace = NULL);
    static int sumWithinHops(const Graph& g, int src, int maxHops, const int countAt[]);
};
