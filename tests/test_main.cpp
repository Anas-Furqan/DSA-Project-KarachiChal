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
#include "core/BFS.h"
#include "ds/LinkedQueue.h"
#include "ds/HashTable.h"
#include "ds/Trie.h"
#include "ds/AVLTree.h"
#include "ds/LinkedList.h"
#include "core/CityMap.h"
#include "core/Models.h"
#include "core/SurgeEngine.h"
#include "core/Dispatcher.h"
#include "core/Simulator.h"

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

// ============================================================== LinkedQueue
struct Req {
    int id;
    const char* rider;
};

static std::vector<int> queueIds(const LinkedQueue<Req>& q) {
    std::vector<int> ids;
    q.forEach([&ids](const Req& r) { ids.push_back(r.id); });
    return ids;
}

// Queue case 1: plain FIFO order
TEST(queue_case1_fifo) {
    LinkedQueue<int> q;
    q.enqueue(10); q.enqueue(20); q.enqueue(30);
    CHECK(q.size() == 3);
    int out = 0;
    CHECK(q.peekFront(out) && out == 10);
    CHECK(q.dequeue(out) && out == 10);
    CHECK(q.dequeue(out) && out == 20);
    q.enqueue(40);
    CHECK(q.dequeue(out) && out == 30);
    CHECK(q.dequeue(out) && out == 40);
    CHECK(q.isEmpty());
}

// Queue case 2: cancel from front, middle and rear
TEST(queue_case2_cancel_by_id) {
    LinkedQueue<Req> q;
    q.enqueue(Req{101, "Ali"});
    q.enqueue(Req{102, "Sara"});
    q.enqueue(Req{103, "Bilal"});
    q.enqueue(Req{104, "Hina"});
    CHECK(q.removeById(102));                       // middle
    CHECK(queueIds(q) == std::vector<int>({101, 103, 104}));
    CHECK(q.removeById(104));                       // rear (tail must move)
    q.enqueue(Req{105, "Omar"});
    CHECK(queueIds(q) == std::vector<int>({101, 103, 105}));
    CHECK(q.removeById(101));                       // front
    CHECK(!q.removeById(999));                      // not present
    CHECK(queueIds(q) == std::vector<int>({103, 105}));
    CHECK(q.size() == 2);
}

// Queue case 3: empty queue is safe; removing the only element resets tail
TEST(queue_case3_empty_and_single) {
    LinkedQueue<Req> q;
    Req out{0, ""};
    CHECK(!q.dequeue(out));
    CHECK(!q.peekFront(out));
    CHECK(!q.removeById(1));
    q.enqueue(Req{7, "Zara"});
    CHECK(q.removeById(7));
    CHECK(q.isEmpty());
    q.enqueue(Req{8, "Asad"});                      // tail was reset correctly
    CHECK(q.dequeue(out) && out.id == 8);
}

TEST(queue_copy_is_deep) {
    LinkedQueue<int> a;
    a.enqueue(1); a.enqueue(2);
    LinkedQueue<int> b = a;
    int out = 0;
    b.dequeue(out);
    CHECK(a.size() == 2 && b.size() == 1);
    a = b;
    CHECK(a.size() == 1);
}

TEST(queue_large_input_10k) {
    LinkedQueue<int> q;
    for (int i = 0; i < 10000; ++i) q.enqueue(i);
    CHECK(q.size() == 10000);
    int out = -1;
    bool inOrder = true;
    for (int i = 0; i < 10000; ++i) {
        if (!q.dequeue(out) || out != i) inOrder = false;
    }
    CHECK(inOrder && q.isEmpty());
}

// ====================================================================== BFS
TEST(bfs_hops_and_reachability) {
    Graph g = makeDryRunGraph();
    std::vector<int> hops = BFS::hopsFrom(g, 0, 2);
    // A:0  B,C:1  D,E:2  F:beyond 2 hops
    CHECK(hops[0] == 0 && hops[1] == 1 && hops[2] == 1);
    CHECK(hops[3] == 2 && hops[4] == 2 && hops[5] == -1);
    CHECK(BFS::isReachable(g, 0, 5));
    int gNode = g.addNode("G", 0, 0);
    CHECK(!BFS::isReachable(g, 0, gNode));
    CHECK(!BFS::isReachable(g, 0, 50));
}

TEST(bfs_blocked_roads_disconnect) {
    Graph g = makeDryRunGraph();
    // cut F off: block D-F and E-F
    g.setBlocked(3, 5, true);
    g.setBlocked(4, 5, true);
    CHECK(!BFS::isReachable(g, 0, 5));
    g.setBlocked(4, 5, false);
    CHECK(BFS::isReachable(g, 0, 5));
    int sum = BFS::sumWithinHops(g, 0, 1, [](int) { return 1; });
    CHECK(sum == 3);   // A, B, C
}

