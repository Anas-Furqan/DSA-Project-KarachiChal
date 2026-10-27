#include "httplib.h"
#include "json.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "api/Server.h"

using namespace std;
using json = nlohmann::json;
using httplib::Request;
using httplib::Response;

const double SIM_START_HOUR = 8.0;
const int MAX_SHOWN_BUCKETS = 64;
const int MAX_TRIE_VIEW = 60;

void sendJson(Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

json traceJson(const DSTrace& trace) {
    json list = json::array();
    for (TraceNode* t = trace.getHead(); t != NULL; t = t->next) {
        json entry;
        entry["ds"] = t->ds;
        entry["op"] = t->op;
        entry["complexity"] = t->complexity;
        entry["detail"] = t->detail;
        list.push_back(entry);
    }
    return list;
}

void sendOk(Response& res, const json& data, const DSTrace& trace) {
    json body;
    body["ok"] = true;
    body["data"] = data;
    body["trace"] = traceJson(trace);
    body["truncated"] = trace.isTruncated();
    sendJson(res, 200, body);
}

void sendError(Response& res, int status, string code, string message) {
    json body;
    body["ok"] = false;
    body["error"] = message;
    body["code"] = code;
    sendJson(res, status, body);
}

void sendFailure(Response& res, const OpResult& r) {
    sendError(res, r.httpStatus, r.code, r.message);
}

// optional '-', then 1 to 9 digits, nothing else
bool parseIntStrict(string s, int& out) {
    int start = 0;
    if (!s.empty() && s[0] == '-') start = 1;
    int digits = (int)s.size() - start;
    if (digits < 1 || digits > 9) return false;
    for (int i = start; i < (int)s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    out = atoi(s.c_str());
    return true;
}

bool parseBody(const Request& req, Response& res, json& out) {
    try {
        out = json::parse(req.body);
    } catch (exception& e) {
        sendError(res, 400, "BAD_JSON", "Request body is not valid JSON");
        return false;
    }
    if (!out.is_object()) {
        sendError(res, 400, "BAD_JSON", "Request body must be a JSON object");
        return false;
    }
    return true;
}

// accepts 7 or "7"
bool readInt(const json& body, string key, int& out, Response& res) {
    if (!body.contains(key) || body[key].is_null()) {
        sendError(res, 400, "MISSING_FIELD", "Field '" + key + "' is required");
        return false;
    }
    json v = body[key];
    if (v.is_number_integer()) {
        long long x = v.get<long long>();
        if (x >= -1000000000LL && x <= 1000000000LL) {
            out = (int)x;
            return true;
        }
    } else if (v.is_string() && parseIntStrict(v.get<string>(), out)) {
        return true;
    }
    sendError(res, 400, "INVALID_FIELD", "Field '" + key + "' must be an integer");
    return false;
}

bool readIntParam(const Request& req, string key, int& out, Response& res) {
    if (!req.has_param(key) || req.get_param_value(key).empty()) {
        sendError(res, 400, "MISSING_FIELD", "Query parameter '" + key + "' is required");
        return false;
    }
    if (!parseIntStrict(req.get_param_value(key), out)) {
        sendError(res, 400, "INVALID_FIELD", "Query parameter '" + key + "' must be an integer");
        return false;
    }
    return true;
}

bool checkArea(const CityMap& cityMap, int id, string label, Response& res) {
    if (cityMap.isValidArea(id)) return true;
    sendError(res, 400, "INVALID_AREA", "Unknown " + label + " area id " + to_string(id));
    return false;
}

string paramOrEmpty(const Request& req, string key) {
    if (!req.has_param(key)) return "";
    return req.get_param_value(key);
}

string clockLabel(double simMinutes) {
    int total = (int)(SIM_START_HOUR * 60 + simMinutes);
    int day = total / (24 * 60);
    total = total % (24 * 60);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    string label = buf;
    if (day > 0) label += " +" + to_string(day) + "d";
    return label;
}

json pathJson(const PathResult& p) {
    json list = json::array();
    for (int i = 0; i < p.nodeCount; i++) list.push_back(p.nodes[i]);
    return list;
}

json pathNamesJson(const CityMap& cityMap, const PathResult& p) {
    json list = json::array();
    for (int i = 0; i < p.nodeCount; i++) list.push_back(cityMap.areaName(p.nodes[i]));
    return list;
}

json roadsJson(const PathResult& p) {
    json list = json::array();
    for (int i = 0; i + 1 < p.nodeCount; i++) list.push_back(p.roads[i]);
    return list;
}

json rideJson(const Ride& r, const Simulator& sim) {
    const CityMap& cityMap = sim.map();
    json j;
    j["id"] = r.id;
    j["riderName"] = r.riderName;
    j["pickup"] = r.pickup;
    j["drop"] = r.drop;
    j["pickupName"] = cityMap.areaName(r.pickup);
    j["dropName"] = cityMap.areaName(r.drop);
    j["rideType"] = rideTypeName(r.type);
    j["status"] = rideStatusName(r.status);
    j["driverId"] = r.driverId;
    j["distanceKm"] = r.distanceKm;
    j["surge"] = r.surge;
    j["fare"] = r.fare;
    j["requestTime"] = r.requestTime;
    j["assignTime"] = r.assignTime;
    j["pickupTime"] = r.pickupTime;
    j["completeTime"] = r.completeTime;
    j["endTime"] = r.endTime;
    j["etaPickup"] = r.etaPickup;
    json trip = json::array();
    for (int i = 0; i < r.tripCount; i++) trip.push_back(r.tripNodes[i]);
    j["tripNodes"] = trip;
    j["cancelReason"] = r.cancelReason;
    j["rating"] = r.rating;
    j["auto"] = r.autoGenerated;
    Driver* d = sim.dispatcher().findDriver(r.driverId);
    if (d != NULL) {
        json driver;
        driver["id"] = d->id;
        driver["name"] = d->name;
        driver["rating"] = d->rating / 100.0;
        driver["type"] = rideTypeName(d->type);
        j["driver"] = driver;
        if (d->motion.active()) j["etaArrive"] = d->motion.endTime();
    }
    return j;
}

json driverJson(const Driver& d, const Simulator& sim) {
    double now = sim.now();
    DriverPosition p = sim.dispatcher().positionOf(d, now);
    json legs = json::array();
    if (d.motion.active()) {
        for (int i = d.motion.legAt(now); i < d.motion.legCount; i++) {
            json leg;
            leg["from"] = d.motion.nodes[i];
            leg["to"] = d.motion.nodes[i + 1];
            leg["start"] = d.motion.legStart[i];
            leg["end"] = d.motion.legEnd[i];
            legs.push_back(leg);
        }
    }
    json j;
    j["id"] = d.id;
    j["name"] = d.name;
    j["type"] = rideTypeName(d.type);
    j["status"] = driverStatusName(d.status);
    j["rating"] = d.rating / 100.0;
    j["areaId"] = d.areaId;
    j["lat"] = p.lat;
    j["lng"] = p.lng;
    j["from"] = p.fromNode;
    j["to"] = p.toNode;
    j["progress"] = p.progress;
    j["rideId"] = d.rideId;
    j["trips"] = d.tripsCompleted;
    j["earnings"] = d.earnings;
    j["legs"] = legs;
    return j;
}

json hashTableJson(const HashTable& t, string name) {
    int shown = t.bucketCount() < MAX_SHOWN_BUCKETS ? t.bucketCount() : MAX_SHOWN_BUCKETS;
    json chains = json::array();
    for (int b = 0; b < shown; b++) {
        json chain = json::array();
        for (HashNode* n = t.bucketHead(b); n != NULL; n = n->next) chain.push_back(n->key);
        chains.push_back(chain);
    }
    json j;
    j["table"] = name;
    j["size"] = t.size();
    j["buckets"] = t.bucketCount();
    j["loadFactor"] = t.loadFactor();
    j["rehashCount"] = t.rehashCount();
    j["longestChain"] = t.longestChain();
    j["chains"] = chains;
    j["shownBuckets"] = shown;
    return j;
}

json lastRotationsJson(const AVLTree& tree) {
    json list = json::array();
    for (int i = 0; i < tree.lastRotationCount(); i++) list.push_back(tree.lastRotation(i));
    return list;
}

ApiServer::ApiServer(Simulator& simRef, string staticFolder) : sim(simRef), staticDir(staticFolder) {
    running = false;
    http = new httplib::Server();
}

ApiServer::~ApiServer() {
    stop();
    if (simThread.joinable()) simThread.join();
    delete http;
}

bool ApiServer::isRunning() {
    lock_guard<mutex> lock(simMutex);
    return running;
}

void ApiServer::simulationLoop() {
    chrono::steady_clock::time_point last = chrono::steady_clock::now();
    while (isRunning()) {
        this_thread::sleep_for(chrono::milliseconds(100));
        chrono::steady_clock::time_point current = chrono::steady_clock::now();
        chrono::duration<double> elapsed = current - last;
        last = current;
        lock_guard<mutex> lock(simMutex);
        sim.advanceReal(elapsed.count());
    }
}

bool ApiServer::run(string host, int port) {
    registerRoutes();
    running = true;
    simThread = thread(&ApiServer::simulationLoop, this);
    bool ok = http->listen(host.c_str(), port);
    {
        lock_guard<mutex> lock(simMutex);
        running = false;
    }
    if (simThread.joinable()) simThread.join();
    return ok;
}

void ApiServer::stop() {
    {
        lock_guard<mutex> lock(simMutex);
        running = false;
    }
    if (http != NULL) http->stop();
}

// every handler locks the simulator; exceptions go to the exception handler below
void ApiServer::registerRoutes() {
    if (!staticDir.empty()) http->set_mount_point("/", staticDir);

    http->Get("/api/map", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getMap(req, res);
    });
    http->Get("/api/state", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getState(req, res);
    });
    http->Get("/api/ride", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getRide(req, res);
    });
    http->Get("/api/search", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getSearch(req, res);
    });
    http->Post("/api/ride/request", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postRideRequest(req, res);
    });
    http->Post("/api/ride/cancel", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postRideCancel(req, res);
    });
    http->Post("/api/ride/rate", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postRideRate(req, res);
    });
    http->Get("/api/rider/history", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getRiderHistory(req, res);
    });
    http->Post("/api/road/traffic", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postRoadTraffic(req, res);
    });
    http->Post("/api/road/block", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postRoadBlock(req, res);
    });
    http->Get("/api/leaderboard", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getLeaderboard(req, res);
    });
    http->Get("/api/analytics", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getAnalytics(req, res);
    });
    http->Post("/api/sim/control", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        postSimControl(req, res);
    });
    http->Get("/api/route", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getRoute(req, res);
    });
    http->Get("/api/dryrun/dijkstra", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getDryRun(req, res);
    });
    http->Get("/api/ds/snapshot", [this](const Request& req, Response& res) {
        lock_guard<mutex> lock(simMutex);
        getSnapshot(req, res);
    });

    http->set_exception_handler([](const Request&, Response& res, exception_ptr) {
        sendError(res, 500, "INTERNAL", "Internal server error");
    });
    // only fills responses that have no body yet (unknown routes, missing files)
    http->set_error_handler([](const Request& req, Response& res) {
        if (!res.body.empty()) return;
        if (req.path.rfind("/api/", 0) == 0) {
            sendError(res, res.status, "NOT_FOUND", "Unknown API route " + req.path);
        } else {
            res.set_content("Not found", "text/plain");
        }
    });
}

