#include "api/Server.h"

#include <chrono>
#include <cstdio>

#include "httplib.h"
#include "json.hpp"

using nlohmann::json;
using httplib::Request;
using httplib::Response;

namespace {

const double kSimStartHour = 8.0;   // sim t=0 is shown as 08:00

// --------------------------------------------------------------- responses

void sendJson(Response& res, int status, const json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

json traceJson(const DSTrace& t) {
    json arr = json::array();
    for (const TraceEntry& e : t.entries())
        arr.push_back({{"ds", e.ds}, {"op", e.op}, {"complexity", e.complexity}, {"detail", e.detail}});
    return arr;
}

void sendOk(Response& res, const json& data, const DSTrace& trace) {
    sendJson(res, 200, {{"ok", true}, {"data", data}, {"trace", traceJson(trace)}, {"truncated", trace.truncated()}});
}

void sendError(Response& res, int status, const std::string& code, const std::string& message) {
    sendJson(res, status, {{"ok", false}, {"error", message}, {"code", code}});
}

void sendFailure(Response& res, const OpResult& r) { sendError(res, r.httpStatus, r.code, r.message); }

// ------------------------------------------------------------------ input

// Strict integer parse: optional '-', 1..9 digits, nothing else.
bool parseIntStrict(const std::string& s, int& out) {
    size_t i = (!s.empty() && s[0] == '-') ? 1 : 0;
    if (i == s.size() || s.size() - i > 9) return false;
    for (size_t k = i; k < s.size(); ++k)
        if (s[k] < '0' || s[k] > '9') return false;
    out = std::stoi(s);
    return true;
}

bool parseBody(const Request& req, Response& res, json& out) {
    try {
        out = json::parse(req.body);
    } catch (const std::exception&) {
        sendError(res, 400, "BAD_JSON", "Request body is not valid JSON");
        return false;
    }
    if (!out.is_object()) {
        sendError(res, 400, "BAD_JSON", "Request body must be a JSON object");
        return false;
    }
    return true;
}

// Accepts 7 or "7". Missing -> MISSING_FIELD, anything else -> INVALID_FIELD.
bool readInt(const json& body, const char* key, int& out, Response& res) {
    if (!body.contains(key) || body[key].is_null()) {
        sendError(res, 400, "MISSING_FIELD", std::string("Field '") + key + "' is required");
        return false;
    }
    const json& v = body[key];
    if (v.is_number_integer()) {
        long long x = v.get<long long>();
        if (x >= -1000000000LL && x <= 1000000000LL) {
            out = (int)x;
            return true;
        }
    } else if (v.is_string() && parseIntStrict(v.get<std::string>(), out)) {
        return true;
    }
    sendError(res, 400, "INVALID_FIELD", std::string("Field '") + key + "' must be an integer");
    return false;
}

bool readIntParam(const Request& req, const char* key, int& out, Response& res) {
    if (!req.has_param(key) || req.get_param_value(key).empty()) {
        sendError(res, 400, "MISSING_FIELD", std::string("Query parameter '") + key + "' is required");
        return false;
    }
    if (!parseIntStrict(req.get_param_value(key), out)) {
        sendError(res, 400, "INVALID_FIELD", std::string("Query parameter '") + key + "' must be an integer");
        return false;
    }
    return true;
}

bool readArea(const CityMap& map, int id, const char* label, Response& res) {
    if (map.isValidArea(id)) return true;
    sendError(res, 400, "INVALID_AREA", std::string("Unknown ") + label + " area id " + std::to_string(id));
    return false;
}

// -------------------------------------------------------------- serialise

std::string clockLabel(double simMinutes) {
    int total = (int)(kSimStartHour * 60 + simMinutes);
    int day = total / (24 * 60);
    total %= 24 * 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    return day > 0 ? std::string(buf) + " +" + std::to_string(day) + "d" : buf;
}

json pathNames(const CityMap& map, const std::vector<int>& nodes) {
    json arr = json::array();
    for (int n : nodes) arr.push_back(map.areaName(n));
    return arr;
}

json rideJson(const Ride& r, const Simulator& sim) {
    const CityMap& map = sim.map();
    const Driver* d = sim.dispatcher().findDriver(r.driverId);
    json j = {{"id", r.id},
              {"riderName", r.riderName},
              {"pickup", r.pickup},
              {"drop", r.drop},
              {"pickupName", map.areaName(r.pickup)},
              {"dropName", map.areaName(r.drop)},
              {"rideType", toString(r.type)},
              {"status", toString(r.status)},
              {"driverId", r.driverId},
              {"distanceKm", r.distanceKm},
              {"surge", r.surge},
              {"fare", r.fare},
              {"requestTime", r.requestTime},
              {"assignTime", r.assignTime},
              {"pickupTime", r.pickupTime},
              {"completeTime", r.completeTime},
              {"endTime", r.endTime},
              {"etaPickup", r.etaPickup},
              {"tripNodes", r.tripNodes},
              {"cancelReason", r.cancelReason},
              {"rating", r.rating},
              {"auto", r.autoGenerated}};
    if (d) {
        j["driver"] = {{"id", d->id}, {"name", d->name}, {"rating", d->ratingCenti / 100.0}, {"type", toString(d->type)}};
        if (d->motion.active()) j["etaArrive"] = d->motion.endTime();
    }
    return j;
}

json driverJson(const Driver& d, const Simulator& sim) {
    double now = sim.now();
    DriverPosition p = sim.dispatcher().positionOf(d, now);
    json legs = json::array();
    if (d.motion.active()) {
        for (int i = d.motion.legAt(now); i < (int)d.motion.roads.size(); ++i) {
            legs.push_back({{"from", d.motion.nodes[i]},
                            {"to", d.motion.nodes[i + 1]},
                            {"start", d.motion.legStart[i]},
                            {"end", d.motion.legEnd[i]}});
        }
    }
    return {{"id", d.id},
            {"name", d.name},
            {"type", toString(d.type)},
            {"status", toString(d.status)},
            {"rating", d.ratingCenti / 100.0},
            {"areaId", d.areaId},
            {"lat", p.lat},
            {"lng", p.lng},
            {"from", p.fromNode},
            {"to", p.toNode},
            {"progress", p.progress},
            {"rideId", d.rideId},
            {"trips", d.tripsCompleted},
            {"earnings", d.earnings},
            {"legs", legs}};
}

json routeFares(double km, double surge) {
    json f = json::object();
    for (int t = 0; t < kRideTypeCount; ++t) f[toString((RideType)t)] = computeFare((RideType)t, km, surge);
    return f;
}

}  // namespace

// =================================================================== server

ApiServer::ApiServer(Simulator& sim, const std::string& staticDir)
    : sim_(sim), staticDir_(staticDir), http_(new httplib::Server()) {}

ApiServer::~ApiServer() {
    stop();
    delete http_;
}

void ApiServer::simulationLoop() {
    auto last = std::chrono::steady_clock::now();
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last).count();
        last = now;
        std::lock_guard<std::mutex> lock(mutex_);
        sim_.advanceReal(elapsed);
    }
}