// ================================================================ HashTable
TEST(hash_insert_find_erase) {
    HashTable<int, std::string> t;
    CHECK(t.insert(7, "Ali"));
    CHECK(t.insert(15, "Sara"));
    CHECK(!t.insert(7, "Ali K"));        // duplicate key overwrites
    CHECK(t.size() == 2);
    CHECK(t.find(7) && *t.find(7) == "Ali K");
    CHECK(t.find(99) == nullptr);
    CHECK(t.erase(7));
    CHECK(!t.erase(7));                  // already gone
    CHECK(!t.contains(7) && t.contains(15));
    CHECK(t.size() == 1);
}

TEST(hash_string_keys_and_negatives) {
    HashTable<std::string, int> names;
    names.insert("saddar", 0);
    names.insert("clifton", 1);
    names.insert("", 2);                 // empty string is a valid key
    CHECK(*names.find("clifton") == 1);
    CHECK(*names.find("") == 2);
    CHECK(names.find("Clifton") == nullptr);   // case sensitive by design
    HashTable<int, int> neg;
    neg.insert(-5, 1);
    neg.insert(-2147483647, 2);
    CHECK(*neg.find(-5) == 1 && *neg.find(-2147483647) == 2);
}

TEST(hash_rehash_10k) {
    HashTable<int, int> t(4);
    for (int i = 0; i < 10000; ++i) t.insert(i, i * 2);
    CHECK(t.size() == 10000);
    CHECK(t.rehashCount() > 0);
    const double maxLoad = HashTable<int, int>::kMaxLoad;
    CHECK(t.loadFactor() <= maxLoad);
    bool allFound = true;
    for (int i = 0; i < 10000; ++i) {
        const int* v = t.find(i);
        if (!v || *v != i * 2) allFound = false;
    }
    CHECK(allFound);
    CHECK(t.longestChain() < 12);        // own hash spreads keys well
    for (int i = 0; i < 10000; i += 2) t.erase(i);
    CHECK(t.size() == 5000 && !t.contains(4) && t.contains(5));
}

struct CollideHasher {
    unsigned int operator()(int) const { return 0; }   // every key -> bucket 0
};

TEST(hash_many_collisions_still_correct) {
    HashTable<int, int, CollideHasher> t;
    for (int i = 0; i < 500; ++i) t.insert(i, -i);
    CHECK(t.size() == 500);
    CHECK(t.longestChain() == 500);
    CHECK(*t.find(250) == -250);
    CHECK(t.erase(0) && t.erase(499) && t.erase(250));
    CHECK(t.size() == 497 && !t.contains(250));
}

TEST(hash_get_or_insert_and_copy) {
    HashTable<int, int> t;
    t.getOrInsert(3, 0) += 5;
    t.getOrInsert(3, 0) += 5;
    CHECK(*t.find(3) == 10);
    HashTable<int, int> copy = t;
    copy.insert(4, 1);
    CHECK(t.size() == 1 && copy.size() == 2);
    int sum = 0;
    copy.forEach([&sum](int, int& v) { sum += v; });
    CHECK(sum == 11);
}

// ===================================================================== Trie
static void fillDryRunTrie(Trie& t) {
    t.insert("Gulshan-e-Iqbal", 8);
    t.insert("Gulistan-e-Johar", 9);
    t.insert("Garden", 30);
    t.insert("Saddar", 0);
}

// Trie case 1: prefix shared by two names, alphabetical order
TEST(trie_case1_shared_prefix) {
    Trie t;
    fillDryRunTrie(t);
    std::vector<TrieMatch> r = t.startsWith("gul");
    CHECK(r.size() == 2);
    CHECK(r.size() == 2 && r[0].word == "Gulistan-e-Johar" && r[0].value == 9);
    CHECK(r.size() == 2 && r[1].word == "Gulshan-e-Iqbal" && r[1].value == 8);
    CHECK(t.startsWith("g").size() == 3);
}

// Trie case 2: case-insensitive, narrows to one
TEST(trie_case2_case_insensitive) {
    Trie t;
    fillDryRunTrie(t);
    std::vector<TrieMatch> r = t.startsWith("GULS");
    CHECK(r.size() == 1 && r[0].value == 8);
    CHECK(t.contains("saddar") && t.contains("SADDAR"));
    CHECK(!t.contains("sadd"));
}

// Trie case 3: prefix with no matches
TEST(trie_case3_no_match) {
    Trie t;
    fillDryRunTrie(t);
    CHECK(t.startsWith("xyz").empty());
    CHECK(t.startsWith("gulz").empty());
    CHECK(t.matchedLength("gulz") == 3);   // g-u-l exist, z does not
}