void ApiServer::getMap(const Request&, Response& res) {
    const Graph& g = sim.map().graph();
    json areas = json::array();
    json roads = json::array();
    for (int i = 0; i < g.nodeCount(); i++) {
        const Area& a = g.getNode(i);
        json area;
        area["id"] = a.id;
        area["name"] = a.name;
        area["lat"] = a.lat;
        area["lng"] = a.lng;
        area["degree"] = g.degree(i);
        areas.push_back(area);
    }
    for (int i = 0; i < g.roadCount(); i++) {
        const Road& r = g.getRoad(i);
        json road;
        road["id"] = r.id;
        road["from"] = r.from;
        road["to"] = r.to;
        road["km"] = r.baseKm;
        road["traffic"] = r.trafficFactor;
        road["blocked"] = r.blocked;
        roads.push_back(road);
    }
    DSTrace trace;
    trace.add("Graph", "adjacencyList", "O(V+E)", to_string(g.nodeCount()) + " areas, " + to_string(g.roadCount()) + " roads");
    json data;
    data["areas"] = areas;
    data["roads"] = roads;
    sendOk(res, data, trace);
}

void ApiServer::getState(const Request&, Response& res) {
    const Dispatcher& d = sim.dispatcher();
    json drivers = json::array();
    int freeDrivers = 0;
    for (int i = 0; i < d.driverCount(); i++) {
        drivers.push_back(driverJson(d.driverAt(i), sim));
        if (d.driverAt(i).status == DriverStatus::AVAILABLE) freeDrivers++;
    }
    json rides = json::array();
    for (ListNode* p = d.activeRides().getHead(); p != NULL; p = p->next) {
        Ride* r = d.findRide(p->value);
        if (r != NULL) rides.push_back(rideJson(*r, sim));
    }
    json queue = json::array();
    for (QueueNode* q = d.queue().getFront(); q != NULL; q = q->next) queue.push_back(q->value);

    json surge = json::array();
    for (int a = 0; a < sim.map().areaCount(); a++) {
        SurgeInfo s = d.surge().infoAt(a);
        if (s.multiplier > 1.0 || s.demand > 0) {
            json zone;
            zone["area"] = a;
            zone["multiplier"] = s.multiplier;
            zone["demand"] = s.demand;
            zone["supply"] = s.supply;
            surge.push_back(zone);
        }
    }
    // only roads that are not normal, so the poll stays small
    json changedRoads = json::array();
    const Graph& g = sim.map().graph();
    for (int i = 0; i < g.roadCount(); i++) {
        const Road& r = g.getRoad(i);
        if (r.blocked || r.trafficFactor != 1.0) {
            json road;
            road["id"] = r.id;
            road["traffic"] = r.trafficFactor;
            road["blocked"] = r.blocked;
            changedRoads.push_back(road);
        }
    }
    json counters;
    counters["active"] = d.activeRides().size();
    counters["waiting"] = d.queue().size();
    counters["freeDrivers"] = freeDrivers;
    counters["drivers"] = d.driverCount();
    counters["events"] = sim.events().size();

    json data;
    data["time"] = sim.now();
    data["clock"] = clockLabel(sim.now());
    data["speed"] = sim.getSpeed();
    data["paused"] = sim.isPaused();
    data["rushHour"] = sim.isRushHour();
    data["counters"] = counters;
    data["drivers"] = drivers;
    data["rides"] = rides;
    data["queue"] = queue;
    data["surge"] = surge;
    data["roads"] = changedRoads;
    sendOk(res, data, sim.backgroundTrace());
    sim.backgroundTrace().clear();
}