bool ApiServer::run(const std::string& host, int port) {
    registerRoutes();
    running_ = true;
    simThread_ = std::thread(&ApiServer::simulationLoop, this);
    bool ok = http_->listen(host, port);   // blocks
    running_ = false;
    if (simThread_.joinable()) simThread_.join();
    return ok;
}

void ApiServer::stop() {
    running_ = false;
    if (http_) http_->stop();
}

void ApiServer::registerRoutes() {
    httplib::Server& s = *http_;
    if (!staticDir_.empty()) s.set_mount_point("/", staticDir_);

    // wrap every handler: lock the simulator, never let an exception escape
    auto route = [this](void (ApiServer::*handler)(const Request&, Response&)) {
        return [this, handler](const Request& req, Response& res) {
            std::lock_guard<std::mutex> lock(mutex_);
            try {
                (this->*handler)(req, res);
            } catch (const std::exception& e) {
                sendError(res, 500, "INTERNAL", std::string("Internal error: ") + e.what());
            }
        };
    };

    s.Get("/api/map", route(&ApiServer::getMap));
    s.Get("/api/state", route(&ApiServer::getState));
    s.Get("/api/ride", route(&ApiServer::getRide));
    s.Get("/api/search", route(&ApiServer::getSearch));
    s.Post("/api/ride/request", route(&ApiServer::postRideRequest));
    s.Post("/api/ride/cancel", route(&ApiServer::postRideCancel));
    s.Post("/api/ride/rate", route(&ApiServer::postRideRate));
    s.Get("/api/rider/history", route(&ApiServer::getRiderHistory));
    s.Post("/api/road/traffic", route(&ApiServer::postRoadTraffic));
    s.Post("/api/road/block", route(&ApiServer::postRoadBlock));
    s.Get("/api/leaderboard", route(&ApiServer::getLeaderboard));
    s.Get("/api/analytics", route(&ApiServer::getAnalytics));
    s.Post("/api/sim/control", route(&ApiServer::postSimControl));
    s.Get("/api/route", route(&ApiServer::getRoute));
    s.Get("/api/dryrun/dijkstra", route(&ApiServer::getDryRun));
    s.Get("/api/ds/snapshot", route(&ApiServer::getSnapshot));

    s.set_exception_handler([](const Request&, Response& res, std::exception_ptr) {
        sendError(res, 500, "INTERNAL", "Internal server error");
    });
    // only fills bodies nobody wrote (unknown routes, missing static files)
    s.set_error_handler([](const Request& req, Response& res) {
        if (!res.body.empty()) return;
        if (req.path.rfind("/api/", 0) == 0) sendError(res, res.status, "NOT_FOUND", "Unknown API route " + req.path);
        else res.set_content("Not found", "text/plain");
    });
}

