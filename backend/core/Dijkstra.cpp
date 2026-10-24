#include "core/Dijkstra.h"

#include "ds/MinHeap.h"

namespace {

const double kInf = 1e18;

struct SearchState {
    std::vector<double> dist;
    std::vector<int> prevNode;
    std::vector<int> prevRoad;
    std::vector<bool> settled;
};

std::string km(double d) { return d >= kInf ? "∞" : DSTrace::num(d, 1); }

double exportDist(double d) { return d >= kInf ? -1.0 : d; }

std::vector<double> exportDistTable(const SearchState& st) {
    std::vector<double> out;
    for (double d : st.dist) out.push_back(exportDist(d));
    return out;
}

// O(path length): follow prev pointers back from dst, then reverse in place.
PathResult buildPath(const SearchState& st, int src, int dst) {
    PathResult r;
    if (st.dist[dst] >= kInf) return r;
    r.found = true;
    r.distance = st.dist[dst];
    for (int v = dst; v != -1; v = st.prevNode[v]) {
        r.nodes.push_back(v);
        if (st.prevRoad[v] != -1) r.roads.push_back(st.prevRoad[v]);
        if (v == src) break;
    }
    for (int i = 0, j = (int)r.nodes.size() - 1; i < j; ++i, --j) {
        int t = r.nodes[i]; r.nodes[i] = r.nodes[j]; r.nodes[j] = t;
    }
    for (int i = 0, j = (int)r.roads.size() - 1; i < j; ++i, --j) {
        int t = r.roads[i]; r.roads[i] = r.roads[j]; r.roads[j] = t;
    }
    return r;
}

// Core loop shared by every public entry point.
// Returns the first popped node satisfying isTarget, or -1.
int runSearch(const Graph& g, int src, const std::function<bool(int)>& isTarget,
              SearchState& st, DSTrace* trace, DijkstraTrace* rec) {
    int n = g.nodeCount();
    st.dist.assign(n, kInf);
    st.prevNode.assign(n, -1);
    st.prevRoad.assign(n, -1);
    st.settled.assign(n, false);

    MinHeap<HeapItem> heap;
    st.dist[src] = 0.0;
    heap.push(HeapItem{0.0, src});
    if (trace) trace->add("MinHeap", "push", "O(log n)", "Start at " + g.node(src).name + " (0.0 km)");

    HeapItem cur{0.0, 0};
    while (heap.popMin(cur)) {
        DijkstraStep step;
        step.popped = cur.node;
        step.poppedDist = cur.dist;
        step.stale = false;

        const std::string& curName = g.node(cur.node).name;
        if (st.settled[cur.node] || cur.dist > st.dist[cur.node]) {
            if (trace) trace->add("MinHeap", "popMin", "O(log n)", "Skipped stale entry " + curName + " (" + km(cur.dist) + " km)");
            if (rec) {
                step.stale = true;
                step.heap = heap.snapshot();
                step.dist = exportDistTable(st);
                step.explanation = "Popped " + curName + " (" + km(cur.dist) + " km) — stale entry, already settled at " + km(st.dist[cur.node]) + " km. Skip.";
                rec->steps.push_back(step);
            }
            continue;
        }
        st.settled[cur.node] = true;
        if (trace) trace->add("MinHeap", "popMin", "O(log n)", "Popped " + curName + " (dist " + km(cur.dist) + " km)");
        step.explanation = "Popped " + curName + " (" + km(cur.dist) + " km).";

        if (isTarget(cur.node)) {
            if (rec) {
                step.heap = heap.snapshot();
                step.dist = exportDistTable(st);
                step.explanation += " Target reached — stop.";
                rec->steps.push_back(step);
            }
            return cur.node;
        }

        if (trace) trace->add("Graph", "neighbors", "O(deg)", "Scan " + std::to_string(g.neighbors(cur.node).size()) + " roads of " + curName);
        std::string relaxText;
        for (const AdjEntry& e : g.neighbors(cur.node)) {
            int v = e.neighbor;
            if (st.settled[v]) continue;
            if (!g.isUsable(e.roadId)) {
                if (trace) trace->add("Graph", "skipBlocked", "O(1)", "Road " + curName + " - " + g.node(v).name + " is blocked");
                continue;
            }
            double nd = cur.dist + g.effectiveWeight(e.roadId);
            if (nd < st.dist[v] - 1e-9) {
                double old = st.dist[v];
                st.dist[v] = nd;
                st.prevNode[v] = cur.node;
                st.prevRoad[v] = e.roadId;
                heap.push(HeapItem{nd, v});
                if (trace) trace->add("MinHeap", "push", "O(log n)", "Relaxed " + g.node(v).name + ": " + km(old) + " → " + km(nd));
                step.relaxed.push_back(DijkstraRelax{cur.node, v, exportDist(old), nd});
                relaxText += (relaxText.empty() ? " Relaxed " : ", ") + g.node(v).name + ": " + km(old) + " → " + km(nd);
            }
        }
        if (rec) {
            step.explanation += relaxText.empty() ? " No distance improved." : relaxText + ".";
            step.heap = heap.snapshot();
            step.dist = exportDistTable(st);
            rec->steps.push_back(step);
        }
    }
    return -1;
}

}  // namespace

PathResult Dijkstra::shortestPath(const Graph& g, int src, int dst, DSTrace* trace) {
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return PathResult();
    SearchState st;
    std::function<bool(int)> isDst = [dst](int u) { return u == dst; };
    if (runSearch(g, src, isDst, st, trace, nullptr) == -1) {
        if (trace) trace->add("Dijkstra", "result", "O((V+E) log V)", g.node(dst).name + " is unreachable");
        return PathResult();
    }
    PathResult r = buildPath(st, src, dst);
    if (trace) trace->add("Dijkstra", "result", "O((V+E) log V)",
                          g.node(src).name + " → " + g.node(dst).name + " = " + km(r.distance) + " km, " +
                              std::to_string(r.nodes.size()) + " areas");
    return r;
}

NearestResult Dijkstra::findNearest(const Graph& g, int src, const std::function<bool(int)>& isTarget,
                                    DSTrace* trace) {
    NearestResult res;
    if (!g.isValidNode(src)) return res;
    SearchState st;
    int hit = runSearch(g, src, isTarget, st, trace, nullptr);
    if (hit == -1) return res;
    res.node = hit;
    res.path = buildPath(st, src, hit);
    return res;
}

DijkstraTrace Dijkstra::traceSteps(const Graph& g, int src, int dst) {
    DijkstraTrace rec;
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return rec;
    SearchState st;
    std::function<bool(int)> isDst = [dst](int u) { return u == dst; };
    if (runSearch(g, src, isDst, st, nullptr, &rec) != -1) rec.result = buildPath(st, src, dst);
    return rec;
}