void ApiServer::getRide(const Request& req, Response& res) {
    int id = 0;
    if (!readIntParam(req, "id", id, res)) return;
    DSTrace trace;
    Ride* r = sim.dispatcher().findRide(id);
    trace.add("HashTable", "find", "O(1) avg", "ride #" + to_string(id) + (r != NULL ? " found" : " not found"));
    if (r == NULL) {
        sendError(res, 404, "RIDE_NOT_FOUND", "No ride with id " + to_string(id));
        return;
    }
    sendOk(res, rideJson(*r, sim), trace);
}

void ApiServer::getSearch(const Request& req, Response& res) {
    string q = paramOrEmpty(req, "q");
    DSTrace trace;
    TrieMatch results[5];
    int found = sim.map().search(q, results, 5, &trace);
    json list = json::array();
    for (int i = 0; i < found; i++) {
        json item;
        item["id"] = results[i].value;
        item["name"] = results[i].word;
        list.push_back(item);
    }
    json data;
    data["query"] = q;
    data["suggestions"] = list;
    sendOk(res, data, trace);
}

void ApiServer::postRideRequest(const Request& req, Response& res) {
    json body;
    if (!parseBody(req, res, body)) return;
    if (!body.contains("riderName") || !body["riderName"].is_string()) {
        sendError(res, 400, "MISSING_FIELD", "Field 'riderName' is required");
        return;
    }
    int pickup = 0;
    int drop = 0;
    if (!readInt(body, "pickupAreaId", pickup, res)) return;
    if (!readInt(body, "dropAreaId", drop, res)) return;
    if (!body.contains("rideType") || !body["rideType"].is_string()) {
        sendError(res, 400, "MISSING_FIELD", "Field 'rideType' is required");
        return;
    }
    RideType type;
    if (!parseRideType(body["rideType"].get<string>(), type)) {
        sendError(res, 400, "INVALID_RIDE_TYPE", "Ride type must be BIKE, MINI or AC");
        return;
    }
    DSTrace trace;
    OpResult r = sim.dispatcher().requestRide(body["riderName"].get<string>(), pickup, drop, type, false, trace);
    if (!r.ok) {
        sendFailure(res, r);
        return;
    }
    sendOk(res, rideJson(*sim.dispatcher().findRide(r.rideId), sim), trace);
}

