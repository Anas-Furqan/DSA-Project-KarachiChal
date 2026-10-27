#include "core/SurgeEngine.h"
#include "core/BFS.h"

using namespace std;

SurgeEngine::SurgeEngine() {
    reset(0);
}

double SurgeEngine::multiplierFor(int demand, int supply) {
    if (demand <= 0) return 1.0;
    if (supply < 1) supply = 1;
    double ratio = (double)demand / supply;
    if (ratio < 1.0) return 1.0;
    if (ratio < 2.0) return 1.25;
    if (ratio < 3.0) return 1.5;
    return 2.0;
}

void SurgeEngine::reset(int areas) {
    if (areas < 0) areas = 0;
    if (areas > MAX_NODES) areas = MAX_NODES;
    areaCount = areas;
    for (int i = 0; i < MAX_NODES; i++) {
        info[i].demand = 0;
        info[i].supply = 0;
        info[i].multiplier = 1.0;
    }
}

void SurgeEngine::recompute(const Graph& g, const int demand[], const int freeDrivers[], DSTrace* trace) {
    if (areaCount != g.nodeCount()) reset(g.nodeCount());
    for (int a = 0; a < g.nodeCount(); a++) {
        info[a].demand = demand[a];
        // no BFS needed when nobody is waiting here
        if (demand[a] > 0) {
            info[a].supply = BFS::sumWithinHops(g, a, SURGE_HOPS, freeDrivers);
        } else {
            info[a].supply = 0;
        }
        info[a].multiplier = multiplierFor(info[a].demand, info[a].supply);
        if (trace && info[a].multiplier > 1.0) {
            trace->add("BFS", "surgeZone", "O(V+E)",
                       g.getNode(a).name + ": " + to_string(info[a].demand) + " requests / " + to_string(info[a].supply) +
                           " drivers within 2 hops -> " + DSTrace::num(info[a].multiplier, 2) + "x");
        }
    }
}

double SurgeEngine::multiplierAt(int area) const {
    if (area < 0 || area >= areaCount) return 1.0;
    return info[area].multiplier;
}

SurgeInfo SurgeEngine::infoAt(int area) const {
    if (area < 0 || area >= areaCount) {
        SurgeInfo none;
        none.demand = 0;
        none.supply = 0;
        none.multiplier = 1.0;
        return none;
    }
    return info[area];
}