TEST(trie_edge_inputs) {
    Trie t;
    fillDryRunTrie(t);
    CHECK(t.startsWith("").empty());               // empty query -> empty list
    CHECK(t.startsWith("@#$").empty());            // special characters
    CHECK(t.startsWith(std::string(500, 'g')).empty());   // very long query
    CHECK(!t.insert("", 1));
    CHECK(!t.insert("bad*name", 1));
    CHECK(t.wordCount() == 4);
    Trie empty;
    CHECK(empty.startsWith("a").empty());
}

TEST(trie_limit_and_dedupe) {
    Trie t;
    const char* names[] = {"Area 1", "Area 2", "Area 3", "Area 4", "Area 5", "Area 6", "Area 7"};
    for (int i = 0; i < 7; ++i) t.insert(names[i], i);
    CHECK(t.startsWith("area").size() == 5);       // capped at 5
    CHECK(t.startsWith("area", 10).size() == 7);
    Trie alias;
    alias.insert("Gulshan-e-Iqbal", 8);
    alias.insert("iqbal", 8, "Gulshan-e-Iqbal");   // alias, same value
    alias.insert("gulshan", 8, "Gulshan-e-Iqbal");
    CHECK(alias.startsWith("gul").size() == 1);    // de-duplicated by value
    CHECK(alias.startsWith("iq").size() == 1 && alias.startsWith("iq")[0].word == "Gulshan-e-Iqbal");
}

TEST(trie_branch_view) {
    Trie t;
    fillDryRunTrie(t);
    std::vector<TrieViewNode> v = t.branchView("gu");
    int onPath = 0;
    for (const TrieViewNode& n : v) if (n.onPath) ++onPath;
    CHECK(onPath == 3);                            // root, g, u
    CHECK(v.size() > 3);
    CHECK(t.branchView("").size() == 1);
}

// ================================================================== AVLTree
// Leaderboard order is rating DESC, so a higher rating is a "smaller" key.
// Inserting ratings 3.00, 2.00, 1.00 is like inserting 1, 2, 3 in a normal BST.
static DriverKey dk(int ratingCenti, int id) { return DriverKey{ratingCenti, id}; }

// AVL case 1 (RR): 300, 200, 100 -> left rotate at 300
TEST(avl_case1_rr_rotation) {
    AVLTree t;
    t.insert(dk(300, 1));
    t.insert(dk(200, 2));
    CHECK(t.lastRotations().empty());
    t.insert(dk(100, 3));
    CHECK(t.rotationCount("RR") == 1);
    CHECK(t.height() == 2 && t.isValidAVL());
    std::vector<AVLViewNode> v = t.view();
    CHECK(v[0].key.driverId == 2);       // new root is 2.00
}

// AVL case 2 (LL): 100, 200, 300 -> right rotate at 100
TEST(avl_case2_ll_rotation) {
    AVLTree t;
    t.insert(dk(100, 1));
    t.insert(dk(200, 2));
    t.insert(dk(300, 3));
    CHECK(t.rotationCount("LL") == 1);
    CHECK(t.view()[0].key.driverId == 2 && t.isValidAVL());
}

// AVL case 3 (LR): 100, 300, 200 -> left rotate at 300, right rotate at 100
TEST(avl_case3_lr_rotation) {
    AVLTree t;
    t.insert(dk(100, 1));
    t.insert(dk(300, 3));
    t.insert(dk(200, 2));
    CHECK(t.rotationCount("LR") == 1);
    CHECK(t.view()[0].key.driverId == 2 && t.isValidAVL());
}

// AVL case 4 (RL): 300, 100, 200 -> right rotate at 100, left rotate at 300
TEST(avl_case4_rl_rotation) {
    AVLTree t;
    t.insert(dk(300, 3));
    t.insert(dk(100, 1));
    t.insert(dk(200, 2));
    CHECK(t.rotationCount("RL") == 1);
    CHECK(t.view()[0].key.driverId == 2 && t.isValidAVL());
}

TEST(avl_duplicate_ratings_tie_break) {
    AVLTree t;
    t.insert(dk(450, 7));
    t.insert(dk(450, 3));
    t.insert(dk(450, 5));
    t.insert(dk(490, 9));
    CHECK(!t.insert(dk(450, 3)));         // exact duplicate key rejected
    CHECK(t.size() == 4);
    std::vector<DriverKey> top = t.topN(10);
    CHECK(top.size() == 4);
    CHECK(top[0].driverId == 9 && top[1].driverId == 3 && top[2].driverId == 5 && top[3].driverId == 7);
}

TEST(avl_delete_and_update_rating) {
    AVLTree t;
    for (int i = 1; i <= 7; ++i) t.insert(dk(400 + i * 10, i));
    CHECK(!t.remove(dk(999, 1)));         // non-existent key
    CHECK(!t.remove(dk(410, 2)));         // right id, wrong rating
    CHECK(t.remove(dk(440, 4)));          // node with two children
    CHECK(t.size() == 6 && t.isValidAVL() && !t.contains(dk(440, 4)));
    // rating update for driver 1: 4.10 -> 4.95 = delete + reinsert
    CHECK(t.remove(dk(410, 1)));
    CHECK(t.insert(dk(495, 1)));
    CHECK(t.topN(1)[0].driverId == 1);
    AVLTree empty;
    CHECK(!empty.remove(dk(1, 1)));
    CHECK(empty.topN(5).empty() && empty.height() == 0);
}

