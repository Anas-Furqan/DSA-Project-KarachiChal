#pragma once
#include <string>

#include "core/DSTrace.h"
#include "core/Models.h"
#include "ds/Graph.h"
#include "ds/HashTable.h"
#include "ds/MinHeap.h"

using namespace std;

struct PathResult {
    bool found;
    double distance;
    int nodes[MAX_ROUTE];
    int roads[MAX_ROUTE];
    int nodeCount;
};

struct RelaxInfo {
    int from;
    int to;
    double oldDist;
    double newDist;
};

// One heap pop of the dry run. Distances of -1 mean infinity.
struct DijkstraStep {
    int popped;
    double poppedDist;
    bool stale;
    HeapItem* heap;
    int heapCount;
    double dist[MAX_NODES];
    RelaxInfo relaxed[MAX_NODES];
    int relaxCount;
    string explanation;
};

class DijkstraTrace {
public:
    DijkstraTrace();
    ~DijkstraTrace();
    void init(int maxSteps);

    DijkstraStep* steps;
    int stepCount;
    int capacity;
    PathResult result;
};

class Dijkstra {
public:
    static PathResult shortestPath(const Graph& g, int src, int dst, DSTrace* trace = NULL);
    static PathResult findNearest(const Graph& g, int src, const HashTable& freeCount, RideType type,
                                  int& foundNode, DSTrace* trace = NULL);
    static void traceSteps(const Graph& g, int src, int dst, DijkstraTrace& out);

private:
    static int search(const Graph& g, int src, int dst, const HashTable* freeCount, RideType type,
                      double dist[], int prevNode[], int prevRoad[], DSTrace* trace, DijkstraTrace* rec);
    static PathResult buildPath(int src, int dst, double dist[], int prevNode[], int prevRoad[]);
    static void saveStep(DijkstraStep* step, const MinHeap& heap, double dist[], int n);
};

PathResult emptyPath();
