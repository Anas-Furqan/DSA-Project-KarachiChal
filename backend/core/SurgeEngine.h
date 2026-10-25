#pragma once
#include <functional>
#include <vector>

#include "core/DSTrace.h"
#include "ds/Graph.h"

/*
 * SurgeEngine — computes a fare multiplier per area from local supply and
 * demand.
 *
 *   demand  = pending requests in the area (HashTable lookup, O(1) avg)
 *   supply  = free drivers within 2 hops   (BFS, O(V + E))
 *   ratio   = demand / max(supply, 1)
 *   ratio < 1 -> 1.0x, < 2 -> 1.25x, < 3 -> 1.5x, else 2.0x
 *
 * Why BFS? "Within 2 hops" is a level-limited neighbourhood question; BFS
 * explores exactly level by level and stops at depth 2.
 *
 * Multipliers are stored in a plain array indexed by area id (O(1) read).
 */

struct SurgeInfo {
    int demand = 0;
    int supply = 0;
    double multiplier = 1.0;
};

class SurgeEngine {
public:
    static const int kHops = 2;

    // O(1): ratio -> multiplier step function.
    static double multiplierFor(int demand, int supply);

    // O(V * (V + E)): one BFS per area. Called every simulation tick.
    void recompute(const Graph& g, const std::function<int(int)>& demandAt,
                   const std::function<int(int)>& freeDriversAt, DSTrace* trace = nullptr);

    // O(1). 1.0 for unknown areas.
    double multiplierAt(int area) const;
    // O(1)
    const SurgeInfo& infoAt(int area) const;
    // O(1)
    int areaCount() const { return (int)info_.size(); }

    // O(V)
    void reset(int areas);

private:
    std::vector<SurgeInfo> info_;
    SurgeInfo none_;
};