// ================================================================ handlers

void ApiServer::getMap(const Request&, Response& res) {
    const Graph& g = sim_.map().graph();
    json areas = json::array(), roads = json::array();
    for (int i = 0; i < g.nodeCount(); ++i) {
        const GraphNode& n = g.node(i);
        areas.push_back({{"id", n.id}, {"name", n.name}, {"lat", n.lat}, {"lng", n.lng}, {"degree", g.neighbors(i).size()}});
    }
    for (int i = 0; i < g.roadCount(); ++i) {
        const Road& r = g.road(i);
        roads.push_back({{"id", r.id}, {"from", r.from}, {"to", r.to}, {"km", r.baseKm},
                         {"traffic", r.trafficFactor}, {"blocked", r.blocked}});
    }
    DSTrace trace;
    trace.add("Graph", "adjacencyList", "O(V+E)", std::to_string(g.nodeCount()) + " areas, " + std::to_string(g.roadCount()) + " roads");
    sendOk(res, {{"areas", areas}, {"roads", roads}}, trace);
}

void ApiServer::getState(const Request&, Response& res) {
    const Dispatcher& d = sim_.dispatcher();
    json drivers = json::array(), rides = json::array(), queue = json::array(), surge = json::array();
    int freeCount = 0;
    d.drivers().forEach([&](int, const Driver& drv) {
        drivers.push_back(driverJson(drv, sim_));
        if (drv.status == DriverStatus::AVAILABLE) ++freeCount;
    });
    d.activeRides().forEach([&](int id, const int&) {
        const Ride* r = d.findRide(id);
        if (r) rides.push_back(rideJson(*r, sim_));
    });
    d.queue().forEach([&](const PendingRequest& p) { queue.push_back(p.id); });
    for (int a = 0; a < sim_.map().areaCount(); ++a) {
        const SurgeInfo& s = d.surge().infoAt(a);
        if (s.multiplier > 1.0 || s.demand > 0)
            surge.push_back({{"area", a}, {"multiplier", s.multiplier}, {"demand", s.demand}, {"supply", s.supply}});
    }
    json roadsChanged = json::array();   // only non-default roads, so the poll stays small
    const Graph& g = sim_.map().graph();
    for (int i = 0; i < g.roadCount(); ++i) {
        const Road& r = g.road(i);
        if (r.blocked || r.trafficFactor != 1.0) roadsChanged.push_back({{"id", r.id}, {"traffic", r.trafficFactor}, {"blocked", r.blocked}});
    }
    json data = {{"time", sim_.now()},
                 {"clock", clockLabel(sim_.now())},
                 {"speed", sim_.speed()},
                 {"paused", sim_.paused()},
                 {"rushHour", sim_.rushHour()},
                 {"counters", {{"active", d.activeRides().size()}, {"waiting", d.queue().size()},
                               {"freeDrivers", freeCount}, {"drivers", d.drivers().size()},
                               {"events", sim_.events().size()}}},
                 {"drivers", drivers},
                 {"rides", rides},
                 {"queue", queue},
                 {"surge", surge},
                 {"roads", roadsChanged}};
    sendOk(res, data, sim_.takeBackgroundTrace());
}