TEST(avl_large_input_stays_balanced) {
    AVLTree t;
    for (int i = 0; i < 10000; ++i) t.insert(dk(i, i));   // sorted input: worst case for a plain BST
    CHECK(t.size() == 10000);
    CHECK(t.isValidAVL());
    CHECK(t.height() <= 20);              // 1.44 * log2(10000) ~ 19
    std::vector<DriverKey> top = t.topN(3);
    CHECK(top.size() == 3 && top[0].ratingCenti == 9999 && top[2].ratingCenti == 9997);
    for (int i = 0; i < 10000; i += 3) t.remove(dk(i, i));
    CHECK(t.isValidAVL() && t.size() == 6666);
}

// =============================================================== LinkedList
static std::vector<int> listValues(const LinkedList<int>& l) {
    std::vector<int> out;
    l.forEach([&out](int v) { out.push_back(v); });
    return out;
}

TEST(list_push_front_newest_first) {
    LinkedList<int> history;
    history.pushFront(1001);
    history.pushFront(1002);
    history.pushFront(1003);
    CHECK(listValues(history) == std::vector<int>({1003, 1002, 1001}));
    int out = 0;
    CHECK(history.front(out) && out == 1003);
    CHECK(history.size() == 3);
}

TEST(list_remove_and_empty) {
    LinkedList<int> l;
    int out = 0;
    CHECK(!l.popFront(out) && !l.front(out));
    CHECK(!l.remove(5));
    l.pushFront(3); l.pushFront(2); l.pushFront(2); l.pushFront(1);
    CHECK(l.remove(2));                   // removes first of the duplicates
    CHECK(listValues(l) == std::vector<int>({1, 2, 3}));
    CHECK(l.remove(3) && l.remove(1) && l.contains(2) && l.size() == 1);
    CHECK(l.popFront(out) && out == 2 && l.isEmpty());
}

TEST(list_copy_and_large) {
    LinkedList<int> a;
    for (int i = 0; i < 10000; ++i) a.pushFront(i);
    LinkedList<int> b = a;
    CHECK(b.size() == 10000);
    int out = -1;
    CHECK(b.front(out) && out == 9999);
    b.clear();
    CHECK(a.size() == 10000 && b.isEmpty());
}

// ================================================================== CityMap
static const char* kTinyMap =
    "{\"areas\":[{\"id\":0,\"name\":\"Saddar\",\"lat\":24.85,\"lng\":67.02},"
    "{\"id\":1,\"name\":\"Gulshan-e-Iqbal\",\"lat\":24.92,\"lng\":67.08},"
    "{\"id\":2,\"name\":\"Clifton\",\"lat\":24.81,\"lng\":67.03}],"
    "\"roads\":[{\"from\":0,\"to\":1,\"km\":9.5},{\"from\":0,\"to\":2,\"km\":5.1}]}";

TEST(citymap_loads_valid_json) {
    CityMap m;
    std::string err;
    CHECK(m.loadFromJsonText(kTinyMap, err));
    CHECK(err.empty());
    CHECK(m.areaCount() == 3 && m.graph().roadCount() == 2);
    CHECK(m.findAreaByName("  SADDAR ") == 0);
    CHECK(m.findAreaByName("Nowhere") == -1);
    CHECK(m.search("iqb").size() == 1);           // alias token
    CHECK(m.search("").empty());
    CHECK(m.search(std::string(200, 'a')).empty());
}

TEST(citymap_rejects_bad_files) {
    CityMap m;
    std::string err;
    CHECK(!m.loadFromFile("data/does_not_exist.json", err) && !err.empty());
    CHECK(!m.loadFromJsonText("{not json", err));
    CHECK(!m.loadFromJsonText("{\"areas\":[]}", err));
    CHECK(!m.loadFromJsonText("{\"areas\":[],\"roads\":[]}", err));
    // duplicate name (case-insensitive)
    CHECK(!m.loadFromJsonText("{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":0,\"lng\":0},"
                              "{\"id\":1,\"name\":\"a\",\"lat\":0,\"lng\":0}],\"roads\":[]}", err));
    CHECK(err.find("Duplicate area name") != std::string::npos);
    // road to unknown area
    CHECK(!m.loadFromJsonText("{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":0,\"lng\":0}],"
                              "\"roads\":[{\"from\":0,\"to\":9,\"km\":1}]}", err));
    CHECK(err.find("unknown area") != std::string::npos);
    // negative distance
    CHECK(!m.loadFromJsonText("{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":0,\"lng\":0},"
                              "{\"id\":1,\"name\":\"B\",\"lat\":0,\"lng\":0}],"
                              "\"roads\":[{\"from\":0,\"to\":1,\"km\":-2}]}", err));
    CHECK(err.find("non-positive") != std::string::npos);
    CHECK(m.areaCount() == 0);                    // failed load leaves map empty
}