void ApiServer::postRideCancel(const Request& req, Response& res) {
    json body;
    int id = 0;
    if (!parseBody(req, res, body)) return;
    if (!readInt(body, "rideId", id, res)) return;
    DSTrace trace;
    OpResult r = sim.dispatcher().cancelRide(id, trace);
    if (!r.ok) {
        sendFailure(res, r);
        return;
    }
    sendOk(res, rideJson(*sim.dispatcher().findRide(id), sim), trace);
}

void ApiServer::postRideRate(const Request& req, Response& res) {
    json body;
    int id = 0;
    int stars = 0;
    if (!parseBody(req, res, body)) return;
    if (!readInt(body, "rideId", id, res)) return;
    if (!readInt(body, "rating", stars, res)) return;
    DSTrace trace;
    OpResult r = sim.dispatcher().rateRide(id, stars, trace);
    if (!r.ok) {
        sendFailure(res, r);
        return;
    }
    sendOk(res, rideJson(*sim.dispatcher().findRide(id), sim), trace);
}

void ApiServer::getRiderHistory(const Request& req, Response& res) {
    string name = paramOrEmpty(req, "name");
    if (CityMap::normalize(name).empty()) {
        sendError(res, 400, "MISSING_FIELD", "Query parameter 'name' is required");
        return;
    }
    DSTrace trace;
    const Dispatcher& d = sim.dispatcher();
    Rider* rider = d.findRider(name);
    bool hasHistory = rider != NULL && !rider->history.isEmpty();
    trace.add("HashTable", "find", "O(1) avg", "History for \"" + name + "\"" + (hasHistory ? " found" : ": none yet"));
    json list = json::array();
    if (hasHistory) {
        for (ListNode* p = rider->history.getHead(); p != NULL; p = p->next) {
            Ride* r = d.findRide(p->value);
            if (r != NULL) list.push_back(rideJson(*r, sim));
        }
        trace.add("LinkedList", "traverse", "O(n)", to_string(rider->history.size()) + " rides, newest first");
    }
    json data;
    data["name"] = name;
    data["rides"] = list;
    data["activeRideId"] = d.activeRideOf(name);
    sendOk(res, data, trace);
}

