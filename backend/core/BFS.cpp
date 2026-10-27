#include "core/BFS.h"
#include "ds/LinkedQueue.h"

using namespace std;

int BFS::hopsFrom(const Graph& g, int src, int maxHops, int hops[], DSTrace* trace) {
    for (int i = 0; i < g.nodeCount(); i++) hops[i] = -1;
    if (!g.isValidNode(src)) return 0;

    LinkedQueue frontier;
    hops[src] = 0;
    frontier.enqueue(src);
    int visited = 1;

    int u = 0;
    while (frontier.dequeue(u)) {
        if (maxHops >= 0 && hops[u] >= maxHops) continue;
        for (EdgeNode* e = g.neighbors(u); e != NULL; e = e->next) {
            if (!g.isUsable(e->roadId) || hops[e->to] != -1) continue;
            hops[e->to] = hops[u] + 1;
            frontier.enqueue(e->to);
            visited++;
        }
    }
    if (trace) {
        string limit = maxHops >= 0 ? " within " + to_string(maxHops) + " hops" : "";
        trace->add("BFS", "traverse", "O(V+E)", "From " + g.getNode(src).name + ": reached " + to_string(visited) + " areas" + limit);
    }
    return visited;
}

bool BFS::isReachable(const Graph& g, int src, int dst, DSTrace* trace) {
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return false;
    int hops[MAX_NODES];
    hopsFrom(g, src, -1, hops, trace);
    return hops[dst] != -1;
}

int BFS::sumWithinHops(const Graph& g, int src, int maxHops, const int countAt[]) {
    int hops[MAX_NODES];
    hopsFrom(g, src, maxHops, hops, NULL);
    int total = 0;
    for (int u = 0; u < g.nodeCount(); u++) {
        if (hops[u] != -1) total += countAt[u];
    }
    return total;
}