TEST(citymap_real_karachi_file) {
    CityMap m;
    std::string err;
    CHECK(m.loadFromFile("data/karachi_map.json", err));
    CHECK(m.areaCount() >= 28 && m.areaCount() <= 30);
    int saddar = m.findAreaByName("Saddar");
    int airport = m.findAreaByName("Jinnah Airport");
    CHECK(saddar >= 0 && airport >= 0);
    PathResult r = Dijkstra::shortestPath(m.graph(), saddar, airport);
    CHECK(r.found && r.distance > 0);
    // the whole city is connected
    std::vector<int> hops = BFS::hopsFrom(m.graph(), 0, -1);
    bool connected = true;
    for (int h : hops) if (h == -1) connected = false;
    CHECK(connected);
}

// =================================================================== Models
TEST(models_fare_and_ride_type) {
    CHECK(computeFare(RideType::BIKE, 10.0, 1.0) == 310);    // 60 + 25*10
    CHECK(computeFare(RideType::MINI, 10.0, 1.0) == 520);    // 120 + 40*10
    CHECK(computeFare(RideType::AC, 10.0, 1.5) == 1095);     // (180 + 550) * 1.5
    CHECK(computeFare(RideType::MINI, 0.0, 1.0) == 120);
    RideType t = RideType::BIKE;
    CHECK(parseRideType("ac", t) && t == RideType::AC);
    CHECK(parseRideType("Mini", t) && t == RideType::MINI);
    CHECK(!parseRideType("RICKSHAW", t) && !parseRideType("", t));
}

TEST(models_event_order_ties) {
    MinHeap<Event> h;
    h.push(Event{5.0, EventType::RIDE_COMPLETED, 1, 0, 2});
    h.push(Event{5.0, EventType::DRIVER_FREE, 2, 0, 1});
    h.push(Event{1.0, EventType::DRIVER_ARRIVED_PICKUP, 3, 0, 3});
    Event e{};
    CHECK(h.popMin(e) && e.rideId == 3);
    CHECK(h.popMin(e) && e.seq == 1);     // same time -> earlier seq first
    CHECK(h.popMin(e) && e.seq == 2);
}

// ==================================================================== Surge
TEST(surge_multiplier_steps) {
    CHECK(SurgeEngine::multiplierFor(0, 0) == 1.0);
    CHECK(SurgeEngine::multiplierFor(1, 3) == 1.0);    // ratio 0.33
    CHECK(SurgeEngine::multiplierFor(3, 2) == 1.25);   // 1.5
    CHECK(SurgeEngine::multiplierFor(5, 2) == 1.5);    // 2.5
    CHECK(SurgeEngine::multiplierFor(4, 0) == 2.0);    // no drivers -> max(0,1)
}

TEST(surge_bfs_two_hops) {
    Graph g = makeDryRunGraph();
    SurgeEngine s;
    // 3 requests at A; free drivers: 1 at F (3 hops away, ignored), 1 at C
    s.recompute(g, [](int a) { return a == 0 ? 3 : 0; },
                [](int a) { return (a == 5 || a == 2) ? 1 : 0; });
    CHECK(s.infoAt(0).supply == 1);
    CHECK(s.multiplierAt(0) == 2.0);
    CHECK(s.multiplierAt(1) == 1.0);
    CHECK(s.multiplierAt(99) == 1.0);
}

// =============================================================== Dispatcher
// Same 6-node graph as the Dijkstra dry runs, as a map file.
static const char* kDryRunMapJson = R"({"areas":[
  {"id":0,"name":"A","lat":24.80,"lng":67.00},{"id":1,"name":"B","lat":24.82,"lng":67.02},
  {"id":2,"name":"C","lat":24.81,"lng":67.01},{"id":3,"name":"D","lat":24.84,"lng":67.03},
  {"id":4,"name":"E","lat":24.85,"lng":67.04},{"id":5,"name":"F","lat":24.87,"lng":67.05}],
 "roads":[{"from":0,"to":1,"km":4},{"from":0,"to":2,"km":2},{"from":1,"to":2,"km":1},
  {"from":1,"to":3,"km":5},{"from":2,"to":3,"km":8},{"from":2,"to":4,"km":10},
  {"from":3,"to":4,"km":2},{"from":3,"to":5,"km":6},{"from":4,"to":5,"km":3}]})";

struct DispatchFixture {
    CityMap map;
    MinHeap<Event> events;
    SimClock clock;
    Dispatcher disp;
    DSTrace trace;