void ApiServer::postRoadTraffic(const Request& req, Response& res) {
    json body;
    int from = 0;
    int to = 0;
    if (!parseBody(req, res, body)) return;
    if (!readInt(body, "from", from, res)) return;
    if (!readInt(body, "to", to, res)) return;
    if (!body.contains("factor") || !body["factor"].is_number()) {
        sendError(res, 400, "INVALID_TRAFFIC", "Field 'factor' must be a number between 1 and 3");
        return;
    }
    DSTrace trace;
    OpResult r = sim.setTraffic(from, to, body["factor"].get<double>(), trace);
    if (!r.ok) {
        sendFailure(res, r);
        return;
    }
    json data;
    data["message"] = r.message;
    sendOk(res, data, trace);
}

void ApiServer::postRoadBlock(const Request& req, Response& res) {
    json body;
    int from = 0;
    int to = 0;
    if (!parseBody(req, res, body)) return;
    if (!readInt(body, "from", from, res)) return;
    if (!readInt(body, "to", to, res)) return;
    if (!body.contains("blocked") || !body["blocked"].is_boolean()) {
        sendError(res, 400, "MISSING_FIELD", "Field 'blocked' must be true or false");
        return;
    }
    DSTrace trace;
    OpResult r = sim.setBlocked(from, to, body["blocked"].get<bool>(), trace);
    if (!r.ok) {
        sendFailure(res, r);
        return;
    }
    json data;
    data["message"] = r.message;
    sendOk(res, data, trace);
}

