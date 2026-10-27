#include "core/Dijkstra.h"

using namespace std;

const double INF = 1e18;

static string kmText(double d) {
    if (d >= INF) return "∞";
    return DSTrace::num(d, 1);
}

PathResult emptyPath() {
    PathResult p;
    p.found = false;
    p.distance = 0;
    p.nodeCount = 0;
    return p;
}

DijkstraTrace::DijkstraTrace() {
    steps = NULL;
    stepCount = 0;
    capacity = 0;
    result = emptyPath();
}

DijkstraTrace::~DijkstraTrace() {
    for (int i = 0; i < stepCount; i++) delete[] steps[i].heap;
    delete[] steps;
}

void DijkstraTrace::init(int maxSteps) {
    steps = new DijkstraStep[maxSteps];
    capacity = maxSteps;
    stepCount = 0;
}

void Dijkstra::saveStep(DijkstraStep* step, const MinHeap& heap, double dist[], int n) {
    step->heapCount = heap.size();
    step->heap = new HeapItem[heap.size() + 1];
    for (int i = 0; i < heap.size(); i++) step->heap[i] = heap.itemAt(i);
    for (int i = 0; i < n; i++) step->dist[i] = dist[i] >= INF ? -1 : dist[i];
}

// follow prev links back from dst, then reverse
PathResult Dijkstra::buildPath(int src, int dst, double dist[], int prevNode[], int prevRoad[]) {
    PathResult p = emptyPath();
    if (dist[dst] >= INF) return p;
    p.found = true;
    p.distance = dist[dst];

    int backNodes[MAX_ROUTE];
    int backRoads[MAX_ROUTE];
    int n = 0;
    int v = dst;
    while (v != -1 && n < MAX_ROUTE) {
        backNodes[n] = v;
        backRoads[n] = prevRoad[v];
        n++;
        if (v == src) break;
        v = prevNode[v];
    }
    for (int i = 0; i < n; i++) p.nodes[i] = backNodes[n - 1 - i];
    for (int i = 0; i < n - 1; i++) p.roads[i] = backRoads[n - 2 - i];
    p.nodeCount = n;
    return p;
}

