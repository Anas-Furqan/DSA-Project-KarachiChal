#pragma once

#include "core/DSTrace.h"
#include "ds/Graph.h"

using namespace std;

const int SURGE_HOPS = 2;

struct SurgeInfo {
    int demand;
    int supply;
    double multiplier;
};

class SurgeEngine {
public:
    SurgeEngine();

    static double multiplierFor(int demand, int supply);
    void recompute(const Graph& g, const int demand[], const int freeDrivers[], DSTrace* trace = NULL);
    double multiplierAt(int area) const;
    SurgeInfo infoAt(int area) const;
    void reset(int areas);

private:
    SurgeInfo info[MAX_NODES];
    int areaCount;
};
