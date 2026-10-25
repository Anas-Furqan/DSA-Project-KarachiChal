#include "core/Simulator.h"

Simulator::Simulator() : dispatcher_(map_, events_, clock_), rng_(kSeed) {}

bool Simulator::init(const std::string& mapPath, std::string& error) {
    mapPath_ = mapPath;
    return reset(error);
}

bool Simulator::reset(std::string& error) {
    if (!map_.loadFromFile(mapPath_, error)) return false;
    events_.clear();
    clock_ = SimClock();
    rng_.seed(kSeed);
    paused_ = false;
    speed_ = 1;
    rushHour_ = false;
    nextSpawnAt_ = 0.0;
    rushRiderCounter_ = 0;
    background_.clear();
    dispatcher_.reset(kDriverCount, kSeed);
    dispatcher_.recomputeSurge(nullptr);
    return true;
}

bool Simulator::setSpeed(int s) {
    if (s != 1 && s != 2 && s != 5) return false;
    speed_ = s;
    return true;
}

void Simulator::setRushHour(bool on) {
    if (on && !rushHour_) nextSpawnAt_ = clock_.now;   // first burst right away
    rushHour_ = on;
}

void Simulator::advanceReal(double realSeconds) {
    if (paused_ || realSeconds <= 0) return;
    advanceSim(realSeconds * speed_);   // 1 real s = 1 sim min at 1x
}

void Simulator::advanceSim(double minutes) {
    if (minutes <= 0) return;
    double target = clock_.now + minutes;
    Event e{};
    while (events_.peek(e) && e.time <= target) {
        events_.popMin(e);
        if (e.time > clock_.now) clock_.now = e.time;
        dispatcher_.handleEvent(e, background_);
    }
    clock_.now = target;
    if (rushHour_ && clock_.now >= nextSpawnAt_) {
        spawnRushHourRequests();
        nextSpawnAt_ = clock_.now + kRushSpawnEveryMin;
    }
    dispatcher_.recomputeSurge(nullptr);
}

void Simulator::spawnRushHourRequests() {
    int areas = map_.areaCount();
    if (areas < 2 || dispatcher_.queue().size() >= kRushQueueCap) return;
    int count = 1 + (int)(rng_() % 3);
    for (int i = 0; i < count; ++i) {
        int pickup = (int)(rng_() % (unsigned)areas);
        int drop = (int)(rng_() % (unsigned)(areas - 1));
        if (drop >= pickup) ++drop;   // never equal to pickup
        RideType type = (RideType)(rng_() % kRideTypeCount);
        std::string name = "Rider " + std::to_string(++rushRiderCounter_);
        dispatcher_.requestRide(name, pickup, drop, type, true, background_);
    }
}

OpResult Simulator::setTraffic(int from, int to, double factor, DSTrace& trace) {
    Graph& g = map_.graph();
    if (!g.isValidNode(from) || !g.isValidNode(to)) return OpResult::fail(400, "INVALID_AREA", "Unknown area id");
    if (!(factor >= 1.0 && factor <= 3.0))
        return OpResult::fail(400, "INVALID_TRAFFIC", "Traffic factor must be between 1 and 3");
    int road = g.findRoad(from, to);
    trace.add("Graph", "findRoad", "O(deg)", map_.areaName(from) + " - " + map_.areaName(to) + (road == -1 ? ": no road" : ": found"));
    if (road == -1) return OpResult::fail(404, "ROAD_NOT_FOUND", "No road between these areas");
    g.setTraffic(from, to, factor);
    trace.add("Graph", "setTraffic", "O(deg)", "Traffic x" + DSTrace::num(factor, 1) + " on " + map_.areaName(from) + " - " + map_.areaName(to));
    dispatcher_.onRoadChanged(road, trace);
    return OpResult::success(-1, "Traffic updated");
}

OpResult Simulator::setBlocked(int from, int to, bool blocked, DSTrace& trace) {
    Graph& g = map_.graph();
    if (!g.isValidNode(from) || !g.isValidNode(to)) return OpResult::fail(400, "INVALID_AREA", "Unknown area id");
    int road = g.findRoad(from, to);
    trace.add("Graph", "findRoad", "O(deg)", map_.areaName(from) + " - " + map_.areaName(to) + (road == -1 ? ": no road" : ": found"));
    if (road == -1) return OpResult::fail(404, "ROAD_NOT_FOUND", "No road between these areas");
    if (g.road(road).blocked == blocked)   // idempotent: nothing to re-route
        return OpResult::success(-1, blocked ? "Road already blocked" : "Road already open");
    g.setBlocked(from, to, blocked);
    trace.add("Graph", blocked ? "blockEdge" : "unblockEdge", "O(deg)",
              map_.areaName(from) + " - " + map_.areaName(to) + (blocked ? " blocked" : " reopened"));
    dispatcher_.onRoadChanged(road, trace);
    return OpResult::success(-1, blocked ? "Road blocked" : "Road reopened");
}

DSTrace Simulator::takeBackgroundTrace() {
    DSTrace out = background_;
    background_.clear();
    return out;
}