    DispatchFixture() : disp(map, events, clock) {
        std::string err;
        map.loadFromJsonText(kDryRunMapJson, err);
        disp.reset(0, 1);
    }
    // pop and handle every event with time <= t, like Simulator does
    void runUntil(double t) {
        Event e{};
        while (events.peek(e) && e.time <= t) {
            events.popMin(e);
            if (e.time > clock.now) clock.now = e.time;
            disp.handleEvent(e, trace);
        }
        clock.now = t;
    }
    RideStatus status(int rideId) const { return disp.findRide(rideId)->status; }
};

TEST(dispatch_nearest_driver_and_fare) {
    DispatchFixture f;
    int dD = f.disp.addDriver("Driver D", RideType::MINI, 3, 450);
    f.disp.addDriver("Driver E", RideType::MINI, 4, 490);
    f.disp.addDriver("Bike B", RideType::BIKE, 1, 500);   // wrong type, ignored
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    CHECK(r.ok);
    const Ride* ride = f.disp.findRide(r.rideId);
    CHECK(ride->status == RideStatus::ASSIGNED);
    CHECK(ride->driverId == dD);               // D is 8 km away, E is 10 km
    CHECK(ride->fare == 640);                  // 120 + 40 * 13 km
    CHECK(nearlyEqual(ride->distanceKm, 13.0));
    CHECK(f.trace.size() > 0);
}

TEST(dispatch_same_area_picks_best_rating) {
    DispatchFixture f;
    f.disp.addDriver("Low", RideType::AC, 2, 380);
    int best = f.disp.addDriver("High", RideType::AC, 2, 495);
    f.disp.addDriver("Mid", RideType::AC, 2, 420);
    OpResult r = f.disp.requestRide("Sara", 0, 3, RideType::AC, false, f.trace);
    CHECK(r.ok && f.disp.findRide(r.rideId)->driverId == best);
}

TEST(dispatch_validation_errors) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    CHECK(f.disp.addDriver("Bad", RideType::MINI, 77, 450) == -1);
    CHECK(f.disp.requestRide("", 0, 1, RideType::MINI, false, f.trace).code == "MISSING_FIELD");
    CHECK(f.disp.requestRide("   ", 0, 1, RideType::MINI, false, f.trace).code == "MISSING_FIELD");
    CHECK(f.disp.requestRide(std::string(41, 'x'), 0, 1, RideType::MINI, false, f.trace).code == "INVALID_NAME");
    CHECK(f.disp.requestRide("Ali", -1, 1, RideType::MINI, false, f.trace).code == "INVALID_AREA");
    CHECK(f.disp.requestRide("Ali", 0, 99, RideType::MINI, false, f.trace).code == "INVALID_AREA");
    OpResult same = f.disp.requestRide("Ali", 2, 2, RideType::MINI, false, f.trace);
    CHECK(!same.ok && same.code == "SAME_LOCATION" && same.httpStatus == 400);
    CHECK(f.disp.requestRide("Ali", 0, 1, RideType::MINI, false, f.trace).ok);
    OpResult dup = f.disp.requestRide(" ali ", 0, 2, RideType::MINI, false, f.trace);   // same rider, other casing
    CHECK(!dup.ok && dup.httpStatus == 409 && dup.code == "DUPLICATE_REQUEST");
}

TEST(dispatch_no_driver_goes_to_queue_then_served) {
    DispatchFixture f;
    OpResult none = f.disp.requestRide("Early", 0, 1, RideType::MINI, false, f.trace);
    CHECK(none.ok && f.status(none.rideId) == RideStatus::WAITING);   // no drivers: queued, not an error
    CHECK(f.disp.queue().size() == 1);
    CHECK(f.disp.cancelRide(none.rideId, f.trace).ok);
    CHECK(f.disp.queue().isEmpty());

    f.disp.addDriver("Solo", RideType::MINI, 0, 450);
    OpResult first = f.disp.requestRide("X", 0, 1, RideType::MINI, false, f.trace);
    OpResult second = f.disp.requestRide("Y", 0, 2, RideType::MINI, false, f.trace);
    CHECK(f.status(first.rideId) == RideStatus::ASSIGNED);
    CHECK(f.status(second.rideId) == RideStatus::WAITING);
    f.runUntil(500);
    CHECK(f.status(first.rideId) == RideStatus::COMPLETED);
    CHECK(f.status(second.rideId) == RideStatus::COMPLETED);   // served from the queue
    CHECK(f.disp.queue().isEmpty());
    CHECK(f.disp.historyOf("x") && f.disp.historyOf("x")->size() == 1);
    CHECK(f.disp.findDriver(1)->tripsCompleted == 2);
}