void ApiServer::getLeaderboard(const Request&, Response& res) {
    const Dispatcher& d = sim.dispatcher();
    int ratings[10];
    int ids[10];
    int found = d.leaderboard().topN(ratings, ids, 10);
    json list = json::array();
    int rank = 1;
    for (int i = 0; i < found; i++) {
        Driver* drv = d.findDriver(ids[i]);
        if (drv == NULL) continue;
        json row;
        row["rank"] = rank;
        row["id"] = drv->id;
        row["name"] = drv->name;
        row["rating"] = ratings[i] / 100.0;
        row["type"] = rideTypeName(drv->type);
        row["trips"] = drv->tripsCompleted;
        row["earnings"] = drv->earnings;
        list.push_back(row);
        rank++;
    }
    DSTrace trace;
    trace.add("AVLTree", "inOrder(topN)", "O(log n + N)",
              "Top " + to_string(list.size()) + " of " + to_string(d.leaderboard().size()) + " drivers");
    json data;
    data["top"] = list;
    data["rotations"] = lastRotationsJson(d.leaderboard());
    sendOk(res, data, trace);
}

void ApiServer::getAnalytics(const Request&, Response& res) {
    const Dispatcher& d = sim.dispatcher();
    Analytics a = d.analytics();
    json busiest = json::array();
    for (int i = 0; i < a.busiestTotal; i++) {
        json row;
        row["area"] = a.busiestArea[i];
        row["name"] = sim.map().areaName(a.busiestArea[i]);
        row["requests"] = a.busiestCount[i];
        busiest.push_back(row);
    }
    json perHour = json::array();
    for (int h = 0; h < a.hourCount; h++) perHour.push_back(a.ridesPerHour[h]);
    json byType;
    for (int t = 0; t < RIDE_TYPE_COUNT; t++) byType[rideTypeName((RideType)t)] = a.byType[t];
    json top = nullptr;
    Driver* earner = d.findDriver(a.topEarnerId);
    if (earner != NULL) {
        top = json::object();
        top["id"] = earner->id;
        top["name"] = earner->name;
        top["earnings"] = earner->earnings;
        top["trips"] = earner->tripsCompleted;
    }
    DSTrace trace;
    trace.add("HashTable", "scan", "O(n)", "Scanned " + to_string(a.totalRides) + " rides");
    trace.add("HashTable", "find", "O(1) avg", "Request count per area, top " + to_string(a.busiestTotal) + " picked");

    json data;
    data["totalRides"] = a.totalRides;
    data["completed"] = a.completed;
    data["cancelled"] = a.cancelled;
    data["active"] = a.active;
    data["waiting"] = a.waiting;
    data["avgWaitMin"] = a.avgWaitMin;
    data["avgFare"] = a.avgFare;
    data["revenue"] = a.revenue;
    data["busiest"] = busiest;
    data["ridesPerHour"] = perHour;
    data["byType"] = byType;
    data["topEarner"] = top;
    data["startHour"] = SIM_START_HOUR;
    sendOk(res, data, trace);
}

void ApiServer::postSimControl(const Request& req, Response& res) {
    json body;
    if (!parseBody(req, res, body)) return;
    if (!body.contains("action") || !body["action"].is_string()) {
        sendError(res, 400, "MISSING_FIELD", "Field 'action' is required");
        return;
    }
    string action = body["action"].get<string>();
    DSTrace trace;
    if (action == "pause") {
        sim.setPaused(true);
    } else if (action == "resume") {
        sim.setPaused(false);
    } else if (action == "speed") {
        int s = 0;
        if (!readInt(body, "value", s, res)) return;
        if (!sim.setSpeed(s)) {
            sendError(res, 400, "INVALID_SPEED", "Speed must be 1, 2 or 5");
            return;
        }
    } else if (action == "rushHour") {
        if (!body.contains("value") || !body["value"].is_boolean()) {
            sendError(res, 400, "MISSING_FIELD", "Field 'value' must be true or false");
            return;
        }
        sim.setRushHour(body["value"].get<bool>());
    } else if (action == "reset") {
        string error;
        if (!sim.reset(error)) {
            sendError(res, 500, "MAP_ERROR", error);
            return;
        }
        trace.add("Simulator", "reset", "O(V+E+D log D)", "Map reloaded, 40 drivers re-seeded");
    } else {
        sendError(res, 400, "INVALID_ACTION", "Action must be pause, resume, speed, rushHour or reset");
        return;
    }
    json data;
    data["paused"] = sim.isPaused();
    data["speed"] = sim.getSpeed();
    data["rushHour"] = sim.isRushHour();
    data["time"] = sim.now();
    sendOk(res, data, trace);
}

