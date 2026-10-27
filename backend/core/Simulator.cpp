#include "core/Simulator.h"

using namespace std;

Simulator::Simulator() : rides(cityMap, eventHeap, simClock), rng(SIM_SEED) {
    simClock.now = 0;
    simClock.nextSeq = 0;
    paused = false;
    speed = 1;
    rushHour = false;
    nextSpawnAt = 0;
    rushRiderCounter = 0;
}

bool Simulator::init(string mapFile, string& error) {
    mapPath = mapFile;
    return reset(error);
}

bool Simulator::reset(string& error) {
    if (!cityMap.loadFromFile(mapPath, error)) return false;
    eventHeap.clear();
    simClock.now = 0;
    simClock.nextSeq = 0;
    rng.seed(SIM_SEED);
    paused = false;
    speed = 1;
    rushHour = false;
    nextSpawnAt = 0;
    rushRiderCounter = 0;
    background.clear();
    rides.reset(DRIVER_COUNT, SIM_SEED);
    rides.recomputeSurge(NULL);
    return true;
}

void Simulator::setPaused(bool p) {
    paused = p;
}

bool Simulator::isPaused() const {
    return paused;
}

bool Simulator::setSpeed(int s) {
    if (s != 1 && s != 2 && s != 5) return false;
    speed = s;
    return true;
}

int Simulator::getSpeed() const {
    return speed;
}

void Simulator::setRushHour(bool on) {
    if (on && !rushHour) nextSpawnAt = simClock.now;
    rushHour = on;
}

bool Simulator::isRushHour() const {
    return rushHour;
}

void Simulator::advanceReal(double realSeconds) {
    if (paused || realSeconds <= 0) return;
    advanceSim(realSeconds * speed);
}

// handle every event whose time is up, in time order
void Simulator::advanceSim(double minutes) {
    if (minutes <= 0) return;
    double target = simClock.now + minutes;
    HeapItem e;
    while (eventHeap.peek(e) && e.key <= target) {
        eventHeap.popMin(e);
        if (e.key > simClock.now) simClock.now = e.key;
        rides.handleEvent(e, background);
    }
    simClock.now = target;
    if (rushHour && simClock.now >= nextSpawnAt) {
        spawnRushHourRequests();
        nextSpawnAt = simClock.now + RUSH_SPAWN_EVERY_MIN;
    }
    rides.recomputeSurge(NULL);
}

void Simulator::spawnRushHourRequests() {
    int areas = cityMap.areaCount();
    if (areas < 2 || rides.queue().size() >= RUSH_QUEUE_CAP) return;
    int count = 1 + (int)(rng() % 3);
    for (int i = 0; i < count; i++) {
        int pickup = (int)(rng() % (unsigned)areas);
        int drop = (int)(rng() % (unsigned)(areas - 1));
        if (drop >= pickup) drop++;
        RideType type = (RideType)(rng() % RIDE_TYPE_COUNT);
        rushRiderCounter++;
        string name = "Rider " + to_string(rushRiderCounter);
        rides.requestRide(name, pickup, drop, type, true, background);
    }
}

OpResult Simulator::setTraffic(int from, int to, double factor, DSTrace& trace) {
    Graph& g = cityMap.graph();
    if (!g.isValidNode(from) || !g.isValidNode(to)) return errorResult(400, "INVALID_AREA", "Unknown area id");
    if (!(factor >= 1.0 && factor <= 3.0)) {
        return errorResult(400, "INVALID_TRAFFIC", "Traffic factor must be between 1 and 3");
    }
    int road = g.findRoad(from, to);
    string label = cityMap.areaName(from) + " - " + cityMap.areaName(to);
    trace.add("Graph", "findRoad", "O(deg)", label + (road == -1 ? ": no road" : ": found"));
    if (road == -1) return errorResult(404, "ROAD_NOT_FOUND", "No road between these areas");
    g.setTraffic(from, to, factor);
    trace.add("Graph", "setTraffic", "O(deg)", "Traffic x" + DSTrace::num(factor, 1) + " on " + label);
    rides.onRoadChanged(road, trace);
    return okResult(-1, "Traffic updated");
}

OpResult Simulator::setBlocked(int from, int to, bool blocked, DSTrace& trace) {
    Graph& g = cityMap.graph();
    if (!g.isValidNode(from) || !g.isValidNode(to)) return errorResult(400, "INVALID_AREA", "Unknown area id");
    int road = g.findRoad(from, to);
    string label = cityMap.areaName(from) + " - " + cityMap.areaName(to);
    trace.add("Graph", "findRoad", "O(deg)", label + (road == -1 ? ": no road" : ": found"));
    if (road == -1) return errorResult(404, "ROAD_NOT_FOUND", "No road between these areas");
    if (g.getRoad(road).blocked == blocked) {
        return okResult(-1, blocked ? "Road already blocked" : "Road already open");
    }
    g.setBlocked(from, to, blocked);
    trace.add("Graph", blocked ? "blockEdge" : "unblockEdge", "O(deg)", label + (blocked ? " blocked" : " reopened"));
    rides.onRoadChanged(road, trace);
    return okResult(-1, blocked ? "Road blocked" : "Road reopened");
}

double Simulator::now() const {
    return simClock.now;
}

CityMap& Simulator::map() {
    return cityMap;
}

const CityMap& Simulator::map() const {
    return cityMap;
}

Dispatcher& Simulator::dispatcher() {
    return rides;
}

const Dispatcher& Simulator::dispatcher() const {
    return rides;
}

const MinHeap& Simulator::events() const {
    return eventHeap;
}

DSTrace& Simulator::backgroundTrace() {
    return background;
}