void ApiServer::getRide(const Request& req, Response& res) {
    int id = 0;
    if (!readIntParam(req, "id", id, res)) return;
    DSTrace trace;
    const Ride* r = sim_.dispatcher().findRide(id);
    trace.add("HashTable", "find", "O(1) avg", "ride #" + std::to_string(id) + (r ? " found" : " not found"));
    if (!r) return sendError(res, 404, "RIDE_NOT_FOUND", "No ride with id " + std::to_string(id));
    sendOk(res, rideJson(*r, sim_), trace);
}

void ApiServer::getSearch(const Request& req, Response& res) {
    std::string q = req.has_param("q") ? req.get_param_value("q") : "";
    DSTrace trace;
    json list = json::array();
    for (const TrieMatch& m : sim_.map().search(q, 5, &trace)) list.push_back({{"id", m.value}, {"name", m.word}});
    sendOk(res, {{"query", q}, {"suggestions", list}}, trace);
}

void ApiServer::postRideRequest(const Request& req, Response& res) {
    json body;
    if (!parseBody(req, res, body)) return;
    if (!body.contains("riderName") || !body["riderName"].is_string())
        return sendError(res, 400, "MISSING_FIELD", "Field 'riderName' is required");
    int pickup = 0, drop = 0;
    if (!readInt(body, "pickupAreaId", pickup, res) || !readInt(body, "dropAreaId", drop, res)) return;
    if (!body.contains("rideType") || !body["rideType"].is_string())
        return sendError(res, 400, "MISSING_FIELD", "Field 'rideType' is required");
    RideType type;
    if (!parseRideType(body["rideType"].get<std::string>(), type))
        return sendError(res, 400, "INVALID_RIDE_TYPE", "Ride type must be BIKE, MINI or AC");

    DSTrace trace;
    OpResult r = sim_.dispatcher().requestRide(body["riderName"].get<std::string>(), pickup, drop, type, false, trace);
    if (!r.ok) return sendFailure(res, r);
    sendOk(res, rideJson(*sim_.dispatcher().findRide(r.rideId), sim_), trace);
}

void ApiServer::postRideCancel(const Request& req, Response& res) {
    json body;
    int id = 0;
    if (!parseBody(req, res, body) || !readInt(body, "rideId", id, res)) return;
    DSTrace trace;
    OpResult r = sim_.dispatcher().cancelRide(id, trace);
    if (!r.ok) return sendFailure(res, r);
    sendOk(res, rideJson(*sim_.dispatcher().findRide(id), sim_), trace);
}

void ApiServer::postRideRate(const Request& req, Response& res) {
    json body;
    int id = 0, stars = 0;
    if (!parseBody(req, res, body) || !readInt(body, "rideId", id, res) || !readInt(body, "rating", stars, res)) return;
    DSTrace trace;
    OpResult r = sim_.dispatcher().rateRide(id, stars, trace);
    if (!r.ok) return sendFailure(res, r);
    sendOk(res, rideJson(*sim_.dispatcher().findRide(id), sim_), trace);
}

void ApiServer::getRiderHistory(const Request& req, Response& res) {
    std::string name = req.has_param("name") ? req.get_param_value("name") : "";
    if (CityMap::normalize(name).empty()) return sendError(res, 400, "MISSING_FIELD", "Query parameter 'name' is required");
    DSTrace trace;
    json list = json::array();
    const LinkedList<int>* hist = sim_.dispatcher().historyOf(name);
    trace.add("HashTable", "find", "O(1) avg", "History for \"" + name + "\"" + (hist ? " found" : ": none yet"));
    if (hist) {
        hist->forEach([&](int id) {
            const Ride* r = sim_.dispatcher().findRide(id);
            if (r) list.push_back(rideJson(*r, sim_));
        });
        trace.add("LinkedList", "traverse", "O(n)", std::to_string(hist->size()) + " rides, newest first");
    }
    int active = sim_.dispatcher().activeRideOf(name);
    sendOk(res, {{"name", name}, {"rides", list}, {"activeRideId", active}}, trace);
}