void ApiServer::getRoute(const Request& req, Response& res) {
    int from = 0;
    int to = 0;
    if (!readIntParam(req, "from", from, res)) return;
    if (!readIntParam(req, "to", to, res)) return;
    const CityMap& cityMap = sim.map();
    if (!checkArea(cityMap, from, "from", res)) return;
    if (!checkArea(cityMap, to, "to", res)) return;
    if (from == to) {
        sendError(res, 400, "SAME_LOCATION", "Pickup and drop must be different areas");
        return;
    }
    DSTrace trace;
    PathResult p = Dijkstra::shortestPath(cityMap.graph(), from, to, &trace);
    if (!p.found) {
        sendError(res, 409, "UNREACHABLE", "Destination unreachable due to road block");
        return;
    }
    double surge = sim.dispatcher().surge().multiplierAt(from);
    json fares;
    for (int t = 0; t < RIDE_TYPE_COUNT; t++) fares[rideTypeName((RideType)t)] = computeFare((RideType)t, p.distance, surge);
    json data;
    data["nodes"] = pathJson(p);
    data["names"] = pathNamesJson(cityMap, p);
    data["roads"] = roadsJson(p);
    data["distanceKm"] = p.distance;
    data["etaMin"] = p.distance / CAR_SPEED_KMH * 60.0;
    data["surge"] = surge;
    data["fares"] = fares;
    sendOk(res, data, trace);
}

void ApiServer::getDryRun(const Request& req, Response& res) {
    int from = 0;
    int to = 0;
    if (!readIntParam(req, "from", from, res)) return;
    if (!readIntParam(req, "to", to, res)) return;
    const CityMap& cityMap = sim.map();
    if (!checkArea(cityMap, from, "from", res)) return;
    if (!checkArea(cityMap, to, "to", res)) return;

    DijkstraTrace t;
    Dijkstra::traceSteps(cityMap.graph(), from, to, t);
    json steps = json::array();
    for (int i = 0; i < t.stepCount; i++) {
        const DijkstraStep& s = t.steps[i];
        json heap = json::array();
        for (int k = 0; k < s.heapCount; k++) {
            json item;
            item["node"] = s.heap[k].value;
            item["name"] = cityMap.areaName(s.heap[k].value);
            item["dist"] = s.heap[k].key;
            heap.push_back(item);
        }
        json relaxed = json::array();
        for (int k = 0; k < s.relaxCount; k++) {
            json r;
            r["from"] = s.relaxed[k].from;
            r["to"] = s.relaxed[k].to;
            r["old"] = s.relaxed[k].oldDist;
            r["new"] = s.relaxed[k].newDist;
            relaxed.push_back(r);
        }
        json distances = json::array();
        for (int k = 0; k < cityMap.areaCount(); k++) distances.push_back(s.dist[k]);
        json step;
        step["popped"] = s.popped;
        step["poppedName"] = cityMap.areaName(s.popped);
        step["dist"] = s.poppedDist;
        step["stale"] = s.stale;
        step["heap"] = heap;
        step["distances"] = distances;
        step["relaxed"] = relaxed;
        step["explanation"] = s.explanation;
        steps.push_back(step);
    }
    DSTrace trace;
    trace.add("Dijkstra", "traceSteps", "O((V+E) log V)", to_string(t.stepCount) + " heap pops recorded");
    json data;
    data["from"] = from;
    data["to"] = to;
    data["steps"] = steps;
    data["found"] = t.result.found;
    data["distanceKm"] = t.result.distance;
    data["path"] = pathJson(t.result);
    data["pathNames"] = pathNamesJson(cityMap, t.result);
    data["roads"] = roadsJson(t.result);
    sendOk(res, data, trace);
}

