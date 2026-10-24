#include "core/BFS.h"

#include "ds/LinkedQueue.h"

std::vector<int> BFS::hopsFrom(const Graph& g, int src, int maxHops, DSTrace* trace) {
    std::vector<int> hops(g.nodeCount(), -1);   // doubles as the visited array
    if (!g.isValidNode(src)) return hops;

    LinkedQueue<int> frontier;
    hops[src] = 0;
    frontier.enqueue(src);
    int visited = 1;

    int u = 0;
    while (frontier.dequeue(u)) {
        if (maxHops >= 0 && hops[u] >= maxHops) continue;   // do not expand further
        for (const AdjEntry& e : g.neighbors(u)) {
            if (!g.isUsable(e.roadId) || hops[e.neighbor] != -1) continue;
            hops[e.neighbor] = hops[u] + 1;
            frontier.enqueue(e.neighbor);
            ++visited;
        }
    }
    if (trace) {
        trace->add("BFS", "traverse", "O(V+E)",
                   "From " + g.node(src).name + ": reached " + std::to_string(visited) + " areas" +
                       (maxHops >= 0 ? " within " + std::to_string(maxHops) + " hops" : ""));
    }
    return hops;
}

bool BFS::isReachable(const Graph& g, int src, int dst, DSTrace* trace) {
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return false;
    std::vector<int> hops = hopsFrom(g, src, -1, trace);
    return hops[dst] != -1;
}

int BFS::sumWithinHops(const Graph& g, int src, int maxHops, const std::function<int(int)>& countAt) {
    std::vector<int> hops = hopsFrom(g, src, maxHops);
    int total = 0;
    for (int u = 0; u < (int)hops.size(); ++u) {
        if (hops[u] != -1) total += countAt(u);
    }
    return total;
}
