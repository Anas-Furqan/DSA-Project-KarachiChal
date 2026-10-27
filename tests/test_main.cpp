#include <cmath>
#include <iostream>
#include <string>

#include "core/BFS.h"
#include "core/CityMap.h"
#include "core/Dijkstra.h"
#include "core/Dispatcher.h"
#include "core/Models.h"
#include "core/Simulator.h"
#include "core/SurgeEngine.h"
#include "ds/AVLTree.h"
#include "ds/Graph.h"
#include "ds/HashTable.h"
#include "ds/LinkedList.h"
#include "ds/LinkedQueue.h"
#include "ds/MinHeap.h"
#include "ds/Trie.h"

using namespace std;

int failedChecks = 0;
int passedTests = 0;
int failedTests = 0;

#define CHECK(cond)                                                                    \
    if (!(cond)) {                                                                     \
        failedChecks++;                                                                \
        cout << "    check failed: " << #cond << " (line " << __LINE__ << ")" << endl; \
    }

void runTest(string name, void (*testFunction)()) {
    int before = failedChecks;
    testFunction();
    if (failedChecks == before) {
        passedTests++;
        cout << "PASS  " << name << endl;
    } else {
        failedTests++;
        cout << "FAIL  " << name << endl;
    }
}

bool nearlyEqual(double a, double b) {
    return fabs(a - b) < 1e-6;
}

// ---------------------------------------------------------------- Graph

void testGraphAddNodesAndEdges() {
    Graph g;
    CHECK(g.nodeCount() == 0);
    int a = g.addNode("A", 1, 2);
    int b = g.addNode("B", 3, 4);
    int c = g.addNode("C", 5, 6);
    CHECK(a == 0 && b == 1 && c == 2);
    int ab = g.addEdge(a, b, 4.5);
    int bc = g.addEdge(b, c, 2.0);
    CHECK(ab == 0 && bc == 1);
    CHECK(g.roadCount() == 2);
    CHECK(g.degree(b) == 2);
    CHECK(g.findRoad(a, b) == ab);
    CHECK(g.findRoad(b, a) == ab);
    CHECK(g.findRoad(a, c) == -1);
    CHECK(g.getNode(c).name == "C");
    CHECK(nearlyEqual(g.effectiveWeight(ab), 4.5));
}

void testGraphRejectsBadEdges() {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 0, 0);
    CHECK(g.addEdge(0, 0, 1) == -1);
    CHECK(g.addEdge(0, 5, 1) == -1);
    CHECK(g.addEdge(-1, 1, 1) == -1);
    CHECK(g.addEdge(0, 1, 0) == -1);
    CHECK(g.addEdge(0, 1, -3) == -1);
    CHECK(g.addEdge(0, 1, 2) == 0);
    CHECK(g.addEdge(1, 0, 2) == -1);
    CHECK(g.roadCount() == 1);
}

void testGraphTrafficAndBlocking() {
    Graph g;
    g.addNode("A", 0, 0);
    g.addNode("B", 0, 0);
    int r = g.addEdge(0, 1, 2);
    CHECK(g.setTraffic(0, 1, 3.0));
    CHECK(nearlyEqual(g.effectiveWeight(r), 6.0));
    CHECK(!g.setTraffic(0, 1, 0.5));
    CHECK(!g.setTraffic(0, 1, 3.5));
    CHECK(!g.setTraffic(0, 1, -1));
    CHECK(g.setBlocked(1, 0, true));
    CHECK(!g.isUsable(r));
    CHECK(g.setBlocked(1, 0, true));
    CHECK(g.setBlocked(0, 1, false));
    CHECK(g.isUsable(r));
    CHECK(!g.setBlocked(0, 0, true));
}

void testGraphEmptyAndInvalid() {
    Graph g;
    CHECK(g.neighbors(0) == NULL);
    CHECK(g.degree(3) == 0);
    CHECK(!g.isValidNode(0));
    CHECK(g.findRoad(0, 1) == -1);
    CHECK(g.effectiveWeight(7) == 0);
}

// ------------------------------------------------------------- MinHeap

void testHeapPushPopOrder() {
    MinHeap h;
    double keys[6] = {5, 3, 8, 1, 9, 2};
    for (int i = 0; i < 6; i++) h.push(makeHeapItem(keys[i], i, i));
    CHECK(h.size() == 6);
    CHECK(h.isValidHeap());
    HeapItem top;
    CHECK(h.peek(top) && top.key == 1);
    double expected[6] = {1, 2, 3, 5, 8, 9};
    for (int i = 0; i < 6; i++) {
        HeapItem out;
        CHECK(h.popMin(out));
        CHECK(out.key == expected[i]);
        CHECK(h.isValidHeap());
    }
    CHECK(h.isEmpty());
}

void testHeapEmptyAndSingle() {
    MinHeap h;
    HeapItem out = makeHeapItem(-1, 0, 0);
    CHECK(!h.popMin(out));
    CHECK(!h.peek(out));
    CHECK(out.key == -1);
    h.push(makeHeapItem(7, 0, 42));
    CHECK(h.popMin(out) && out.value == 42);
    CHECK(!h.popMin(out));
}

void testHeapDuplicatesAndNegatives() {
    MinHeap h;
    h.push(makeHeapItem(4, 2, 0));
    h.push(makeHeapItem(-3, 0, 1));
    h.push(makeHeapItem(4, 1, 2));
    h.push(makeHeapItem(-3, 1, 3));
    HeapItem out;
    h.popMin(out);
    CHECK(out.value == 1);
    h.popMin(out);
    CHECK(out.value == 3);
    h.popMin(out);
    CHECK(out.value == 2);
    h.popMin(out);
    CHECK(out.value == 0);
}

void testHeapLarge10k() {
    MinHeap h;
    unsigned int seed = 7;
    for (int i = 0; i < 10000; i++) {
        seed = seed * 1103515245u + 12345u;
        h.push(makeHeapItem((int)(seed % 100000) - 50000, i, i));
    }
    CHECK(h.size() == 10000);
    CHECK(h.isValidHeap());
    double last = -1e9;
    bool sorted = true;
    int count = 0;
    HeapItem out;
    while (h.popMin(out)) {
        if (out.key < last) sorted = false;
        last = out.key;
        count++;
    }
    CHECK(sorted);
    CHECK(count == 10000);
}

// ------------------------------------------------------------ Dijkstra