void ApiServer::postRoadTraffic(const Request& req, Response& res) {
    json body;
    int from = 0, to = 0;
    if (!parseBody(req, res, body) || !readInt(body, "from", from, res) || !readInt(body, "to", to, res)) return;
    if (!body.contains("factor") || !body["factor"].is_number())
        return sendError(res, 400, "INVALID_TRAFFIC", "Field 'factor' must be a number between 1 and 3");
    DSTrace trace;
    OpResult r = sim_.setTraffic(from, to, body["factor"].get<double>(), trace);
    if (!r.ok) return sendFailure(res, r);
    sendOk(res, {{"message", r.message}}, trace);
}

void ApiServer::postRoadBlock(const Request& req, Response& res) {
    json body;
    int from = 0, to = 0;
    if (!parseBody(req, res, body) || !readInt(body, "from", from, res) || !readInt(body, "to", to, res)) return;
    if (!body.contains("blocked") || !body["blocked"].is_boolean())
        return sendError(res, 400, "MISSING_FIELD", "Field 'blocked' must be true or false");
    DSTrace trace;
    OpResult r = sim_.setBlocked(from, to, body["blocked"].get<bool>(), trace);
    if (!r.ok) return sendFailure(res, r);
    sendOk(res, {{"message", r.message}}, trace);
}

void ApiServer::getLeaderboard(const Request&, Response& res) {
    const Dispatcher& d = sim_.dispatcher();
    DSTrace trace;
    json list = json::array();
    int rank = 1;
    for (const DriverKey& k : d.leaderboard().topN(10)) {
        const Driver* drv = d.findDriver(k.driverId);
        if (!drv) continue;
        list.push_back({{"rank", rank++}, {"id", drv->id}, {"name", drv->name}, {"rating", k.ratingCenti / 100.0},
                        {"type", toString(drv->type)}, {"trips", drv->tripsCompleted}, {"earnings", drv->earnings}});
    }
    trace.add("AVLTree", "inOrder(topN)", "O(log n + N)", "Top " + std::to_string(list.size()) + " of " + std::to_string(d.leaderboard().size()) + " drivers");
    sendOk(res, {{"top", list}, {"rotations", d.lastRotations()}}, trace);
}

void ApiServer::getAnalytics(const Request&, Response& res) {
    const Dispatcher& d = sim_.dispatcher();
    Analytics a = d.analytics();
    json busiest = json::array();
    for (const AreaStat& s : a.busiest) busiest.push_back({{"area", s.area}, {"name", sim_.map().areaName(s.area)}, {"requests", s.requests}});
    json byType = json::object();
    for (int t = 0; t < kRideTypeCount; ++t) byType[toString((RideType)t)] = a.byType[t];
    json top = nullptr;
    const Driver* earner = d.findDriver(a.topEarnerId);
    if (earner) top = {{"id", earner->id}, {"name", earner->name}, {"earnings", earner->earnings}, {"trips", earner->tripsCompleted}};
    DSTrace trace;
    trace.add("HashTable", "forEach", "O(n)", "Scanned " + std::to_string(a.totalRides) + " rides");
    trace.add("MinHeap", "top-5", "O(V log 5)", "Busiest areas kept in a size-5 heap");
    sendOk(res,
           {{"totalRides", a.totalRides}, {"completed", a.completed}, {"cancelled", a.cancelled}, {"active", a.active},
            {"waiting", a.waiting}, {"avgWaitMin", a.avgWaitMin}, {"avgFare", a.avgFare}, {"revenue", a.revenue},
            {"busiest", busiest}, {"ridesPerHour", a.ridesPerHour}, {"byType", byType}, {"topEarner", top},
            {"startHour", kSimStartHour}},
           trace);
}

