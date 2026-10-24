/*
 * KarachiChal console test runner — no external framework.
 *
 * TEST(name) { ... } registers a test automatically.
 * CHECK(cond) records a failure (with file:line) but keeps the test going.
 * Every dry-run case in docs/DRY_RUNS.md has a matching test here.
 */
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "ds/Graph.h"
#include "ds/MinHeap.h"
#include "core/Dijkstra.h"

// ---------------------------------------------------------------- framework
struct TestCase {
    const char* name;
    void (*fn)();
};

static std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) { registry().push_back(TestCase{name, fn}); }
};

static int g_failures = 0;

#define TEST(name)                                          \
    static void name();                                     \
    static TestRegistrar name##_registrar(#name, &name);    \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cout << "    check failed: " #cond " (" << __FILE__ << ":"      \
                      << __LINE__ << ")\n";                                      \
        }                                                                        \
    } while (0)

static bool nearlyEqual(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

int main() {
    int passed = 0, failed = 0;
    for (const TestCase& t : registry()) {
        int before = g_failures;
        t.fn();
        if (g_failures == before) {
            ++passed;
            std::cout << "PASS  " << t.name << "\n";
        } else {
            ++failed;
            std::cout << "FAIL  " << t.name << "\n";
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed, "
              << registry().size() << " total\n";
    return failed == 0 ? 0 : 1;
}

// ==================================================================== Graph
TEST(graph_add_nodes_and_edges) {
    Graph g;
    int a = g.addNode("A", 0, 0);
    int b = g.addNode("B", 0, 1);
    int c = g.addNode("C", 1, 1);
    CHECK(g.nodeCount() == 3);
    CHECK(g.addEdge(a, b, 2.5) == 0);
    CHECK(g.addEdge(b, c, 1.0) == 1);
    CHECK(g.roadCount() == 2);
    CHECK(g.neighbors(b).size() == 2);
    CHECK(g.findRoad(a, b) == 0);
    CHECK(g.findRoad(b, a) == 0);   // undirected
    CHECK(g.findRoad(a, c) == -1);
}

TEST(graph_rejects_bad_edges) {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 0, 1);
    CHECK(g.addEdge(0, 0, 1.0) == -1);    // self loop
    CHECK(g.addEdge(0, 5, 1.0) == -1);    // unknown node
    CHECK(g.addEdge(-1, 1, 1.0) == -1);   // negative id
    CHECK(g.addEdge(0, 1, 0.0) == -1);    // zero distance
    CHECK(g.addEdge(0, 1, -3.0) == -1);   // negative distance
    CHECK(g.addEdge(0, 1, 2.0) == 0);
    CHECK(g.addEdge(1, 0, 2.0) == -1);    // duplicate road
    CHECK(g.roadCount() == 1);
}

TEST(graph_traffic_and_blocking) {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 0, 1);
    g.addEdge(0, 1, 2.0);
    CHECK(g.setTraffic(0, 1, 2.0));
    CHECK(nearlyEqual(g.effectiveWeight(0), 4.0));
    CHECK(!g.setTraffic(0, 1, 0.0));      // zero factor
    CHECK(!g.setTraffic(0, 1, -1.0));     // negative factor
    CHECK(!g.setTraffic(0, 1, 3.5));      // above max
    CHECK(nearlyEqual(g.effectiveWeight(0), 4.0));
    CHECK(g.setBlocked(1, 0, true));
    CHECK(!g.isUsable(0));
    CHECK(g.setBlocked(0, 1, true));      // idempotent
    CHECK(g.setBlocked(0, 1, false));
    CHECK(g.isUsable(0));
    CHECK(!g.setBlocked(0, 7, true));     // no such road
}

TEST(graph_empty_and_invalid_queries) {
    Graph g;
    CHECK(g.nodeCount() == 0);
    CHECK(g.neighbors(0).empty());
    CHECK(g.neighbors(-4).empty());
    CHECK(g.findRoad(0, 1) == -1);
    CHECK(!g.isUsable(0));
    CHECK(nearlyEqual(g.effectiveWeight(3), 0.0));
}

// ================================================================== MinHeap
// Dry run "MinHeap case 1" in docs/DRY_RUNS.md
TEST(heap_push_pop_order) {
    MinHeap<int> h;
    int input[] = {5, 3, 8, 1, 9, 2};
    for (int x : input) h.push(x);
    CHECK(h.size() == 6);
    CHECK(h.isValidHeap());
    // after pushes the array is [1, 3, 2, 5, 9, 8]
    const std::vector<int>& arr = h.snapshot();
    int expected[] = {1, 3, 2, 5, 9, 8};
    for (int i = 0; i < 6; ++i) CHECK(arr[i] == expected[i]);
    int out = 0, last = -1000;
    while (h.popMin(out)) {
        CHECK(out >= last);
        last = out;
    }
    CHECK(h.isEmpty());
}

// Dry run "MinHeap case 2": pop from an empty / single-element heap
TEST(heap_empty_and_single) {
    MinHeap<int> h;
    int out = 42;
    CHECK(!h.popMin(out));
    CHECK(out == 42);          // untouched
    CHECK(!h.peek(out));
    h.push(7);
    CHECK(h.peek(out) && out == 7);
    CHECK(h.popMin(out) && out == 7);
    CHECK(!h.popMin(out));
}

// Dry run "MinHeap case 3": duplicates and negatives
TEST(heap_duplicates_and_negatives) {
    MinHeap<int> h;
    int input[] = {4, -2, 4, 0, -2};
    for (int x : input) h.push(x);
    int expected[] = {-2, -2, 0, 4, 4};
    int out = 0;
    for (int i = 0; i < 5; ++i) CHECK(h.popMin(out) && out == expected[i]);
}

struct MaxFirst {
    bool operator()(int a, int b) const { return a > b; }
};

TEST(heap_custom_comparator) {
    MinHeap<int, MaxFirst> h;
    h.push(1); h.push(10); h.push(5);
    int out = 0;
    CHECK(h.popMin(out) && out == 10);
}

TEST(heap_large_input_10k) {
    MinHeap<int> h;
    unsigned int seed = 12345;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1103515245u + 12345u;
        h.push((int)(seed % 100000) - 50000);
    }
    CHECK(h.size() == 10000);
    CHECK(h.isValidHeap());
    int out = 0, last = -1000000, count = 0;
    bool sorted = true;
    while (h.popMin(out)) {
        if (out < last) sorted = false;
        last = out;
        ++count;
    }
    CHECK(sorted);
    CHECK(count == 10000);
}