// Graph used by every Dijkstra dry run in docs/DRY_RUNS.md:
// A-B 4, A-C 2, B-C 1, B-D 5, C-D 8, C-E 10, D-E 2, D-F 6, E-F 3
void buildDryRunGraph(Graph& g) {
    string names[6] = {"A", "B", "C", "D", "E", "F"};
    for (int i = 0; i < 6; i++) g.addNode(names[i], 0, i);
    g.addEdge(0, 1, 4);
    g.addEdge(0, 2, 2);
    g.addEdge(1, 2, 1);
    g.addEdge(1, 3, 5);
    g.addEdge(2, 3, 8);
    g.addEdge(2, 4, 10);
    g.addEdge(3, 4, 2);
    g.addEdge(3, 5, 6);
    g.addEdge(4, 5, 3);
}

bool pathIs(const PathResult& p, int expected[], int n) {
    if (p.nodeCount != n) return false;
    for (int i = 0; i < n; i++) {
        if (p.nodes[i] != expected[i]) return false;
    }
    return true;
}

// Dry run 1: A -> F = A C B D E F, 13 km
void testDijkstraCase1Basic() {
    Graph g;
    buildDryRunGraph(g);
    PathResult p = Dijkstra::shortestPath(g, 0, 5);
    int expected[6] = {0, 2, 1, 3, 4, 5};
    CHECK(p.found);
    CHECK(nearlyEqual(p.distance, 13.0));
    CHECK(pathIs(p, expected, 6));
    CHECK(p.roads[0] == g.findRoad(0, 2));
}

// Dry run 2: block D-E, A -> E = A C E, 12 km
void testDijkstraCase2BlockedEdge() {
    Graph g;
    buildDryRunGraph(g);
    CHECK(g.setBlocked(3, 4, true));
    PathResult p = Dijkstra::shortestPath(g, 0, 4);
    int expected[3] = {0, 2, 4};
    CHECK(p.found);
    CHECK(nearlyEqual(p.distance, 12.0));
    CHECK(pathIs(p, expected, 3));
}

// Dry run 3: isolated node G is unreachable
void testDijkstraCase3Unreachable() {
    Graph g;
    buildDryRunGraph(g);
    int gNode = g.addNode("G", 9, 9);
    PathResult p = Dijkstra::shortestPath(g, 0, gNode);
    CHECK(!p.found);
    CHECK(p.nodeCount == 0);
}

// Dry run 4: traffic x3 on B-C, A -> D = A B D, 9 km
void testDijkstraCase4Traffic() {
    Graph g;
    buildDryRunGraph(g);
    CHECK(g.setTraffic(1, 2, 3.0));
    PathResult p = Dijkstra::shortestPath(g, 0, 3);
    int expected[3] = {0, 1, 3};
    CHECK(p.found);
    CHECK(nearlyEqual(p.distance, 9.0));
    CHECK(pathIs(p, expected, 3));
}

void testDijkstraSameNodeAndInvalid() {
    Graph g;
    buildDryRunGraph(g);
    PathResult same = Dijkstra::shortestPath(g, 2, 2);
    CHECK(same.found && nearlyEqual(same.distance, 0) && same.nodeCount == 1);
    CHECK(!Dijkstra::shortestPath(g, -1, 2).found);
    CHECK(!Dijkstra::shortestPath(g, 0, 99).found);
    Graph empty;
    CHECK(!Dijkstra::shortestPath(empty, 0, 0).found);
}

void testDijkstraFindNearest() {
    Graph g;
    buildDryRunGraph(g);
    HashTable freeCount;
    freeCount.insert(freeKey(3, RideType::MINI), 1);
    freeCount.insert(freeKey(4, RideType::MINI), 1);
    freeCount.insert(freeKey(1, RideType::BIKE), 1);
    int found = -1;
    PathResult p = Dijkstra::findNearest(g, 0, freeCount, RideType::MINI, found);
    CHECK(found == 3);
    CHECK(nearlyEqual(p.distance, 8.0));
    Dijkstra::findNearest(g, 0, freeCount, RideType::AC, found);
    CHECK(found == -1);
    Dijkstra::findNearest(g, 0, freeCount, RideType::BIKE, found);
    CHECK(found == 1);
}

void testDijkstraTraceSteps() {
    Graph g;
    buildDryRunGraph(g);
    DijkstraTrace t;
    Dijkstra::traceSteps(g, 0, 5, t);
    CHECK(t.result.found && nearlyEqual(t.result.distance, 13.0));
    // pops: A0 C2 B3 B4(stale) D8 D10(stale) E10 E12(stale) F13
    CHECK(t.stepCount == 9);
    CHECK(t.steps[0].popped == 0 && t.steps[0].relaxCount == 2);
    CHECK(t.steps[3].stale);
    CHECK(t.steps[t.stepCount - 1].popped == 5);
    DSTrace trace;
    Dijkstra::shortestPath(g, 0, 5, &trace);
    CHECK(trace.size() > 0 && !trace.isTruncated());
}

// --------------------------------------------------------- LinkedQueue

