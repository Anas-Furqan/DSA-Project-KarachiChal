#include "ds/Graph.h"

using namespace std;

Graph::Graph() {
    numNodes = 0;
    numRoads = 0;
    for (int i = 0; i < MAX_NODES; i++) head[i] = NULL;
}

Graph::~Graph() {
    clear();
}

int Graph::addNode(string name, double lat, double lng) {
    if (numNodes >= MAX_NODES) return -1;
    int id = numNodes;
    nodes[id].id = id;
    nodes[id].name = name;
    nodes[id].lat = lat;
    nodes[id].lng = lng;
    head[id] = NULL;
    numNodes++;
    return id;
}

int Graph::addEdge(int u, int v, double km) {
    if (!isValidNode(u) || !isValidNode(v) || u == v) return -1;
    if (!(km > 0)) return -1;
    if (numRoads >= MAX_ROADS) return -1;
    if (findRoad(u, v) != -1) return -1;

    int id = numRoads;
    roads[id].id = id;
    roads[id].from = u;
    roads[id].to = v;
    roads[id].baseKm = km;
    roads[id].trafficFactor = 1.0;
    roads[id].blocked = false;
    numRoads++;

    // add at the tail so neighbours keep the order roads were added in
    int ends[2] = {u, v};
    int others[2] = {v, u};
    for (int k = 0; k < 2; k++) {
        EdgeNode* e = new EdgeNode;
        e->to = others[k];
        e->roadId = id;
        e->next = NULL;
        if (head[ends[k]] == NULL) {
            head[ends[k]] = e;
        } else {
            EdgeNode* last = head[ends[k]];
            while (last->next != NULL) last = last->next;
            last->next = e;
        }
    }
    return id;
}

int Graph::nodeCount() const {
    return numNodes;
}

int Graph::roadCount() const {
    return numRoads;
}

bool Graph::isValidNode(int u) const {
    return u >= 0 && u < numNodes;
}

bool Graph::isValidRoad(int r) const {
    return r >= 0 && r < numRoads;
}

const Area& Graph::getNode(int u) const {
    return nodes[u];
}

const Road& Graph::getRoad(int r) const {
    return roads[r];
}

EdgeNode* Graph::neighbors(int u) const {
    if (!isValidNode(u)) return NULL;
    return head[u];
}

int Graph::degree(int u) const {
    int count = 0;
    for (EdgeNode* e = neighbors(u); e != NULL; e = e->next) count++;
    return count;
}

int Graph::findRoad(int u, int v) const {
    if (!isValidNode(u) || !isValidNode(v)) return -1;
    for (EdgeNode* e = head[u]; e != NULL; e = e->next) {
        if (e->to == v) return e->roadId;
    }
    return -1;
}

bool Graph::setTraffic(int u, int v, double factor) {
    if (!(factor >= 1.0 && factor <= 3.0)) return false;
    int r = findRoad(u, v);
    if (r == -1) return false;
    roads[r].trafficFactor = factor;
    return true;
}

bool Graph::setBlocked(int u, int v, bool blocked) {
    int r = findRoad(u, v);
    if (r == -1) return false;
    roads[r].blocked = blocked;
    return true;
}

double Graph::effectiveWeight(int roadId) const {
    if (!isValidRoad(roadId)) return 0;
    return roads[roadId].baseKm * roads[roadId].trafficFactor;
}

bool Graph::isUsable(int roadId) const {
    return isValidRoad(roadId) && !roads[roadId].blocked;
}

void Graph::clear() {
    for (int i = 0; i < numNodes; i++) {
        EdgeNode* e = head[i];
        while (e != NULL) {
            EdgeNode* next = e->next;
            delete e;
            e = next;
        }
        head[i] = NULL;
    }
    numNodes = 0;
    numRoads = 0;
}