// ================================================================= Dijkstra
// Shared 6-node graph used by every Dijkstra dry run in docs/DRY_RUNS.md:
//   A-B 4, A-C 2, B-C 1, B-D 5, C-D 8, C-E 10, D-E 2, D-F 6, E-F 3
static Graph makeDryRunGraph() {
    Graph g;
    const char* names[] = {"A", "B", "C", "D", "E", "F"};
    for (int i = 0; i < 6; ++i) g.addNode(names[i], 0, i);
    g.addEdge(0, 1, 4);  g.addEdge(0, 2, 2);  g.addEdge(1, 2, 1);
    g.addEdge(1, 3, 5);  g.addEdge(2, 3, 8);  g.addEdge(2, 4, 10);
    g.addEdge(3, 4, 2);  g.addEdge(3, 5, 6);  g.addEdge(4, 5, 3);
    return g;
}

static bool pathIs(const PathResult& r, const std::vector<int>& expected) {
    if (r.nodes.size() != expected.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) if (r.nodes[i] != expected[i]) return false;
    return true;
}

// Dijkstra case 1: A -> F, expected A C B D E F = 13
TEST(dijkstra_case1_basic_shortest_path) {
    Graph g = makeDryRunGraph();
    PathResult r = Dijkstra::shortestPath(g, 0, 5);
    CHECK(r.found);
    CHECK(nearlyEqual(r.distance, 13.0));
    CHECK(pathIs(r, {0, 2, 1, 3, 4, 5}));
    CHECK(r.roads.size() == r.nodes.size() - 1);
}

// Dijkstra case 2: block D-E, A -> E becomes A C E = 12
TEST(dijkstra_case2_blocked_edge) {
    Graph g = makeDryRunGraph();
    CHECK(g.setBlocked(3, 4, true));
    PathResult r = Dijkstra::shortestPath(g, 0, 4);
    CHECK(r.found);
    CHECK(nearlyEqual(r.distance, 12.0));
    CHECK(pathIs(r, {0, 2, 4}));
}

// Dijkstra case 3: isolated node G is unreachable
TEST(dijkstra_case3_unreachable) {
    Graph g = makeDryRunGraph();
    int gNode = g.addNode("G", 9, 9);
    PathResult r = Dijkstra::shortestPath(g, 0, gNode);
    CHECK(!r.found);
    CHECK(r.nodes.empty());
}

// Dijkstra case 4: traffic x3 on B-C, A -> D becomes A B D = 9
TEST(dijkstra_case4_traffic_changes_route) {
    Graph g = makeDryRunGraph();
    CHECK(g.setTraffic(1, 2, 3.0));
    PathResult r = Dijkstra::shortestPath(g, 0, 3);
    CHECK(r.found);
    CHECK(nearlyEqual(r.distance, 9.0));
    CHECK(pathIs(r, {0, 1, 3}));
}

TEST(dijkstra_same_node_and_invalid) {
    Graph g = makeDryRunGraph();
    PathResult same = Dijkstra::shortestPath(g, 2, 2);
    CHECK(same.found && nearlyEqual(same.distance, 0.0) && same.nodes.size() == 1);
    CHECK(!Dijkstra::shortestPath(g, -1, 2).found);
    CHECK(!Dijkstra::shortestPath(g, 0, 99).found);
    Graph empty;
    CHECK(!Dijkstra::shortestPath(empty, 0, 0).found);
}

TEST(dijkstra_find_nearest_target) {
    Graph g = makeDryRunGraph();
    // drivers waiting at D and E: from A, D (8) is closer than E (10)
    NearestResult r = Dijkstra::findNearest(g, 0, [](int u) { return u == 3 || u == 4; });
    CHECK(r.node == 3);
    CHECK(nearlyEqual(r.path.distance, 8.0));
    NearestResult none = Dijkstra::findNearest(g, 0, [](int) { return false; });
    CHECK(none.node == -1);
}

TEST(dijkstra_trace_steps_record) {
    Graph g = makeDryRunGraph();
    DijkstraTrace t = Dijkstra::traceSteps(g, 0, 5);
    CHECK(t.result.found && nearlyEqual(t.result.distance, 13.0));
    // pops: A0 C2 B3 B4(stale) D8 D10(stale) E10 E12(stale) F13
    CHECK(t.steps.size() == 9);
    CHECK(t.steps[0].popped == 0 && t.steps[0].relaxed.size() == 2);
    CHECK(t.steps[3].stale);
    CHECK(t.steps.back().popped == 5);
    DSTrace trace;
    Dijkstra::shortestPath(g, 0, 5, &trace);
    CHECK(trace.size() > 0 && !trace.truncated());
}