void ApiServer::postSimControl(const Request& req, Response& res) {
    json body;
    if (!parseBody(req, res, body)) return;
    if (!body.contains("action") || !body["action"].is_string())
        return sendError(res, 400, "MISSING_FIELD", "Field 'action' is required");
    std::string action = body["action"].get<std::string>();
    DSTrace trace;
    if (action == "pause") {
        sim_.setPaused(true);
    } else if (action == "resume") {
        sim_.setPaused(false);
    } else if (action == "speed") {
        int s = 0;
        if (!readInt(body, "value", s, res)) return;
        if (!sim_.setSpeed(s)) return sendError(res, 400, "INVALID_SPEED", "Speed must be 1, 2 or 5");
    } else if (action == "rushHour") {
        if (!body.contains("value") || !body["value"].is_boolean())
            return sendError(res, 400, "MISSING_FIELD", "Field 'value' must be true or false");
        sim_.setRushHour(body["value"].get<bool>());
    } else if (action == "reset") {
        std::string err;
        if (!sim_.reset(err)) return sendError(res, 500, "MAP_ERROR", err);
        trace.add("Simulator", "reset", "O(V+E+D log D)", "Map reloaded, 40 drivers re-seeded");
    } else {
        return sendError(res, 400, "INVALID_ACTION", "Action must be pause, resume, speed, rushHour or reset");
    }
    sendOk(res, {{"paused", sim_.paused()}, {"speed", sim_.speed()}, {"rushHour", sim_.rushHour()}, {"time", sim_.now()}}, trace);
}

void ApiServer::getRoute(const Request& req, Response& res) {
    int from = 0, to = 0;
    if (!readIntParam(req, "from", from, res) || !readIntParam(req, "to", to, res)) return;
    const CityMap& map = sim_.map();
    if (!readArea(map, from, "from", res) || !readArea(map, to, "to", res)) return;
    if (from == to) return sendError(res, 400, "SAME_LOCATION", "Pickup and drop must be different areas");
    DSTrace trace;
    PathResult p = Dijkstra::shortestPath(map.graph(), from, to, &trace);
    if (!p.found) return sendError(res, 409, "UNREACHABLE", "Destination unreachable due to road block");
    double surge = sim_.dispatcher().surge().multiplierAt(from);
    sendOk(res,
           {{"nodes", p.nodes}, {"names", pathNames(map, p.nodes)}, {"roads", p.roads}, {"distanceKm", p.distance},
            {"etaMin", p.distance / Dispatcher::kCarSpeedKmh * 60.0}, {"surge", surge}, {"fares", routeFares(p.distance, surge)}},
           trace);
}

void ApiServer::getDryRun(const Request& req, Response& res) {
    int from = 0, to = 0;
    if (!readIntParam(req, "from", from, res) || !readIntParam(req, "to", to, res)) return;
    const CityMap& map = sim_.map();
    if (!readArea(map, from, "from", res) || !readArea(map, to, "to", res)) return;
    DijkstraTrace t = Dijkstra::traceSteps(map.graph(), from, to);
    json steps = json::array();
    for (const DijkstraStep& s : t.steps) {
        json heap = json::array(), relaxed = json::array();
        for (const HeapItem& h : s.heap) heap.push_back({{"node", h.node}, {"name", map.areaName(h.node)}, {"dist", h.dist}});
        for (const DijkstraRelax& r : s.relaxed)
            relaxed.push_back({{"from", r.from}, {"to", r.to}, {"old", r.oldDist}, {"new", r.newDist}});
        steps.push_back({{"popped", s.popped}, {"poppedName", map.areaName(s.popped)}, {"dist", s.poppedDist},
                         {"stale", s.stale}, {"heap", heap}, {"distances", s.dist}, {"relaxed", relaxed},
                         {"explanation", s.explanation}});
    }
    DSTrace trace;
    trace.add("Dijkstra", "traceSteps", "O((V+E) log V)", std::to_string(t.steps.size()) + " heap pops recorded");
    sendOk(res,
           {{"from", from}, {"to", to}, {"steps", steps}, {"found", t.result.found}, {"distanceKm", t.result.distance},
            {"path", t.result.nodes}, {"pathNames", pathNames(map, t.result.nodes)}, {"roads", t.result.roads}},
           trace);
}