// dst >= 0: stop at dst. dst == -1: stop at the first area with a free driver of this type.
int Dijkstra::search(const Graph& g, int src, int dst, const HashTable* freeCount, RideType type,
                     double dist[], int prevNode[], int prevRoad[], DSTrace* trace, DijkstraTrace* rec) {
    int n = g.nodeCount();
    bool settled[MAX_NODES];
    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        prevNode[i] = -1;
        prevRoad[i] = -1;
        settled[i] = false;
    }

    MinHeap heap;
    dist[src] = 0;
    heap.push(makeHeapItem(0, src, src));
    if (trace) trace->add("MinHeap", "push", "O(log n)", "Start at " + g.getNode(src).name + " (0.0 km)");

    HeapItem cur;
    while (heap.popMin(cur)) {
        int u = cur.value;
        string uName = g.getNode(u).name;

        DijkstraStep* step = NULL;
        if (rec != NULL && rec->stepCount < rec->capacity) {
            step = &rec->steps[rec->stepCount];
            rec->stepCount++;
            step->popped = u;
            step->poppedDist = cur.key;
            step->stale = false;
            step->relaxCount = 0;
            step->heap = NULL;
            step->heapCount = 0;
        }

        if (settled[u] || cur.key > dist[u]) {
            if (trace) trace->add("MinHeap", "popMin", "O(log n)", "Skipped stale entry " + uName + " (" + kmText(cur.key) + " km)");
            if (step != NULL) {
                step->stale = true;
                step->explanation = "Popped " + uName + " (" + kmText(cur.key) + " km) — stale entry, already settled at " +
                                    kmText(dist[u]) + " km. Skip.";
                saveStep(step, heap, dist, n);
            }
            continue;
        }
        settled[u] = true;
        if (trace) trace->add("MinHeap", "popMin", "O(log n)", "Popped " + uName + " (dist " + kmText(cur.key) + " km)");
        string text = "Popped " + uName + " (" + kmText(cur.key) + " km).";

        bool isTarget = false;
        if (dst >= 0) {
            isTarget = (u == dst);
        } else {
            isTarget = freeCount->get(freeKey(u, type)) > 0;
            if (trace) {
                trace->add("HashTable", "find", "O(1) avg",
                           "Free " + rideTypeName(type) + " at " + uName + "? " + (isTarget ? "yes" : "no"));
            }
        }
        if (isTarget) {
            if (step != NULL) {
                step->explanation = text + " Target reached — stop.";
                saveStep(step, heap, dist, n);
            }
            return u;
        }

        if (trace) trace->add("Graph", "neighbors", "O(deg)", "Scan " + to_string(g.degree(u)) + " roads of " + uName);
        string relaxText = "";
        for (EdgeNode* e = g.neighbors(u); e != NULL; e = e->next) {
            int v = e->to;
            if (settled[v]) continue;
            if (!g.isUsable(e->roadId)) {
                if (trace) trace->add("Graph", "skipBlocked", "O(1)", "Road " + uName + " - " + g.getNode(v).name + " is blocked");
                continue;
            }
            double newDist = cur.key + g.effectiveWeight(e->roadId);
            if (newDist < dist[v] - 1e-9) {
                double oldDist = dist[v];
                dist[v] = newDist;
                prevNode[v] = u;
                prevRoad[v] = e->roadId;
                heap.push(makeHeapItem(newDist, v, v));
                if (trace) {
                    trace->add("MinHeap", "push", "O(log n)",
                               "Relaxed " + g.getNode(v).name + ": " + kmText(oldDist) + " → " + kmText(newDist));
                }
                if (step != NULL && step->relaxCount < MAX_NODES) {
                    RelaxInfo& r = step->relaxed[step->relaxCount];
                    r.from = u;
                    r.to = v;
                    r.oldDist = oldDist >= INF ? -1 : oldDist;
                    r.newDist = newDist;
                    step->relaxCount++;
                }
                if (relaxText.empty()) {
                    relaxText = " Relaxed ";
                } else {
                    relaxText += ", ";
                }
                relaxText += g.getNode(v).name + ": " + kmText(oldDist) + " → " + kmText(newDist);
            }
        }
        if (step != NULL) {
            if (relaxText.empty()) {
                step->explanation = text + " No distance improved.";
            } else {
                step->explanation = text + relaxText + ".";
            }
            saveStep(step, heap, dist, n);
        }
    }
    return -1;
}

PathResult Dijkstra::shortestPath(const Graph& g, int src, int dst, DSTrace* trace) {
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return emptyPath();
    double dist[MAX_NODES];
    int prevNode[MAX_NODES];
    int prevRoad[MAX_NODES];
    int hit = search(g, src, dst, NULL, RideType::MINI, dist, prevNode, prevRoad, trace, NULL);
    if (hit == -1) {
        if (trace) trace->add("Dijkstra", "result", "O((V+E) log V)", g.getNode(dst).name + " is unreachable");
        return emptyPath();
    }
    PathResult p = buildPath(src, dst, dist, prevNode, prevRoad);
    if (trace) {
        trace->add("Dijkstra", "result", "O((V+E) log V)",
                   g.getNode(src).name + " → " + g.getNode(dst).name + " = " + kmText(p.distance) + " km, " +
                       to_string(p.nodeCount) + " areas");
    }
    return p;
}

PathResult Dijkstra::findNearest(const Graph& g, int src, const HashTable& freeCount, RideType type,
                                 int& foundNode, DSTrace* trace) {
    foundNode = -1;
    if (!g.isValidNode(src)) return emptyPath();
    double dist[MAX_NODES];
    int prevNode[MAX_NODES];
    int prevRoad[MAX_NODES];
    int hit = search(g, src, -1, &freeCount, type, dist, prevNode, prevRoad, trace, NULL);
    if (hit == -1) return emptyPath();
    foundNode = hit;
    return buildPath(src, hit, dist, prevNode, prevRoad);
}

void Dijkstra::traceSteps(const Graph& g, int src, int dst, DijkstraTrace& out) {
    if (!g.isValidNode(src) || !g.isValidNode(dst)) return;
    out.init(2 * g.roadCount() + 2);
    double dist[MAX_NODES];
    int prevNode[MAX_NODES];
    int prevRoad[MAX_NODES];
    int hit = search(g, src, dst, NULL, RideType::MINI, dist, prevNode, prevRoad, NULL, &out);
    if (hit != -1) out.result = buildPath(src, dst, dist, prevNode, prevRoad);
}