// Queue dry run 1: FIFO order
void testQueueCase1Fifo() {
    LinkedQueue q;
    q.enqueue(10);
    q.enqueue(20);
    q.enqueue(30);
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

// Queue dry run 2: cancel 102 from the middle, then the rear
void testQueueCase2CancelById() {
    LinkedQueue q;
    q.enqueue(101);
    q.enqueue(102);
    q.enqueue(103);
    q.enqueue(104);
    CHECK(q.removeById(102));
    CHECK(q.size() == 3);
    CHECK(!q.removeById(102));
    CHECK(q.removeById(104));
    q.enqueue(105);
    int expected[3] = {101, 103, 105};
    int i = 0;
    for (QueueNode* n = q.getFront(); n != NULL; n = n->next) {
        CHECK(i < 3 && n->value == expected[i]);
        i++;
    }
    CHECK(i == 3);
}

// Queue dry run 3: empty and single element
void testQueueCase3EmptyAndSingle() {
    LinkedQueue q;
    int out = -1;
    CHECK(!q.dequeue(out));
    CHECK(!q.peekFront(out));
    CHECK(out == -1);
    CHECK(!q.removeById(5));
    q.enqueue(5);
    CHECK(q.removeById(5));
    CHECK(q.isEmpty());
    q.enqueue(6);
    CHECK(q.dequeue(out) && out == 6);
    CHECK(q.getFront() == NULL);
}

void testQueueLarge10k() {
    LinkedQueue q;
    for (int i = 0; i < 10000; i++) q.enqueue(i);
    CHECK(q.size() == 10000);
    CHECK(q.removeById(5000));
    bool inOrder = true;
    int out = 0;
    int last = -1;
    while (q.dequeue(out)) {
        if (out <= last || out == 5000) inOrder = false;
        last = out;
    }
    CHECK(inOrder);
}

// ----------------------------------------------------------------- BFS

void testBfsHopsAndReachability() {
    Graph g;
    buildDryRunGraph(g);
    int hops[MAX_NODES];
    int reached = BFS::hopsFrom(g, 0, -1, hops);
    CHECK(reached == 6);
    CHECK(hops[0] == 0 && hops[1] == 1 && hops[2] == 1);
    CHECK(hops[3] == 2 && hops[4] == 2 && hops[5] == 3);
    BFS::hopsFrom(g, 0, 2, hops);
    CHECK(hops[5] == -1);
    CHECK(BFS::isReachable(g, 0, 5));
}

void testBfsBlockedRoadsDisconnect() {
    Graph g;
    buildDryRunGraph(g);
    g.setBlocked(3, 5, true);
    g.setBlocked(4, 5, true);
    CHECK(!BFS::isReachable(g, 0, 5));
    CHECK(BFS::isReachable(g, 0, 4));
    CHECK(!BFS::isReachable(g, 0, 77));
    int counts[6] = {1, 1, 1, 1, 1, 1};
    CHECK(BFS::sumWithinHops(g, 5, 2, counts) == 1);
}

// ----------------------------------------------------------- HashTable

void testHashInsertFindErase() {
    HashTable t;
    CHECK(t.insert(7, 70));
    CHECK(t.insert("seven", 7));
    CHECK(!t.insert(7, 71));
    int v = 0;
    CHECK(t.find(7, v) && v == 71);
    CHECK(t.find("seven", v) && v == 7);
    CHECK(!t.find(8, v));
    CHECK(t.size() == 2);
    CHECK(t.erase(7));
    CHECK(!t.erase(7));
    CHECK(!t.contains(7));
    CHECK(t.size() == 1);
}

void testHashNegativesAndCounters() {
    HashTable t;
    t.insert(-5, 1);
    t.insert("", 2);
    CHECK(t.get(-5) == 1);
    CHECK(t.get("") == 2);
    CHECK(t.get("missing") == 0);
    t.addTo("count", 1);
    t.addTo("count", 1);
    t.addTo("count", -1);
    CHECK(t.get("count") == 1);
}

void testHashRehash10k() {
    HashTable t;
    CHECK(t.bucketCount() == 8);
    for (int i = 0; i < 10000; i++) t.insert(i, i * 2);
    CHECK(t.size() == 10000);
    CHECK(t.rehashCount() > 0);
    CHECK(t.loadFactor() <= 0.75);
    bool allFound = true;
    for (int i = 0; i < 10000; i++) {
        int v = 0;
        if (!t.find(i, v) || v != i * 2) allFound = false;
    }
    CHECK(allFound);
    for (int i = 0; i < 10000; i += 2) t.erase(i);
    CHECK(t.size() == 5000);
    CHECK(!t.contains(4) && t.contains(5));
}

void testHashManyCollisions() {
    HashTable t(1);
    for (int i = 0; i < 50; i++) t.insert("key" + to_string(i), i);
    bool ok = true;
    for (int i = 0; i < 50; i++) {
        if (t.get("key" + to_string(i)) != i) ok = false;
    }
    CHECK(ok);
    CHECK(t.longestChain() >= 1);
    t.clear();
    CHECK(t.isEmpty());
}

// ---------------------------------------------------------------- Trie

void addNames(Trie& t) {
    t.insert("gulshan-e-iqbal", 0, "Gulshan-e-Iqbal");
    t.insert("gulistan-e-johar", 1, "Gulistan-e-Johar");
    t.insert("gulberg", 2, "Gulberg");
    t.insert("clifton", 3, "Clifton");
}

// Trie dry run 1: "gul" has three completions in alphabet order
void testTrieCase1SharedPrefix() {
    Trie t;
    addNames(t);
    TrieMatch results[5];
    int found = t.startsWith("gul", results, 5);
    CHECK(found == 3);
    CHECK(results[0].word == "Gulberg");
    CHECK(results[1].word == "Gulistan-e-Johar");
    CHECK(results[2].word == "Gulshan-e-Iqbal");
    CHECK(t.wordCount() == 4);
}

// Trie dry run 2: case-insensitive
void testTrieCase2CaseInsensitive() {
    Trie t;
    addNames(t);
    TrieMatch results[5];
    int found = t.startsWith("CLIF", results, 5);
    CHECK(found == 1 && results[0].value == 3);
    CHECK(t.contains("Clifton"));
    CHECK(!t.contains("Clif"));
}

// Trie dry run 3: no match
void testTrieCase3NoMatch() {
    Trie t;
    addNames(t);
    TrieMatch results[5];
    CHECK(t.startsWith("gux", results, 5) == 0);
    CHECK(t.matchedLength("gux") == 2);
    CHECK(t.startsWith("z", results, 5) == 0);
}

void testTrieEdgeInputs() {
    Trie t;
    addNames(t);
    TrieMatch results[5];
    CHECK(t.startsWith("", results, 5) == 0);
    CHECK(t.startsWith("gul", results, 0) == 0);
    CHECK(t.startsWith("g@l", results, 5) == 0);
    CHECK(!t.insert("", 9, ""));
    CHECK(!t.insert("bad#name", 9, ""));
    CHECK(t.wordCount() == 4);
    string longText(300, 'a');
    CHECK(t.startsWith(longText, results, 5) == 0);
}

void testTrieLimitAndDedupe() {
    Trie t;
    for (int i = 0; i < 10; i++) t.insert("area " + to_string(i), i, "");
    t.insert("area alias", 0, "Area 0");
    TrieMatch results[5];
    CHECK(t.startsWith("area", results, 5) == 5);
    TrieMatch all[20];
    CHECK(t.startsWith("area", all, 20) == 10);
}

void testTrieBranchView() {
    Trie t;
    addNames(t);
    TrieViewNode view[60];
    int count = t.branchView("gul", view, 60);
    CHECK(count > 4);
    CHECK(view[0].parent == -1);
    int onPath = 0;
    for (int i = 0; i < count; i++) {
        if (view[i].onPath) onPath++;
    }
    CHECK(onPath == 4);
}

// ----------------------------------------------------------------- AVL
// Higher rating comes first, so inserting 300, 200, 100 is like 1, 2, 3 in a normal BST.

// AVL dry run 1 (RR)
void testAvlCase1RR() {
    AVLTree t;
    t.insert(300, 1);
    t.insert(200, 2);
    CHECK(t.lastRotationCount() == 0);
    t.insert(100, 3);
    CHECK(t.rotationCount("RR") == 1);
    CHECK(t.lastRotationCount() == 1);
    CHECK(t.height() == 2 && t.isValidAVL());
    AVLViewNode v[10];
    t.view(v, 10);
    CHECK(v[0].driverId == 2);
}

// AVL dry run 2 (LL)
void testAvlCase2LL() {
    AVLTree t;
    t.insert(100, 1);
    t.insert(200, 2);
    t.insert(300, 3);
    CHECK(t.rotationCount("LL") == 1);
    AVLViewNode v[10];
    t.view(v, 10);
    CHECK(v[0].driverId == 2 && t.isValidAVL());
}

// AVL dry run 3 (LR)
void testAvlCase3LR() {
    AVLTree t;
    t.insert(100, 1);
    t.insert(300, 3);
    t.insert(200, 2);
    CHECK(t.rotationCount("LR") == 1);
    AVLViewNode v[10];
    t.view(v, 10);
    CHECK(v[0].driverId == 2 && t.isValidAVL());
}

// AVL dry run 4 (RL)
void testAvlCase4RL() {
    AVLTree t;
    t.insert(300, 3);
    t.insert(100, 1);
    t.insert(200, 2);
    CHECK(t.rotationCount("RL") == 1);
    AVLViewNode v[10];
    t.view(v, 10);
    CHECK(v[0].driverId == 2 && t.isValidAVL());
}

void testAvlDuplicateRatings() {
    AVLTree t;
    t.insert(450, 7);
    t.insert(450, 3);
    t.insert(450, 5);
    t.insert(490, 9);
    CHECK(!t.insert(450, 3));
    CHECK(t.size() == 4);
    int ratings[10];
    int ids[10];
    int n = t.topN(ratings, ids, 10);
    CHECK(n == 4);
    CHECK(ids[0] == 9 && ids[1] == 3 && ids[2] == 5 && ids[3] == 7);
}

void testAvlDeleteAndUpdate() {
    AVLTree t;
    for (int i = 1; i <= 7; i++) t.insert(400 + i * 10, i);
    CHECK(!t.remove(999, 1));
    CHECK(t.remove(440, 4));
    CHECK(!t.contains(440, 4));
    CHECK(t.size() == 6 && t.isValidAVL());
    t.remove(420, 2);
    t.insert(500, 2);
    int ratings[3];
    int ids[3];
    CHECK(t.topN(ratings, ids, 3) == 3);
    CHECK(ids[0] == 2 && ratings[0] == 500);
    AVLTree empty;
    CHECK(!empty.remove(1, 1));
    CHECK(empty.topN(ratings, ids, 3) == 0);
}

void testAvlLargeStaysBalanced() {
    AVLTree t;
    for (int i = 0; i < 1000; i++) t.insert(100 + (i % 400), i);
    CHECK(t.size() == 1000);
    CHECK(t.isValidAVL());
    CHECK(t.height() <= 15);
    for (int i = 0; i < 1000; i += 3) t.remove(100 + (i % 400), i);
    CHECK(t.isValidAVL());
}

// ---------------------------------------------------------- LinkedList

void testListPushFrontNewestFirst() {
    LinkedList list;
    list.pushFront(1001);
    list.pushFront(1002);
    list.pushFront(1003);
    CHECK(list.size() == 3);
    CHECK(list.getHead()->value == 1003);
    CHECK(list.getHead()->next->next->value == 1001);
}

void testListRemoveAndEmpty() {
    LinkedList list;
    int out = 0;
    CHECK(!list.popFront(out));
    CHECK(!list.remove(4));
    list.pushFront(1);
    list.pushFront(2);
    list.pushFront(3);
    CHECK(list.remove(2));
    CHECK(list.contains(1) && !list.contains(2));
    CHECK(list.remove(3));
    CHECK(list.remove(1));
    CHECK(list.isEmpty() && list.getHead() == NULL);
}

void testListLarge() {
    LinkedList list;
    for (int i = 0; i < 10000; i++) list.pushFront(i);
    CHECK(list.size() == 10000);
    CHECK(list.getHead()->value == 9999);
    list.clear();
    CHECK(list.size() == 0);
}

// ------------------------------------------------------------- CityMap

string dryRunMapJson() {
    return "{\"areas\":["
           "{\"id\":0,\"name\":\"A\",\"lat\":24.80,\"lng\":67.00},{\"id\":1,\"name\":\"B\",\"lat\":24.82,\"lng\":67.02},"
           "{\"id\":2,\"name\":\"C\",\"lat\":24.81,\"lng\":67.01},{\"id\":3,\"name\":\"D\",\"lat\":24.84,\"lng\":67.03},"
           "{\"id\":4,\"name\":\"E\",\"lat\":24.85,\"lng\":67.04},{\"id\":5,\"name\":\"F\",\"lat\":24.87,\"lng\":67.05}],"
           "\"roads\":[{\"from\":0,\"to\":1,\"km\":4},{\"from\":0,\"to\":2,\"km\":2},{\"from\":1,\"to\":2,\"km\":1},"
           "{\"from\":1,\"to\":3,\"km\":5},{\"from\":2,\"to\":3,\"km\":8},{\"from\":2,\"to\":4,\"km\":10},"
           "{\"from\":3,\"to\":4,\"km\":2},{\"from\":3,\"to\":5,\"km\":6},{\"from\":4,\"to\":5,\"km\":3}]}";
}

void testCityMapLoadsValidJson() {
    CityMap m;
    string error;
    CHECK(m.loadFromJsonText(dryRunMapJson(), error));
    CHECK(error.empty());
    CHECK(m.areaCount() == 6);
    CHECK(m.graph().roadCount() == 9);
    CHECK(m.findAreaByName("  d ") == 3);
    CHECK(m.findAreaByName("Z") == -1);
    CHECK(m.areaName(99) == "?");
}

void testCityMapRejectsBadFiles() {
    CityMap m;
    string error;
    CHECK(!m.loadFromFile("no/such/file.json", error));
    CHECK(error.find("not found") != string::npos);
    CHECK(!m.loadFromJsonText("{not json", error));
    CHECK(!m.loadFromJsonText("{\"areas\":[]}", error));
    CHECK(!m.loadFromJsonText("{\"areas\":[],\"roads\":[]}", error));
    string dupName = "{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":1,\"lng\":1},{\"id\":1,\"name\":\" a \",\"lat\":1,\"lng\":1}],\"roads\":[]}";
    CHECK(!m.loadFromJsonText(dupName, error));
    CHECK(error.find("Duplicate area name") != string::npos);
    string badRoad = "{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":1,\"lng\":1}],\"roads\":[{\"from\":0,\"to\":9,\"km\":1}]}";
    CHECK(!m.loadFromJsonText(badRoad, error));
    CHECK(error.find("unknown area") != string::npos);
    string negative = "{\"areas\":[{\"id\":0,\"name\":\"A\",\"lat\":1,\"lng\":1},{\"id\":1,\"name\":\"B\",\"lat\":1,\"lng\":1}],"
                      "\"roads\":[{\"from\":0,\"to\":1,\"km\":-2}]}";
    CHECK(!m.loadFromJsonText(negative, error));
    CHECK(error.find("non-positive") != string::npos);
    CHECK(m.areaCount() == 0);
}

void testCityMapRealKarachiFile() {
    CityMap m;
    string error;
    CHECK(m.loadFromFile("data/karachi_map.json", error));
    CHECK(m.areaCount() >= 28 && m.areaCount() <= 30);
    TrieMatch results[5];
    int found = m.search("gul", results, 5);
    CHECK(found >= 2);
    CHECK(m.search("iqbal", results, 5) >= 1);
    CHECK(m.search("", results, 5) == 0);
    CHECK(m.search("@@@", results, 5) == 0);
    int saddar = m.findAreaByName("saddar");
    CHECK(saddar >= 0);
    bool connected = true;
    for (int i = 0; i < m.areaCount(); i++) {
        if (!BFS::isReachable(m.graph(), saddar, i)) connected = false;
    }
    CHECK(connected);
}

// -------------------------------------------------------- Models/Surge

void testModelsFareAndRideType() {
    CHECK(computeFare(RideType::BIKE, 10, 1.0) == 310);
    CHECK(computeFare(RideType::MINI, 10, 1.0) == 520);
    CHECK(computeFare(RideType::AC, 10, 2.0) == 1460);
    RideType t;
    CHECK(parseRideType("bike", t) && t == RideType::BIKE);
    CHECK(parseRideType("Ac", t) && t == RideType::AC);
    CHECK(!parseRideType("TRUCK", t));
    CHECK(!parseRideType("", t));
}

void testEventOrderTies() {
    MinHeap h;
    HeapItem a = makeHeapItem(5.0, 2, 1002);
    HeapItem b = makeHeapItem(5.0, 1, 1001);
    HeapItem c = makeHeapItem(1.0, 3, 1003);
    h.push(a);
    h.push(b);
    h.push(c);
    HeapItem out;
    h.popMin(out);
    CHECK(out.value == 1003);
    h.popMin(out);
    CHECK(out.value == 1001);
    h.popMin(out);
    CHECK(out.value == 1002);
}

void testSurgeMultiplierSteps() {
    CHECK(SurgeEngine::multiplierFor(0, 0) == 1.0);
    CHECK(SurgeEngine::multiplierFor(1, 2) == 1.0);
    CHECK(SurgeEngine::multiplierFor(1, 1) == 1.25);
    CHECK(SurgeEngine::multiplierFor(2, 1) == 1.5);
    CHECK(SurgeEngine::multiplierFor(3, 1) == 2.0);
    CHECK(SurgeEngine::multiplierFor(5, 0) == 2.0);
    CHECK(SurgeEngine::multiplierFor(-3, 4) == 1.0);
}

void testSurgeBfsTwoHops() {
    Graph g;
    buildDryRunGraph(g);
    SurgeEngine s;
    int demand[6] = {4, 0, 0, 0, 0, 0};
    int freeDrivers[6] = {0, 0, 0, 0, 0, 1};
    s.recompute(g, demand, freeDrivers);
    CHECK(s.multiplierAt(0) == 2.0);
    CHECK(s.infoAt(0).supply == 0);
    freeDrivers[3] = 2;
    s.recompute(g, demand, freeDrivers);
    CHECK(s.infoAt(0).supply == 2);
    CHECK(s.multiplierAt(0) == 1.5);
    CHECK(s.multiplierAt(1) == 1.0);
    CHECK(s.multiplierAt(99) == 1.0);
}

// ---------------------------------------------------------- Dispatcher

struct DispatchFixture {
    CityMap cityMap;
    MinHeap events;
    SimClock clock;
    Dispatcher disp;
    DSTrace trace;

    DispatchFixture() : disp(cityMap, events, clock) {
        clock.now = 0;
        clock.nextSeq = 0;
        string error;
        cityMap.loadFromJsonText(dryRunMapJson(), error);
        disp.reset(0, 1);
    }

    // pop every event up to time t, like the Simulator does
    void runUntil(double t) {
        HeapItem e;
        while (events.peek(e) && e.key <= t) {
            events.popMin(e);
            if (e.key > clock.now) clock.now = e.key;
            disp.handleEvent(e, trace);
        }
        clock.now = t;
    }

    RideStatus status(int rideId) {
        return disp.findRide(rideId)->status;
    }
};

void testDispatchNearestDriverAndFare() {
    DispatchFixture f;
    int driverD = f.disp.addDriver("Driver D", RideType::MINI, 3, 450);
    f.disp.addDriver("Driver E", RideType::MINI, 4, 490);
    f.disp.addDriver("Bike B", RideType::BIKE, 1, 500);
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    CHECK(r.ok);
    Ride* ride = f.disp.findRide(r.rideId);
    CHECK(ride->status == RideStatus::ASSIGNED);
    CHECK(ride->driverId == driverD);
    CHECK(ride->fare == 640);
    CHECK(nearlyEqual(ride->distanceKm, 13.0));
    CHECK(f.trace.size() > 0);
}

void testDispatchSameAreaBestRating() {
    DispatchFixture f;
    f.disp.addDriver("Low", RideType::AC, 2, 380);
    int best = f.disp.addDriver("High", RideType::AC, 2, 495);
    f.disp.addDriver("Mid", RideType::AC, 2, 420);
    OpResult r = f.disp.requestRide("Sara", 0, 3, RideType::AC, false, f.trace);
    CHECK(r.ok && f.disp.findRide(r.rideId)->driverId == best);
}

void testDispatchValidationErrors() {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    CHECK(f.disp.addDriver("Bad", RideType::MINI, 77, 450) == -1);
    CHECK(f.disp.addDriver("Bad", RideType::MINI, 0, 900) == -1);
    CHECK(f.disp.requestRide("", 0, 1, RideType::MINI, false, f.trace).code == "MISSING_FIELD");
    CHECK(f.disp.requestRide("   ", 0, 1, RideType::MINI, false, f.trace).code == "MISSING_FIELD");
    CHECK(f.disp.requestRide(string(41, 'x'), 0, 1, RideType::MINI, false, f.trace).code == "INVALID_NAME");
    CHECK(f.disp.requestRide("Ali", -1, 1, RideType::MINI, false, f.trace).code == "INVALID_AREA");
    CHECK(f.disp.requestRide("Ali", 0, 40, RideType::MINI, false, f.trace).code == "INVALID_AREA");
    CHECK(f.disp.requestRide("Ali", 2, 2, RideType::MINI, false, f.trace).code == "SAME_LOCATION");
    OpResult first = f.disp.requestRide("Ali", 0, 1, RideType::MINI, false, f.trace);
    CHECK(first.ok);
    OpResult again = f.disp.requestRide(" ALI ", 2, 3, RideType::MINI, false, f.trace);
    CHECK(!again.ok && again.httpStatus == 409 && again.code == "DUPLICATE_REQUEST");
}

void testDispatchQueueThenServed() {
    DispatchFixture f;
    f.disp.addDriver("Only", RideType::MINI, 0, 450);
    OpResult r1 = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    OpResult r2 = f.disp.requestRide("Sara", 1, 2, RideType::MINI, false, f.trace);
    OpResult r3 = f.disp.requestRide("Omar", 3, 4, RideType::AC, false, f.trace);
    CHECK(f.status(r1.rideId) == RideStatus::ASSIGNED);
    CHECK(f.status(r2.rideId) == RideStatus::WAITING);
    CHECK(f.status(r3.rideId) == RideStatus::WAITING);
    CHECK(f.disp.queue().size() == 2);
    CHECK(f.disp.pendingInArea(1) == 1);
    f.runUntil(28.5);
    CHECK(f.status(r1.rideId) == RideStatus::COMPLETED);
    CHECK(f.status(r2.rideId) == RideStatus::ASSIGNED);
    CHECK(f.disp.queue().size() == 1);
    f.runUntil(200);
    CHECK(f.status(r2.rideId) == RideStatus::COMPLETED);
    CHECK(f.status(r3.rideId) == RideStatus::WAITING);
    CHECK(f.disp.findRider("Ali")->history.size() == 1);
}

void testDispatchCancelRules() {
    DispatchFixture f;
    f.disp.addDriver("Only", RideType::MINI, 1, 450);
    OpResult assigned = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    OpResult waiting = f.disp.requestRide("Sara", 2, 3, RideType::MINI, false, f.trace);
    CHECK(f.disp.cancelRide(9999, f.trace).httpStatus == 404);
    CHECK(f.disp.cancelRide(waiting.rideId, f.trace).ok);
    CHECK(f.disp.queue().size() == 0);
    CHECK(f.disp.cancelRide(waiting.rideId, f.trace).code == "ALREADY_CANCELLED");
    CHECK(f.disp.cancelRide(assigned.rideId, f.trace).ok);
    CHECK(f.disp.freeDriversInArea(0) + f.disp.freeDriversInArea(1) + f.disp.freeDriversInArea(2) == 1);
    CHECK(f.disp.activeRideOf("Ali") == -1);

    OpResult trip = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    f.runUntil(10);
    CHECK(f.status(trip.rideId) == RideStatus::IN_PROGRESS);
    CHECK(f.disp.cancelRide(trip.rideId, f.trace).code == "CANCEL_NOT_ALLOWED");
    f.runUntil(100);
    CHECK(f.disp.cancelRide(trip.rideId, f.trace).code == "ALREADY_COMPLETED");
}

void testDispatchRatingRules() {
    DispatchFixture f;
    int driver = f.disp.addDriver("Only", RideType::BIKE, 0, 400);
    OpResult r = f.disp.requestRide("Ali", 0, 1, RideType::BIKE, false, f.trace);
    CHECK(f.disp.rateRide(r.rideId, 5, f.trace).code == "NOT_COMPLETED");
    f.runUntil(60);
    CHECK(f.status(r.rideId) == RideStatus::COMPLETED);
    CHECK(f.disp.rateRide(r.rideId, 0, f.trace).code == "INVALID_RATING");
    CHECK(f.disp.rateRide(r.rideId, 6, f.trace).code == "INVALID_RATING");
    CHECK(f.disp.rateRide(4242, 5, f.trace).httpStatus == 404);
    CHECK(f.disp.rateRide(r.rideId, 5, f.trace).ok);
    CHECK(f.disp.rateRide(r.rideId, 4, f.trace).code == "ALREADY_RATED");
    CHECK(f.disp.findDriver(driver)->rating == 405);
    CHECK(f.disp.leaderboard().contains(405, driver));
    CHECK(!f.disp.leaderboard().contains(400, driver));
}

void testDispatchRerouteOnBlock() {
    DispatchFixture f;
    f.disp.addDriver("Only", RideType::MINI, 0, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    f.runUntil(1.5);
    CHECK(f.status(r.rideId) == RideStatus::IN_PROGRESS);
    f.cityMap.graph().setBlocked(3, 4, true);
    f.disp.onRoadChanged(f.cityMap.graph().findRoad(3, 4), f.trace);
    Ride* ride = f.disp.findRide(r.rideId);
    bool usesE = false;
    for (int i = 0; i < ride->tripCount; i++) {
        if (ride->tripNodes[i] == 4) usesE = true;
    }
    CHECK(!usesE);
    CHECK(ride->tripNodes[ride->tripCount - 1] == 5);
    f.runUntil(100);
    CHECK(f.status(r.rideId) == RideStatus::COMPLETED);
}

void testDispatchUnreachableCancels() {
    DispatchFixture f;
    f.disp.addDriver("Only", RideType::MINI, 0, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    f.runUntil(1.5);
    f.cityMap.graph().setBlocked(3, 5, true);
    f.disp.onRoadChanged(f.cityMap.graph().findRoad(3, 5), f.trace);
    CHECK(f.status(r.rideId) == RideStatus::IN_PROGRESS);
    f.cityMap.graph().setBlocked(4, 5, true);
    f.disp.onRoadChanged(f.cityMap.graph().findRoad(4, 5), f.trace);
    CHECK(f.status(r.rideId) == RideStatus::CANCELLED);
    CHECK(f.disp.findRide(r.rideId)->cancelReason == "Destination unreachable due to road block");
    CHECK(f.disp.requestRide("Sara", 0, 5, RideType::MINI, false, f.trace).code == "UNREACHABLE");
    CHECK(f.disp.freeDriversInArea(0) + f.disp.freeDriversInArea(2) == 1);
}

void testDispatchBlockUpdatesPlannedTrip() {
    DispatchFixture f;
    f.disp.addDriver("Far", RideType::MINI, 5, 450);
    OpResult r = f.disp.requestRide("Ali", 0, 3, RideType::MINI, false, f.trace);
    CHECK(f.status(r.rideId) == RideStatus::ASSIGNED);
    f.cityMap.graph().setBlocked(1, 3, true);
    f.disp.onRoadChanged(f.cityMap.graph().findRoad(1, 3), f.trace);
    Ride* ride = f.disp.findRide(r.rideId);
    CHECK(ride->status == RideStatus::ASSIGNED);
    CHECK(ride->tripCount == 3 && ride->tripNodes[1] == 2);
}

void testDispatchAnalytics() {
    DispatchFixture f;
    f.disp.addDriver("D1", RideType::MINI, 0, 450);
    f.disp.addDriver("D2", RideType::BIKE, 2, 450);
    OpResult a = f.disp.requestRide("Ali", 0, 5, RideType::MINI, false, f.trace);
    OpResult b = f.disp.requestRide("Sara", 2, 3, RideType::BIKE, false, f.trace);
    f.disp.requestRide("Omar", 0, 4, RideType::AC, false, f.trace);
    f.disp.cancelRide(b.rideId, f.trace);
    f.runUntil(100);
    Analytics s = f.disp.analytics();
    CHECK(s.totalRides == 3);
    CHECK(s.completed == 1);
    CHECK(s.cancelled == 1);
    CHECK(s.waiting == 1);
    CHECK(nearlyEqual(s.avgFare, f.disp.findRide(a.rideId)->fare));
    CHECK(s.busiestTotal == 2 && s.busiestArea[0] == 0 && s.busiestCount[0] == 2);
    CHECK(s.byType[(int)RideType::AC] == 1);
    CHECK(s.hourCount == 1 && s.ridesPerHour[0] == 1);
    CHECK(s.topEarnerId == 1);
}

// ----------------------------------------------------------- Simulator

void testSimInitAndControls() {
    Simulator* sim = new Simulator();
    string error;
    CHECK(sim->init("data/karachi_map.json", error));
    CHECK(sim->dispatcher().driverCount() == DRIVER_COUNT);
    CHECK(sim->dispatcher().leaderboard().size() == DRIVER_COUNT);
    CHECK(!sim->setSpeed(3));
    CHECK(sim->setSpeed(5) && sim->getSpeed() == 5);
    sim->setPaused(true);
    sim->advanceReal(10);
    CHECK(sim->now() == 0);
    sim->setPaused(false);
    sim->advanceReal(2);
    CHECK(nearlyEqual(sim->now(), 10));
    CHECK(sim->reset(error));
    CHECK(sim->now() == 0 && sim->getSpeed() == 1);
    Simulator* broken = new Simulator();
    CHECK(!broken->init("missing.json", error));
    delete broken;
    delete sim;
}

void testSimSameSeedReproducible() {
    Simulator* a = new Simulator();
    Simulator* b = new Simulator();
    string error;
    a->init("data/karachi_map.json", error);
    b->init("data/karachi_map.json", error);
    a->setRushHour(true);
    b->setRushHour(true);
    for (int i = 0; i < 120; i++) {
        a->advanceSim(0.5);
        b->advanceSim(0.5);
    }
    CHECK(a->dispatcher().rideCount() == b->dispatcher().rideCount());
    CHECK(a->dispatcher().rideCount() > 0);
    bool same = true;
    for (int i = 0; i < a->dispatcher().driverCount(); i++) {
        if (a->dispatcher().driverAt(i).areaId != b->dispatcher().driverAt(i).areaId) same = false;
        if (a->dispatcher().driverAt(i).rating != b->dispatcher().driverAt(i).rating) same = false;
    }
    CHECK(same);
    delete a;
    delete b;
}

void testSimRushHourLongRun() {
    Simulator* sim = new Simulator();
    string error;
    sim->init("data/karachi_map.json", error);
    sim->setRushHour(true);
    for (int i = 0; i < 2000; i++) sim->advanceSim(0.5);
    const Dispatcher& d = sim->dispatcher();
    CHECK(d.rideCount() > 100);
    CHECK(d.queue().size() <= RUSH_QUEUE_CAP + 3);
    CHECK(d.leaderboard().isValidAVL());
    CHECK(d.leaderboard().size() == DRIVER_COUNT);
    int busy = 0;
    for (int i = 0; i < d.driverCount(); i++) {
        if (d.driverAt(i).status != DriverStatus::AVAILABLE) busy++;
    }
    int assignedOrOnTrip = 0;
    for (ListNode* p = d.activeRides().getHead(); p != NULL; p = p->next) {
        RideStatus s = d.findRide(p->value)->status;
        if (s == RideStatus::ASSIGNED || s == RideStatus::IN_PROGRESS) assignedOrOnTrip++;
    }
    // drivers in the 1 minute drop-off handover are busy but have no active ride
    CHECK(busy >= assignedOrOnTrip);
    Analytics a = d.analytics();
    CHECK(a.completed > 50);
    CHECK(a.totalRides == a.completed + a.cancelled + a.active);
    delete sim;
}

void testSimRoadEditsValidate() {
    Simulator* sim = new Simulator();
    string error;
    sim->init("data/karachi_map.json", error);
    DSTrace trace;
    const Road& road = sim->map().graph().getRoad(0);
    CHECK(sim->setTraffic(-1, 2, 2, trace).code == "INVALID_AREA");
    CHECK(sim->setTraffic(road.from, road.to, 0, trace).code == "INVALID_TRAFFIC");
    CHECK(sim->setTraffic(road.from, road.to, 3.5, trace).code == "INVALID_TRAFFIC");
    CHECK(sim->setTraffic(road.from, road.to, 2, trace).ok);
    int noRoadTo = -1;
    for (int v = 0; v < sim->map().areaCount(); v++) {
        if (v != road.from && sim->map().graph().findRoad(road.from, v) == -1) noRoadTo = v;
    }
    CHECK(sim->setBlocked(road.from, noRoadTo, true, trace).code == "ROAD_NOT_FOUND");
    CHECK(sim->setBlocked(road.from, road.to, true, trace).message == "Road blocked");
    CHECK(sim->setBlocked(road.from, road.to, true, trace).message == "Road already blocked");
    CHECK(sim->setBlocked(road.from, road.to, false, trace).message == "Road reopened");
    delete sim;
}

void testTraceCap() {
    DSTrace t;
    for (int i = 0; i < 250; i++) t.add("X", "op", "O(1)", "entry");
    CHECK(t.size() == MAX_TRACE);
    CHECK(t.isTruncated());
    t.clear();
    CHECK(t.size() == 0 && !t.isTruncated() && t.getHead() == NULL);
}

int main() {
    runTest("graph_add_nodes_and_edges", testGraphAddNodesAndEdges);
    runTest("graph_rejects_bad_edges", testGraphRejectsBadEdges);
    runTest("graph_traffic_and_blocking", testGraphTrafficAndBlocking);
    runTest("graph_empty_and_invalid", testGraphEmptyAndInvalid);
    runTest("heap_push_pop_order", testHeapPushPopOrder);
    runTest("heap_empty_and_single", testHeapEmptyAndSingle);
    runTest("heap_duplicates_and_negatives", testHeapDuplicatesAndNegatives);
    runTest("heap_large_10k", testHeapLarge10k);
    runTest("dijkstra_case1_basic", testDijkstraCase1Basic);
    runTest("dijkstra_case2_blocked_edge", testDijkstraCase2BlockedEdge);
    runTest("dijkstra_case3_unreachable", testDijkstraCase3Unreachable);
    runTest("dijkstra_case4_traffic", testDijkstraCase4Traffic);
    runTest("dijkstra_same_node_and_invalid", testDijkstraSameNodeAndInvalid);
    runTest("dijkstra_find_nearest", testDijkstraFindNearest);
    runTest("dijkstra_trace_steps", testDijkstraTraceSteps);
    runTest("queue_case1_fifo", testQueueCase1Fifo);
    runTest("queue_case2_cancel_by_id", testQueueCase2CancelById);
    runTest("queue_case3_empty_and_single", testQueueCase3EmptyAndSingle);
    runTest("queue_large_10k", testQueueLarge10k);
    runTest("bfs_hops_and_reachability", testBfsHopsAndReachability);
    runTest("bfs_blocked_roads_disconnect", testBfsBlockedRoadsDisconnect);
    runTest("hash_insert_find_erase", testHashInsertFindErase);
    runTest("hash_negatives_and_counters", testHashNegativesAndCounters);
    runTest("hash_rehash_10k", testHashRehash10k);
    runTest("hash_many_collisions", testHashManyCollisions);
    runTest("trie_case1_shared_prefix", testTrieCase1SharedPrefix);
    runTest("trie_case2_case_insensitive", testTrieCase2CaseInsensitive);
    runTest("trie_case3_no_match", testTrieCase3NoMatch);
    runTest("trie_edge_inputs", testTrieEdgeInputs);
    runTest("trie_limit_and_dedupe", testTrieLimitAndDedupe);
    runTest("trie_branch_view", testTrieBranchView);
    runTest("avl_case1_rr", testAvlCase1RR);
    runTest("avl_case2_ll", testAvlCase2LL);
    runTest("avl_case3_lr", testAvlCase3LR);
    runTest("avl_case4_rl", testAvlCase4RL);
    runTest("avl_duplicate_ratings", testAvlDuplicateRatings);
    runTest("avl_delete_and_update", testAvlDeleteAndUpdate);
    runTest("avl_large_stays_balanced", testAvlLargeStaysBalanced);
    runTest("list_push_front_newest_first", testListPushFrontNewestFirst);
    runTest("list_remove_and_empty", testListRemoveAndEmpty);
    runTest("list_large", testListLarge);
    runTest("citymap_loads_valid_json", testCityMapLoadsValidJson);
    runTest("citymap_rejects_bad_files", testCityMapRejectsBadFiles);
    runTest("citymap_real_karachi_file", testCityMapRealKarachiFile);
    runTest("models_fare_and_ride_type", testModelsFareAndRideType);
    runTest("event_order_ties", testEventOrderTies);
    runTest("surge_multiplier_steps", testSurgeMultiplierSteps);
    runTest("surge_bfs_two_hops", testSurgeBfsTwoHops);
    runTest("dispatch_nearest_driver_and_fare", testDispatchNearestDriverAndFare);
    runTest("dispatch_same_area_best_rating", testDispatchSameAreaBestRating);
    runTest("dispatch_validation_errors", testDispatchValidationErrors);
    runTest("dispatch_queue_then_served", testDispatchQueueThenServed);
    runTest("dispatch_cancel_rules", testDispatchCancelRules);
    runTest("dispatch_rating_rules", testDispatchRatingRules);
    runTest("dispatch_reroute_on_block", testDispatchRerouteOnBlock);
    runTest("dispatch_unreachable_cancels", testDispatchUnreachableCancels);
    runTest("dispatch_block_updates_planned_trip", testDispatchBlockUpdatesPlannedTrip);
    runTest("dispatch_analytics", testDispatchAnalytics);
    runTest("sim_init_and_controls", testSimInitAndControls);
    runTest("sim_same_seed_reproducible", testSimSameSeedReproducible);
    runTest("sim_rush_hour_long_run", testSimRushHourLongRun);
    runTest("sim_road_edits_validate", testSimRoadEditsValidate);
    runTest("trace_cap", testTraceCap);

    cout << endl << passedTests << " passed, " << failedTests << " failed, " << passedTests + failedTests << " total" << endl;
    if (failedTests > 0) return 1;
    return 0;
}