void ApiServer::getSnapshot(const Request& req, Response& res) {
    const Dispatcher& d = sim_.dispatcher();
    const CityMap& map = sim_.map();

    // Event-scheduler heap, raw array order (index 0 = root)
    json heap = json::array();
    for (const Event& e : sim_.events().snapshot()) {
        const Ride* r = d.findRide(e.rideId);
        heap.push_back({{"time", e.time}, {"type", toString(e.type)}, {"rideId", e.rideId},
                        {"stale", !r || r->eventVersion != e.version}});
    }

    // Waiting queue, front -> rear
    json queue = json::array();
    d.queue().forEach([&](const PendingRequest& p) {
        const Ride* r = d.findRide(p.id);
        if (!r) return;
        queue.push_back({{"rideId", r->id}, {"rider", r->riderName}, {"pickupName", map.areaName(r->pickup)},
                         {"dropName", map.areaName(r->drop)}, {"rideType", toString(r->type)},
                         {"waitedMin", sim_.now() - r->requestTime}});
    });

    // Hash table buckets (drivers or rides table)
    std::string table = req.has_param("table") ? req.get_param_value("table") : "drivers";
    json buckets = json::array();
    json hash;
    const int kMaxBuckets = 64;
    auto dumpTable = [&](const auto& t, const std::string& name) {
        int shown = t.bucketCount() < kMaxBuckets ? t.bucketCount() : kMaxBuckets;
        for (int b = 0; b < shown; ++b) {
            json chain = json::array();
            t.forEachInBucket(b, [&chain](int key) { chain.push_back(key); });
            buckets.push_back(chain);
        }
        hash = {{"table", name}, {"size", t.size()}, {"buckets", t.bucketCount()}, {"loadFactor", t.loadFactor()},
                {"rehashCount", t.rehashCount()}, {"longestChain", t.longestChain()}, {"chains", buckets},
                {"shownBuckets", shown}};
    };
    if (table == "rides") dumpTable(d.rides(), "rides");
    else dumpTable(d.drivers(), "drivers");

    // Trie branch for the current prefix
    std::string prefix = CityMap::normalize(req.has_param("prefix") ? req.get_param_value("prefix") : "");
    if (prefix.size() > 40) prefix = prefix.substr(0, 40);
    json trieNodes = json::array();
    for (const TrieViewNode& n : map.trie().branchView(prefix, 60)) {
        trieNodes.push_back({{"ch", n.ch ? std::string(1, n.ch) : std::string("")}, {"parent", n.parent},
                             {"depth", n.depth}, {"isEnd", n.isEnd}, {"onPath", n.onPath}, {"word", n.word}});
    }
    json trie = {{"prefix", prefix}, {"matched", map.trie().matchedLength(prefix)}, {"nodes", trieNodes},
                 {"words", map.trie().wordCount()}, {"nodeCount", map.trie().nodeCount()}};

    // AVL leaderboard tree
    json avlNodes = json::array();
    for (const AVLViewNode& n : d.leaderboard().view()) {
        const Driver* drv = d.findDriver(n.key.driverId);
        avlNodes.push_back({{"id", n.key.driverId}, {"name", drv ? drv->name : "?"}, {"rating", n.key.ratingCenti / 100.0},
                            {"height", n.height}, {"balance", n.balance}, {"left", n.left}, {"right", n.right}});
    }
    const AVLTree& avl = d.leaderboard();
    json rotations = {{"LL", avl.rotationCount("LL")}, {"RR", avl.rotationCount("RR")},
                      {"LR", avl.rotationCount("LR")}, {"RL", avl.rotationCount("RL")}};

    DSTrace trace;
    sendOk(res,
           {{"time", sim_.now()},
            {"heap", {{"size", sim_.events().size()}, {"items", heap}}},
            {"queue", {{"size", d.queue().size()}, {"items", queue}}},
            {"hash", hash},
            {"trie", trie},
            {"avl", {{"size", avl.size()}, {"height", avl.height()}, {"nodes", avlNodes},
                     {"lastRotations", d.lastRotations()}, {"rotationCounts", rotations}}}},
           trace);
}