TEST(dispatch_cancel_rules) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::BIKE, 3, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 1, RideType::BIKE, false, f.trace);
    CHECK(f.status(r.rideId) == RideStatus::ASSIGNED);
    CHECK(f.disp.cancelRide(r.rideId, f.trace).ok);            // ASSIGNED -> CANCELLED
    CHECK(f.status(r.rideId) == RideStatus::CANCELLED);
    CHECK(f.disp.findDriver(1)->status == DriverStatus::AVAILABLE);
    OpResult again = f.disp.cancelRide(r.rideId, f.trace);
    CHECK(!again.ok && again.code == "ALREADY_CANCELLED");
    CHECK(f.disp.cancelRide(4242, f.trace).code == "RIDE_NOT_FOUND");
    f.runUntil(100);                                           // stale events are ignored safely
    CHECK(f.status(r.rideId) == RideStatus::CANCELLED);

    OpResult done = f.disp.requestRide("Ali", 0, 1, RideType::BIKE, false, f.trace);
    f.runUntil(1000);
    CHECK(f.status(done.rideId) == RideStatus::COMPLETED);
    CHECK(f.disp.cancelRide(done.rideId, f.trace).code == "ALREADY_COMPLETED");
}

TEST(dispatch_rating_rules) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 400);
    OpResult r = f.disp.requestRide("Ali", 0, 1, RideType::MINI, false, f.trace);
    CHECK(f.disp.rateRide(r.rideId, 5, f.trace).code == "NOT_COMPLETED");
    f.runUntil(1000);
    CHECK(f.disp.rateRide(r.rideId, 0, f.trace).code == "INVALID_RATING");
    CHECK(f.disp.rateRide(r.rideId, 6, f.trace).code == "INVALID_RATING");
    CHECK(f.disp.rateRide(r.rideId, 5, f.trace).ok);
    CHECK(f.disp.rateRide(r.rideId, 4, f.trace).code == "ALREADY_RATED");
    CHECK(f.disp.findDriver(1)->ratingCenti == 405);           // (4.0*20 + 5) / 21
    CHECK(f.disp.leaderboard().contains(DriverKey{405, 1}));
    CHECK(!f.disp.leaderboard().contains(DriverKey{400, 1}));
    CHECK(f.disp.rateRide(999, 5, f.trace).code == "RIDE_NOT_FOUND");
}

TEST(dispatch_reroute_on_block) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    f.runUntil(2.0);                       // picked up at t=1, now on road A-C
    CHECK(f.status(r.rideId) == RideStatus::IN_PROGRESS);
    int roadDE = f.map.graph().findRoad(3, 4);
    f.map.graph().setBlocked(3, 4, true);
    f.disp.onRoadChanged(roadDE, f.trace);
    const Ride* ride = f.disp.findRide(r.rideId);
    // new route: A C B D F  (finish A-C, then C-B-D-F = 12 km)
    CHECK(ride->tripNodes.size() == 5 && ride->tripNodes[3] == 3 && ride->tripNodes[4] == 5);
    f.runUntil(500);
    CHECK(f.status(r.rideId) == RideStatus::COMPLETED);
    CHECK(nearlyEqual(ride->completeTime, 29.0));          // 1 + 4 (A-C) + 24 (12 km at 30 km/h)
}

TEST(dispatch_destination_unreachable_cancels) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    f.runUntil(2.0);
    f.map.graph().setBlocked(3, 5, true);
    f.disp.onRoadChanged(f.map.graph().findRoad(3, 5), f.trace);
    f.map.graph().setBlocked(4, 5, true);
    f.disp.onRoadChanged(f.map.graph().findRoad(4, 5), f.trace);
    const Ride* ride = f.disp.findRide(r.rideId);
    CHECK(ride->status == RideStatus::CANCELLED);
    CHECK(ride->cancelReason == "Destination unreachable due to road block");
    CHECK(f.disp.findDriver(1)->status == DriverStatus::AVAILABLE);
    OpResult blocked = f.disp.requestRide("Sara", 0, 5, RideType::MINI, false, f.trace);
    CHECK(!blocked.ok && blocked.code == "UNREACHABLE");
}

TEST(dispatch_analytics) {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    f.disp.requestRide("A1", 0, 1, RideType::MINI, false, f.trace);
    OpResult c = f.disp.requestRide("A2", 2, 3, RideType::MINI, false, f.trace);
    f.disp.cancelRide(c.rideId, f.trace);
    f.runUntil(1000);
    Analytics a = f.disp.analytics();
    CHECK(a.totalRides == 2 && a.completed == 1 && a.cancelled == 1);
    CHECK(a.byType[(int)RideType::MINI] == 2);
    CHECK(a.busiest.size() == 2);
    CHECK(a.topEarnerId == 1 && a.topEarnings > 0);
    CHECK(a.avgFare > 0);
}

