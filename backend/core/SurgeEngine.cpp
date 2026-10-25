#include "core/SurgeEngine.h"

#include "core/BFS.h"

double SurgeEngine::multiplierFor(int demand, int supply) {
    if (demand <= 0) return 1.0;
    double ratio = (double)demand / (double)(supply > 1 ? supply : 1);
    if (ratio < 1.0) return 1.0;
    if (ratio < 2.0) return 1.25;
    if (ratio < 3.0) return 1.5;
    return 2.0;
}

void SurgeEngine::reset(int areas) { info_.assign(areas < 0 ? 0 : areas, SurgeInfo()); }

void SurgeEngine::recompute(const Graph& g, const std::function<int(int)>& demandAt,
                            const std::function<int(int)>& freeDriversAt, DSTrace* trace) {
    if ((int)info_.size() != g.nodeCount()) reset(g.nodeCount());
    for (int a = 0; a < g.nodeCount(); ++a) {
        SurgeInfo& s = info_[a];
        s.demand = demandAt(a);
        // skip the BFS when nobody is waiting here: multiplier is 1.0 anyway
        s.supply = s.demand > 0 ? BFS::sumWithinHops(g, a, kHops, freeDriversAt) : 0;
        s.multiplier = multiplierFor(s.demand, s.supply);
        if (s.multiplier > 1.0 && trace) {
            trace->add("BFS", "surgeZone", "O(V+E)",
                       g.node(a).name + ": " + std::to_string(s.demand) + " requests / " +
                           std::to_string(s.supply) + " drivers within 2 hops -> " +
                           DSTrace::num(s.multiplier, 2) + "x");
        }
    }
}

double SurgeEngine::multiplierAt(int area) const {
    if (area < 0 || area >= (int)info_.size()) return 1.0;
    return info_[area].multiplier;
}

const SurgeInfo& SurgeEngine::infoAt(int area) const {
    if (area < 0 || area >= (int)info_.size()) return none_;
    return info_[area];
}