void ApiServer::getSnapshot(const Request& req, Response& res) {
    const Dispatcher& d = sim.dispatcher();
    const CityMap& cityMap = sim.map();

    // event heap in array order, index 0 is the root
    json heapItems = json::array();
    for (int i = 0; i < sim.events().size(); i++) {
        HeapItem e = sim.events().itemAt(i);
        Ride* r = d.findRide(e.value);
        json item;
        item["time"] = e.key;
        item["type"] = eventTypeName((EventType)e.type);
        item["rideId"] = e.value;
        item["stale"] = r == NULL || r->eventVersion != e.version;
        heapItems.push_back(item);
    }
    json heap;
    heap["size"] = sim.events().size();
    heap["items"] = heapItems;

    json queueItems = json::array();
    for (QueueNode* q = d.queue().getFront(); q != NULL; q = q->next) {
        Ride* r = d.findRide(q->value);
        if (r == NULL) continue;
        json item;
        item["rideId"] = r->id;
        item["rider"] = r->riderName;
        item["pickupName"] = cityMap.areaName(r->pickup);
        item["dropName"] = cityMap.areaName(r->drop);
        item["rideType"] = rideTypeName(r->type);
        item["waitedMin"] = sim.now() - r->requestTime;
        queueItems.push_back(item);
    }
    json queue;
    queue["size"] = d.queue().size();
    queue["items"] = queueItems;

    json hash;
    if (paramOrEmpty(req, "table") == "rides") {
        hash = hashTableJson(d.rideTable(), "rides");
    } else {
        hash = hashTableJson(d.driverTable(), "drivers");
    }

    string prefix = CityMap::normalize(paramOrEmpty(req, "prefix"));
    if (prefix.size() > 40) prefix = prefix.substr(0, 40);
    TrieViewNode trieView[MAX_TRIE_VIEW];
    int trieCount = cityMap.trie().branchView(prefix, trieView, MAX_TRIE_VIEW);
    json trieNodes = json::array();
    for (int i = 0; i < trieCount; i++) {
        json n;
        n["ch"] = trieView[i].ch == '\0' ? string("") : string(1, trieView[i].ch);
        n["parent"] = trieView[i].parent;
        n["depth"] = trieView[i].depth;
        n["isEnd"] = trieView[i].isEnd;
        n["onPath"] = trieView[i].onPath;
        n["word"] = trieView[i].word;
        trieNodes.push_back(n);
    }
    json trie;
    trie["prefix"] = prefix;
    trie["matched"] = cityMap.trie().matchedLength(prefix);
    trie["nodes"] = trieNodes;
    trie["words"] = cityMap.trie().wordCount();
    trie["nodeCount"] = cityMap.trie().nodeCount();

    const AVLTree& tree = d.leaderboard();
    AVLViewNode avlView[MAX_DRIVERS];
    int avlCount = tree.view(avlView, MAX_DRIVERS);
    json avlNodes = json::array();
    for (int i = 0; i < avlCount; i++) {
        Driver* drv = d.findDriver(avlView[i].driverId);
        json n;
        n["id"] = avlView[i].driverId;
        n["name"] = drv != NULL ? drv->name : "?";
        n["rating"] = avlView[i].rating / 100.0;
        n["height"] = avlView[i].height;
        n["balance"] = avlView[i].balance;
        n["left"] = avlView[i].left;
        n["right"] = avlView[i].right;
        avlNodes.push_back(n);
    }
    json rotationCounts;
    rotationCounts["LL"] = tree.rotationCount("LL");
    rotationCounts["RR"] = tree.rotationCount("RR");
    rotationCounts["LR"] = tree.rotationCount("LR");
    rotationCounts["RL"] = tree.rotationCount("RL");
    json avl;
    avl["size"] = tree.size();
    avl["height"] = tree.height();
    avl["nodes"] = avlNodes;
    avl["lastRotations"] = lastRotationsJson(tree);
    avl["rotationCounts"] = rotationCounts;

    json data;
    data["time"] = sim.now();
    data["heap"] = heap;
    data["queue"] = queue;
    data["hash"] = hash;
    data["trie"] = trie;
    data["avl"] = avl;
    DSTrace trace;
    sendOk(res, data, trace);
}
