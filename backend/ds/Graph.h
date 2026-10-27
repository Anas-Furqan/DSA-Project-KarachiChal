#pragma once
#include <string>

using namespace std;

const int MAX_NODES = 64;
const int MAX_ROADS = 256;
const int MAX_ROUTE = MAX_NODES + 2;

struct Area {
    int id;
    string name;
    double lat;
    double lng;
};

struct Road {
    int id;
    int from;
    int to;
    double baseKm;
    double trafficFactor;
    bool blocked;
};

struct EdgeNode {
    int to;
    int roadId;
    EdgeNode* next;
};

class Graph {
public:
    Graph();
    ~Graph();

    int addNode(string name, double lat, double lng);
    int addEdge(int u, int v, double km);

    int nodeCount() const;
    int roadCount() const;
    bool isValidNode(int u) const;
    bool isValidRoad(int r) const;
    const Area& getNode(int u) const;
    const Road& getRoad(int r) const;

    EdgeNode* neighbors(int u) const;
    int degree(int u) const;
    int findRoad(int u, int v) const;

    bool setTraffic(int u, int v, double factor);
    bool setBlocked(int u, int v, bool blocked);
    double effectiveWeight(int roadId) const;
    bool isUsable(int roadId) const;

    void clear();

private:
    Area nodes[MAX_NODES];
    EdgeNode* head[MAX_NODES];
    Road roads[MAX_ROADS];
    int numNodes;
    int numRoads;
};