// ================================================================ Simulator
TEST(sim_init_and_controls) {
    Simulator sim;
    std::string err;
    CHECK(!Simulator().init("data/missing.json", err) && !err.empty());
    CHECK(sim.init("data/karachi_map.json", err));
    CHECK(sim.dispatcher().drivers().size() == Simulator::kDriverCount);
    CHECK(sim.dispatcher().leaderboard().size() == Simulator::kDriverCount);
    CHECK(sim.setSpeed(5) && sim.speed() == 5);
    CHECK(!sim.setSpeed(3) && sim.speed() == 5);
    sim.setPaused(true);
    sim.advanceReal(10);
    CHECK(nearlyEqual(sim.now(), 0.0));       // paused: clock frozen
    sim.setPaused(false);
    sim.advanceReal(2);
    CHECK(nearlyEqual(sim.now(), 10.0));      // 2 s at 5x = 10 sim min
}

TEST(sim_same_seed_is_reproducible) {
    Simulator a, b;
    std::string err;
    a.init("data/karachi_map.json", err);
    b.init("data/karachi_map.json", err);
    bool same = true;
    for (int id = 1; id <= Simulator::kDriverCount; ++id) {
        const Driver* x = a.dispatcher().findDriver(id);
        const Driver* y = b.dispatcher().findDriver(id);
        if (!x || !y || x->areaId != y->areaId || x->ratingCenti != y->ratingCenti || x->type != y->type) same = false;
    }
    CHECK(same);
}

TEST(sim_rush_hour_long_run_invariants) {
    Simulator sim;
    std::string err;
    CHECK(sim.init("data/karachi_map.json", err));
    sim.setRushHour(true);
    for (int i = 0; i < 2400; ++i) sim.advanceSim(0.25);   // 10 sim hours
    const Dispatcher& d = sim.dispatcher();
    Analytics a = d.analytics();
    CHECK(a.totalRides > 50);
    CHECK(a.completed > 30);
    CHECK(sim.dispatcher().queue().size() <= Simulator::kRushQueueCap + 3);
    // every driver is consistent: free drivers have no ride, busy ones do
    bool consistent = true;
    d.drivers().forEach([&](int, const Driver& drv) {
        if (drv.status == DriverStatus::AVAILABLE && drv.rideId != -1) consistent = false;
        if (drv.status != DriverStatus::AVAILABLE && drv.rideId == -1) consistent = false;
    });
    CHECK(consistent);
    CHECK(d.leaderboard().size() == Simulator::kDriverCount && d.leaderboard().isValidAVL());
    CHECK(sim.events().isValidHeap());
    DSTrace bg = sim.takeBackgroundTrace();
    CHECK(bg.size() > 0);
    CHECK(sim.takeBackgroundTrace().size() == 0);   // drained
}

TEST(sim_road_edits_validate) {
    Simulator sim;
    std::string err;
    sim.init("data/karachi_map.json", err);
    DSTrace t;
    int saddar = sim.map().findAreaByName("Saddar");
    int clifton = sim.map().findAreaByName("Clifton");
    int malir = sim.map().findAreaByName("Malir");
    CHECK(sim.setTraffic(saddar, clifton, 2.0, t).ok);
    CHECK(sim.setTraffic(saddar, clifton, 0.0, t).code == "INVALID_TRAFFIC");
    CHECK(sim.setTraffic(saddar, clifton, -1.0, t).code == "INVALID_TRAFFIC");
    CHECK(sim.setTraffic(saddar, clifton, 3.5, t).code == "INVALID_TRAFFIC");
    CHECK(sim.setTraffic(saddar, malir, 2.0, t).code == "ROAD_NOT_FOUND");
    CHECK(sim.setTraffic(-1, clifton, 2.0, t).code == "INVALID_AREA");
    CHECK(sim.setBlocked(saddar, clifton, true, t).ok);
    OpResult again = sim.setBlocked(saddar, clifton, true, t);   // idempotent
    CHECK(again.ok && again.message == "Road already blocked");
    CHECK(sim.setBlocked(saddar, malir, true, t).code == "ROAD_NOT_FOUND");
    CHECK(sim.reset(err));
    CHECK(!sim.map().graph().road(sim.map().graph().findRoad(saddar, clifton)).blocked);   // reset clears blocks
}

TEST(dispatch_block_updates_planned_trip) {
    DispatchFixture f;
    f.disp.addDriver("Far", RideType::MINI, 5, 450);          // at F: still driving to pickup A
    OpResult r = f.disp.requestRide("Ali", 0, 3, RideType::MINI, false, f.trace);
    CHECK(f.status(r.rideId) == RideStatus::ASSIGNED);
    // planned trip A C B D; block B-D -> planned trip must change before pickup
    f.map.graph().setBlocked(1, 3, true);
    f.disp.onRoadChanged(f.map.graph().findRoad(1, 3), f.trace);
    const Ride* ride = f.disp.findRide(r.rideId);
    CHECK(ride->status == RideStatus::ASSIGNED);
    CHECK(ride->tripNodes.size() == 3 && ride->tripNodes[1] == 2);   // A C D (10 km)
}
