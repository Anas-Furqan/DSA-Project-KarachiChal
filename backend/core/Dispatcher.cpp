#include "core/Dispatcher.h"
#include <cmath>

#include "core/BFS.h"

using namespace std;

const char* FIRST_NAMES[] = {"Ahmed", "Bilal", "Faisal", "Hamza", "Imran", "Junaid", "Kashif", "Usman",
                             "Naveed", "Owais", "Qasim", "Rizwan", "Salman", "Tariq", "Waqas", "Yasir",
                             "Zeeshan", "Adeel", "Danish", "Fahad", "Haris", "Irfan", "Kamran", "Moiz",
                             "Nadeem", "Rehan", "Sajid", "Taimoor", "Umair", "Zubair"};
const int FIRST_NAME_COUNT = 30;
const char* LAST_INITIALS = "AKMRSZHQ";

static string rideTag(int id) {
    return "ride #" + to_string(id);
}

// Dijkstra from the pickup gives pickup -> driver, the car needs driver -> pickup
static PathResult reversePath(const PathResult& p) {
    PathResult r = p;
    for (int i = 0; i < p.nodeCount; i++) r.nodes[i] = p.nodes[p.nodeCount - 1 - i];
    for (int i = 0; i < p.nodeCount - 1; i++) r.roads[i] = p.roads[p.nodeCount - 2 - i];
    return r;
}

OpResult okResult(int rideId, string message) {
    OpResult r;
    r.ok = true;
    r.httpStatus = 200;
    r.code = "";
    r.message = message;
    r.rideId = rideId;
    return r;
}

OpResult errorResult(int httpStatus, string code, string message) {
    OpResult r;
    r.ok = false;
    r.httpStatus = httpStatus;
    r.code = code;
    r.message = message;
    r.rideId = -1;
    return r;
}

Dispatcher::Dispatcher(CityMap& mapRef, MinHeap& eventsRef, SimClock& clockRef)
    : cityMap(mapRef), eventHeap(eventsRef), simClock(clockRef), rng(42) {
    drivers = new Driver[MAX_DRIVERS];
    numDrivers = 0;
    rideCapacity = 64;
    rides = new Ride*[rideCapacity];
    numRides = 0;
    riderCapacity = 64;
    riders = new Rider*[riderCapacity];
    numRiders = 0;
    nextRideId = 1001;
}

Dispatcher::~Dispatcher() {
    freeRecords();
    delete[] rides;
    delete[] riders;
    delete[] drivers;
}

void Dispatcher::freeRecords() {
    for (int i = 0; i < numRides; i++) delete rides[i];
    for (int i = 0; i < numRiders; i++) delete riders[i];
    numRides = 0;
    numRiders = 0;
}

void Dispatcher::reset(int driverTotal, unsigned seed) {
    rng.seed(seed);
    freeRecords();
    numDrivers = 0;
    driverIndex.clear();
    rideIndex.clear();
    riderIndex.clear();
    freeCount.clear();
    pendingPerArea.clear();
    requestsPerArea.clear();
    active.clear();
    waiting.clear();
    ratings.clear();
    surgeEngine.reset(cityMap.areaCount());
    nextRideId = 1001;
    if (cityMap.areaCount() == 0) return;

    for (int i = 0; i < driverTotal; i++) {
        string name = string(FIRST_NAMES[rng() % FIRST_NAME_COUNT]) + " " + LAST_INITIALS[rng() % 8] + ".";
        RideType type = (RideType)(rng() % RIDE_TYPE_COUNT);
        int area = (int)(rng() % (unsigned)cityMap.areaCount());
        int rating = 350 + (int)(rng() % 151);
        addDriver(name, type, area, rating);
    }
}

int Dispatcher::addDriver(string name, RideType type, int area, int rating) {
    if (!cityMap.isValidArea(area) || rating < 100 || rating > 500) return -1;
    if (numDrivers >= MAX_DRIVERS) return -1;
    Driver& d = drivers[numDrivers];
    d.id = numDrivers + 1;
    d.name = name;
    d.type = type;
    d.areaId = area;
    // start with 20 ratings worth of weight so one new rating moves the average gently
    d.ratingCount = 20;
    d.ratingSum = rating / 100.0 * d.ratingCount;
    d.rating = rating;
    d.status = DriverStatus::AVAILABLE;
    d.rideId = -1;
    d.motion.clear();
    d.earnings = 0;
    d.tripsCompleted = 0;
    driverIndex.insert(d.id, numDrivers);
    numDrivers++;
    addFree(d, NULL);
    ratings.insert(d.rating, d.id);
    return d.id;
}

Ride* Dispatcher::addRide(Ride* r) {
    if (numRides == rideCapacity) {
        Ride** bigger = new Ride*[rideCapacity * 2];
        for (int i = 0; i < numRides; i++) bigger[i] = rides[i];
        delete[] rides;
        rides = bigger;
        rideCapacity = rideCapacity * 2;
    }
    rides[numRides] = r;
    rideIndex.insert(r->id, numRides);
    numRides++;
    return r;
}

Rider* Dispatcher::addRider(string name) {
    if (numRiders == riderCapacity) {
        Rider** bigger = new Rider*[riderCapacity * 2];
        for (int i = 0; i < numRiders; i++) bigger[i] = riders[i];
        delete[] riders;
        riders = bigger;
        riderCapacity = riderCapacity * 2;
    }
    Rider* rider = new Rider;
    rider->name = name;
    rider->activeRideId = -1;
    riders[numRiders] = rider;
    riderIndex.insert(CityMap::normalize(name), numRiders);
    numRiders++;
    return rider;
}

Ride* Dispatcher::findRide(int id) const {
    int slot = -1;
    if (!rideIndex.find(id, slot)) return NULL;
    return rides[slot];
}

Driver* Dispatcher::findDriver(int id) const {
    int slot = -1;
    if (!driverIndex.find(id, slot)) return NULL;
    return &drivers[slot];
}

Rider* Dispatcher::findRider(string name) const {
    int slot = -1;
    if (!riderIndex.find(CityMap::normalize(name), slot)) return NULL;
    return riders[slot];
}

int Dispatcher::activeRideOf(string riderName) const {
    Rider* rider = findRider(riderName);
    if (rider == NULL) return -1;
    return rider->activeRideId;
}

double Dispatcher::legMinutes(int roadId) const {
    return cityMap.graph().effectiveWeight(roadId) / CAR_SPEED_KMH * 60.0;
}

void Dispatcher::addFree(const Driver& d, DSTrace* trace) {
    freeCount.addTo(freeKey(d.areaId, d.type), 1);
    if (trace) {
        trace->add("HashTable", "insert", "O(1) avg",
                   d.name + " free at " + cityMap.areaName(d.areaId) + " [" + rideTypeName(d.type) + "]");
    }
}

void Dispatcher::removeFree(const Driver& d, DSTrace* trace) {
    if (freeCount.get(freeKey(d.areaId, d.type)) > 0) freeCount.addTo(freeKey(d.areaId, d.type), -1);
    if (trace) trace->add("HashTable", "erase", "O(1) avg", d.name + " no longer free at " + cityMap.areaName(d.areaId));
}

int Dispatcher::freeDriversInArea(int area) const {
    int total = 0;
    for (int t = 0; t < RIDE_TYPE_COUNT; t++) total += freeCount.get(freeKey(area, (RideType)t));
    return total;
}

int Dispatcher::pendingInArea(int area) const {
    return pendingPerArea.get(area);
}

void Dispatcher::startMotion(Driver& d, const PathResult& path, double start) {
    d.motion.clear();
    if (path.nodeCount < 2) return;
    double t = start;
    for (int i = 0; i < path.nodeCount; i++) d.motion.nodes[i] = path.nodes[i];
    for (int i = 0; i < path.nodeCount - 1; i++) {
        d.motion.roads[i] = path.roads[i];
        d.motion.legStart[i] = t;
        t += legMinutes(path.roads[i]);
        d.motion.legEnd[i] = t;
    }
    d.motion.legCount = path.nodeCount - 1;
}

void Dispatcher::schedule(EventType type, Ride& r, double time, DSTrace& trace) {
    HeapItem e = makeHeapItem(time, simClock.nextSeq, r.id);
    simClock.nextSeq++;
    e.type = (int)type;
    e.version = r.eventVersion;
    eventHeap.push(e);
    trace.add("MinHeap", "push", "O(log n)",
              eventTypeName(type) + " for " + rideTag(r.id) + " at t=" + DSTrace::num(time, 1) + " min");
}

// the end of the current road that the car is closer to
int Dispatcher::nearestNode(const Driver& d) const {
    if (!d.motion.active()) return d.areaId;
    double now = simClock.now;
    if (now >= d.motion.endTime()) return d.motion.nodes[d.motion.legCount];
    int i = d.motion.legAt(now);
    double span = d.motion.legEnd[i] - d.motion.legStart[i];
    double progress = span > 0 ? (now - d.motion.legStart[i]) / span : 1.0;
    return progress < 0.5 ? d.motion.nodes[i] : d.motion.nodes[i + 1];
}

DriverPosition Dispatcher::positionOf(const Driver& d, double t) const {
    const Graph& g = cityMap.graph();
    DriverPosition pos;
    if (!d.motion.active()) {
        pos.lat = g.getNode(d.areaId).lat;
        pos.lng = g.getNode(d.areaId).lng;
        pos.fromNode = d.areaId;
        pos.toNode = d.areaId;
        pos.progress = 0;
        return pos;
    }
    int i = d.motion.legAt(t);
    double span = d.motion.legEnd[i] - d.motion.legStart[i];
    double p = span > 0 ? (t - d.motion.legStart[i]) / span : 1.0;
    if (p < 0) p = 0;
    if (p > 1) p = 1;
    const Area& a = g.getNode(d.motion.nodes[i]);
    const Area& b = g.getNode(d.motion.nodes[i + 1]);
    pos.lat = a.lat + (b.lat - a.lat) * p;
    pos.lng = a.lng + (b.lng - a.lng) * p;
    pos.fromNode = a.id;
    pos.toNode = b.id;
    pos.progress = p;
    return pos;
}

void Dispatcher::releaseDriver(Driver& d, DSTrace& trace) {
    d.areaId = nearestNode(d);
    d.motion.clear();
    d.status = DriverStatus::AVAILABLE;
    d.rideId = -1;
    addFree(d, &trace);
}

void Dispatcher::finishRide(Ride& r, RideStatus status, DSTrace& trace) {
    r.status = status;
    r.endTime = simClock.now;
    active.remove(r.id);
    Rider* rider = findRider(r.riderName);
    if (rider != NULL) {
        rider->activeRideId = -1;
        rider->history.pushFront(r.id);
    }
    trace.add("LinkedList", "pushFront", "O(1)", rideTag(r.id) + " added to " + r.riderName + "'s history");
}

void Dispatcher::forceCancel(Ride& r, string reason, DSTrace& trace) {
    if (r.status == RideStatus::WAITING) {
        if (waiting.removeById(r.id)) {
            trace.add("LinkedQueue", "removeById", "O(n)", rideTag(r.id) + " removed from waiting queue");
        }
        pendingPerArea.addTo(r.pickup, -1);
    } else if (r.status == RideStatus::ASSIGNED || r.status == RideStatus::IN_PROGRESS) {
        if (r.status == RideStatus::ASSIGNED) pendingPerArea.addTo(r.pickup, -1);
        Driver* d = findDriver(r.driverId);
        if (d != NULL) releaseDriver(*d, trace);
    }
    // any event already in the heap for this ride is now stale
    r.eventVersion++;
    r.cancelReason = reason;
    finishRide(r, RideStatus::CANCELLED, trace);
}

void Dispatcher::applyRating(Driver& d, int stars, DSTrace& trace) {
    int oldRating = d.rating;
    ratings.clearLastRotations();
    ratings.remove(d.rating, d.id);
    d.ratingSum += stars;
    d.ratingCount++;
    d.rating = (int)round(d.ratingSum / d.ratingCount * 100.0);
    ratings.insert(d.rating, d.id);
    trace.add("AVLTree", "delete+insert", "O(log n)",
              d.name + " rated " + to_string(stars) + "★: " + DSTrace::num(oldRating / 100.0, 2) + " → " +
                  DSTrace::num(d.rating / 100.0, 2));
    for (int i = 0; i < ratings.lastRotationCount(); i++) trace.add("AVLTree", "rotate", "O(1)", ratings.lastRotation(i));
}

bool Dispatcher::tryAssign(Ride& r, DSTrace& trace) {
    int area = -1;
    PathResult path = Dijkstra::findNearest(cityMap.graph(), r.pickup, freeCount, r.type, area, &trace);
    if (area == -1) return false;

    // several drivers in the closest area: take the best rated one
    Driver* best = NULL;
    for (int i = 0; i < numDrivers; i++) {
        Driver& d = drivers[i];
        if (d.status != DriverStatus::AVAILABLE || d.areaId != area || d.type != r.type) continue;
        if (best == NULL || d.rating > best->rating) best = &d;
    }
    if (best == NULL) return false;
    trace.add("Dijkstra", "nearestDriver", "O((V+E) log V)",
              best->name + " at " + cityMap.areaName(best->areaId) + ", " + DSTrace::num(path.distance, 1) + " km from pickup");
    assignDriver(r, *best, reversePath(path), trace);
    return true;
}

void Dispatcher::assignDriver(Ride& r, Driver& d, const PathResult& driverToPickup, DSTrace& trace) {
    removeFree(d, &trace);
    d.status = DriverStatus::TO_PICKUP;
    d.rideId = r.id;
    startMotion(d, driverToPickup, simClock.now);
    double arrival = simClock.now + PICKUP_DELAY_MIN;
    if (d.motion.active()) arrival = d.motion.endTime();

    r.status = RideStatus::ASSIGNED;
    r.driverId = d.id;
    r.assignTime = simClock.now;
    r.etaPickup = arrival;
    r.eventVersion++;
    schedule(EventType::DRIVER_ARRIVED_PICKUP, r, arrival, trace);
}

void Dispatcher::setTrip(Ride& r, const PathResult& trip) {
    for (int i = 0; i < trip.nodeCount; i++) r.tripNodes[i] = trip.nodes[i];
    r.tripCount = trip.nodeCount;
}

OpResult Dispatcher::requestRide(string riderName, int pickup, int drop, RideType type, bool autoGenerated,
                                 DSTrace& trace) {
    string key = CityMap::normalize(riderName);
    if (key.empty()) return errorResult(400, "MISSING_FIELD", "Rider name is required");
    if ((int)riderName.size() > MAX_NAME_LENGTH) {
        return errorResult(400, "INVALID_NAME", "Rider name must be at most 40 characters");
    }
    if (!cityMap.isValidArea(pickup)) return errorResult(400, "INVALID_AREA", "Unknown pickup area");
    if (!cityMap.isValidArea(drop)) return errorResult(400, "INVALID_AREA", "Unknown drop area");
    if (pickup == drop) return errorResult(400, "SAME_LOCATION", "Pickup and drop must be different areas");

    Rider* rider = findRider(riderName);
    int activeId = rider != NULL ? rider->activeRideId : -1;
    trace.add("HashTable", "find", "O(1) avg",
              "Rider \"" + riderName + "\" active ride? " + (activeId == -1 ? string("none") : rideTag(activeId)));
    if (activeId != -1) {
        return errorResult(409, "DUPLICATE_REQUEST", "This rider already has an active ride (" + rideTag(activeId) + ")");
    }

    PathResult trip = Dijkstra::shortestPath(cityMap.graph(), pickup, drop, &trace);
    if (!trip.found) return errorResult(409, "UNREACHABLE", "Destination unreachable due to road block");

    Ride* r = new Ride;
    r->id = nextRideId;
    nextRideId++;
    r->riderName = riderName;
    r->pickup = pickup;
    r->drop = drop;
    r->type = type;
    r->status = RideStatus::WAITING;
    r->driverId = -1;
    r->distanceKm = trip.distance;
    r->surge = surgeEngine.multiplierAt(pickup);
    r->fare = computeFare(type, trip.distance, r->surge);
    r->requestTime = simClock.now;
    r->assignTime = -1;
    r->pickupTime = -1;
    r->completeTime = -1;
    r->endTime = -1;
    r->etaPickup = -1;
    setTrip(*r, trip);
    r->cancelReason = "";
    r->rating = 0;
    r->autoGenerated = autoGenerated;
    r->eventVersion = 0;
    trace.add("BFS+HashTable", "surgeLookup", "O(1)",
              cityMap.areaName(pickup) + " surge " + DSTrace::num(r->surge, 2) + "x, fare Rs " + to_string(r->fare));

    addRide(r);
    if (rider == NULL) rider = addRider(riderName);
    rider->activeRideId = r->id;
    active.pushFront(r->id);
    pendingPerArea.addTo(pickup, 1);
    requestsPerArea.addTo(pickup, 1);
    trace.add("HashTable", "insert", "O(1) avg", rideTag(r->id) + " stored");

    if (!tryAssign(*r, trace)) {
        waiting.enqueue(r->id);
        trace.add("LinkedQueue", "enqueue", "O(1)",
                  "No free " + rideTypeName(type) + " reachable — " + rideTag(r->id) + " waits (position " +
                      to_string(waiting.size()) + ")");
    }
    return okResult(r->id, "");
}

void Dispatcher::processQueue(DSTrace& trace) {
    int n = waiting.size();
    for (int i = 0; i < n; i++) {
        int id = 0;
        if (!waiting.dequeue(id)) break;
        trace.add("LinkedQueue", "dequeue", "O(1)", "Retry " + rideTag(id));
        Ride* r = findRide(id);
        if (r == NULL || r->status != RideStatus::WAITING) continue;
        if (!BFS::isReachable(cityMap.graph(), r->pickup, r->drop, &trace)) {
            forceCancel(*r, "Destination unreachable due to road block", trace);
            continue;
        }
        if (!tryAssign(*r, trace)) {
            // back to the rear, so FIFO order is kept for the next retry
            waiting.enqueue(id);
            trace.add("LinkedQueue", "enqueue", "O(1)", rideTag(id) + " still waiting");
        }
    }
}

OpResult Dispatcher::cancelRide(int rideId, DSTrace& trace) {
    Ride* r = findRide(rideId);
    trace.add("HashTable", "find", "O(1) avg", rideTag(rideId) + (r != NULL ? " found" : " not found"));
    if (r == NULL) return errorResult(404, "RIDE_NOT_FOUND", "No ride with id " + to_string(rideId));
    if (r->status == RideStatus::COMPLETED) return errorResult(409, "ALREADY_COMPLETED", "Ride is already completed");
    if (r->status == RideStatus::CANCELLED) return errorResult(409, "ALREADY_CANCELLED", "Ride is already cancelled");
    if (r->status == RideStatus::IN_PROGRESS) {
        return errorResult(409, "CANCEL_NOT_ALLOWED", "Ride is in progress and cannot be cancelled");
    }
    bool freesDriver = r->status == RideStatus::ASSIGNED;
    forceCancel(*r, "Cancelled by rider", trace);
    if (freesDriver) processQueue(trace);
    return okResult(rideId, "Ride cancelled");
}

OpResult Dispatcher::rateRide(int rideId, int stars, DSTrace& trace) {
    if (stars < 1 || stars > 5) return errorResult(400, "INVALID_RATING", "Rating must be between 1 and 5");
    Ride* r = findRide(rideId);
    trace.add("HashTable", "find", "O(1) avg", rideTag(rideId) + (r != NULL ? " found" : " not found"));
    if (r == NULL) return errorResult(404, "RIDE_NOT_FOUND", "No ride with id " + to_string(rideId));
    if (r->status != RideStatus::COMPLETED) return errorResult(409, "NOT_COMPLETED", "Only completed rides can be rated");
    if (r->rating != 0) return errorResult(409, "ALREADY_RATED", "This ride was already rated");
    Driver* d = findDriver(r->driverId);
    if (d == NULL) return errorResult(404, "DRIVER_NOT_FOUND", "Driver no longer exists");
    r->rating = stars;
    applyRating(*d, stars, trace);
    return okResult(rideId, "Thanks for rating");
}

void Dispatcher::handleEvent(const HeapItem& e, DSTrace& trace) {
    EventType type = (EventType)e.type;
    Ride* r = findRide(e.value);
    if (r == NULL || r->eventVersion != e.version) {
        trace.add("MinHeap", "popMin", "O(log n)", "Stale " + eventTypeName(type) + " for " + rideTag(e.value) + " ignored");
        return;
    }
    trace.add("MinHeap", "popMin", "O(log n)", eventTypeName(type) + " for " + rideTag(r->id));
    if (type == EventType::DRIVER_ARRIVED_PICKUP && r->status == RideStatus::ASSIGNED) {
        onArrivedPickup(*r, trace);
    } else if (type == EventType::RIDE_COMPLETED && r->status == RideStatus::IN_PROGRESS) {
        onRideCompleted(*r, trace);
    } else if (type == EventType::DRIVER_FREE && r->status == RideStatus::COMPLETED) {
        onDriverFree(*r, trace);
    }
}

void Dispatcher::onArrivedPickup(Ride& r, DSTrace& trace) {
    Driver* d = findDriver(r.driverId);
    if (d == NULL) return;
    d->areaId = r.pickup;
    d->motion.clear();

    PathResult trip = Dijkstra::shortestPath(cityMap.graph(), r.pickup, r.drop, &trace);
    if (!trip.found) {
        forceCancel(r, "Destination unreachable due to road block", trace);
        processQueue(trace);
        return;
    }
    pendingPerArea.addTo(r.pickup, -1);
    r.status = RideStatus::IN_PROGRESS;
    r.pickupTime = simClock.now;
    setTrip(r, trip);
    d->status = DriverStatus::ON_TRIP;
    startMotion(*d, trip, simClock.now);
    schedule(EventType::RIDE_COMPLETED, r, d->motion.endTime(), trace);
}

void Dispatcher::onRideCompleted(Ride& r, DSTrace& trace) {
    Driver* d = findDriver(r.driverId);
    if (d == NULL) return;
    d->areaId = r.drop;
    d->motion.clear();
    d->earnings += r.fare;
    d->tripsCompleted++;
    r.completeTime = simClock.now;
    finishRide(r, RideStatus::COMPLETED, trace);
    // rush hour riders rate automatically (3 to 5 stars)
    if (r.autoGenerated) {
        r.rating = 3 + (int)(rng() % 3);
        applyRating(*d, r.rating, trace);
    }
    schedule(EventType::DRIVER_FREE, r, simClock.now + DROP_HANDOVER_MIN, trace);
}

void Dispatcher::onDriverFree(Ride& r, DSTrace& trace) {
    Driver* d = findDriver(r.driverId);
    if (d == NULL || d->rideId != r.id) return;
    releaseDriver(*d, trace);
    processQueue(trace);
}

bool Dispatcher::pathUsesRoad(const Ride& r, int roadId) const {
    for (int i = 0; i + 1 < r.tripCount; i++) {
        if (cityMap.graph().findRoad(r.tripNodes[i], r.tripNodes[i + 1]) == roadId) return true;
    }
    return false;
}

void Dispatcher::reroute(Ride& r, Driver& d, DSTrace& trace) {
    double now = simClock.now;
    if (!d.motion.active() || now >= d.motion.endTime()) return;
    int i = d.motion.legAt(now);
    // the car finishes the road it is on, then follows the new route
    int startNode = d.motion.nodes[i + 1];
    double startTime = d.motion.legEnd[i];
    bool toPickup = r.status == RideStatus::ASSIGNED;
    int target = toPickup ? r.pickup : r.drop;

    if (toPickup && !BFS::isReachable(cityMap.graph(), r.pickup, r.drop, &trace)) {
        forceCancel(r, "Destination unreachable due to road block", trace);
        return;
    }
    if (!BFS::isReachable(cityMap.graph(), startNode, target, &trace)) {
        if (!toPickup) {
            forceCancel(r, "Destination unreachable due to road block", trace);
            return;
        }
        // driver is cut off from the pickup: free the driver and wait again
        r.eventVersion++;
        r.status = RideStatus::WAITING;
        r.driverId = -1;
        r.etaPickup = -1;
        releaseDriver(d, trace);
        waiting.enqueue(r.id);
        trace.add("LinkedQueue", "enqueue", "O(1)", rideTag(r.id) + " back to waiting (driver cut off)");
        return;
    }

    PathResult rest = Dijkstra::shortestPath(cityMap.graph(), startNode, target, &trace);
    Motion m;
    m.nodes[0] = d.motion.nodes[i];
    m.nodes[1] = startNode;
    m.roads[0] = d.motion.roads[i];
    m.legStart[0] = d.motion.legStart[i];
    m.legEnd[0] = startTime;
    m.legCount = 1;
    double t = startTime;
    for (int k = 0; k + 1 < rest.nodeCount && m.legCount + 1 < MAX_ROUTE; k++) {
        m.nodes[m.legCount + 1] = rest.nodes[k + 1];
        m.roads[m.legCount] = rest.roads[k];
        m.legStart[m.legCount] = t;
        t += legMinutes(rest.roads[k]);
        m.legEnd[m.legCount] = t;
        m.legCount++;
    }
    d.motion = m;
    r.eventVersion++;
    if (toPickup) {
        r.etaPickup = t;
        schedule(EventType::DRIVER_ARRIVED_PICKUP, r, t, trace);
    } else {
        for (int k = 0; k <= m.legCount; k++) r.tripNodes[k] = m.nodes[k];
        r.tripCount = m.legCount + 1;
        schedule(EventType::RIDE_COMPLETED, r, t, trace);
    }
    trace.add("Dijkstra", "reroute", "O((V+E) log V)",
              rideTag(r.id) + " re-routed from " + cityMap.areaName(startNode) + ", new ETA t=" + DSTrace::num(t, 1));
}

void Dispatcher::onRoadChanged(int roadId, DSTrace& trace) {
    // copy the ids first because cancelling a ride removes it from the active list
    int count = active.size();
    int* ids = new int[count + 1];
    int k = 0;
    for (ListNode* p = active.getHead(); p != NULL; p = p->next) {
        ids[k] = p->value;
        k++;
    }
    double now = simClock.now;
    for (int j = 0; j < count; j++) {
        Ride* r = findRide(ids[j]);
        if (r == NULL) continue;
        // planned trip of rides not picked up yet
        if ((r->status == RideStatus::ASSIGNED || r->status == RideStatus::WAITING) && pathUsesRoad(*r, roadId)) {
            PathResult trip = Dijkstra::shortestPath(cityMap.graph(), r->pickup, r->drop, &trace);
            if (!trip.found) {
                bool freesDriver = r->status == RideStatus::ASSIGNED;
                forceCancel(*r, "Destination unreachable due to road block", trace);
                if (freesDriver) processQueue(trace);
                continue;
            }
            setTrip(*r, trip);
        }
        if (r->status != RideStatus::ASSIGNED && r->status != RideStatus::IN_PROGRESS) continue;
        Driver* d = findDriver(r->driverId);
        if (d == NULL || !d->motion.active() || now >= d->motion.endTime()) continue;
        int current = d->motion.legAt(now);
        bool usesRoad = false;
        for (int leg = current + 1; leg < d->motion.legCount; leg++) {
            if (d->motion.roads[leg] == roadId) usesRoad = true;
        }
        if (usesRoad) reroute(*r, *d, trace);
    }
    delete[] ids;
    processQueue(trace);
}

void Dispatcher::recomputeSurge(DSTrace* trace) {
    int demand[MAX_NODES];
    int freeAt[MAX_NODES];
    for (int a = 0; a < cityMap.areaCount(); a++) {
        demand[a] = pendingInArea(a);
        freeAt[a] = freeDriversInArea(a);
    }
    surgeEngine.recompute(cityMap.graph(), demand, freeAt, trace);
}

int Dispatcher::driverCount() const {
    return numDrivers;
}

const Driver& Dispatcher::driverAt(int i) const {
    return drivers[i];
}

int Dispatcher::rideCount() const {
    return numRides;
}

const Ride& Dispatcher::rideAt(int i) const {
    return *rides[i];
}

const LinkedList& Dispatcher::activeRides() const {
    return active;
}

const LinkedQueue& Dispatcher::queue() const {
    return waiting;
}

const AVLTree& Dispatcher::leaderboard() const {
    return ratings;
}

const SurgeEngine& Dispatcher::surge() const {
    return surgeEngine;
}

const HashTable& Dispatcher::driverTable() const {
    return driverIndex;
}

const HashTable& Dispatcher::rideTable() const {
    return rideIndex;
}

Analytics Dispatcher::analytics() const {
    Analytics a;
    a.totalRides = 0;
    a.completed = 0;
    a.cancelled = 0;
    a.active = 0;
    a.waiting = 0;
    a.hourCount = 0;
    a.busiestTotal = 0;
    for (int t = 0; t < RIDE_TYPE_COUNT; t++) a.byType[t] = 0;
    for (int h = 0; h < MAX_HOURS; h++) a.ridesPerHour[h] = 0;

    double waitSum = 0;
    double fareSum = 0;
    int waitCount = 0;
    for (int i = 0; i < numRides; i++) {
        const Ride& r = *rides[i];
        a.totalRides++;
        a.byType[(int)r.type]++;
        if (r.status == RideStatus::COMPLETED) {
            a.completed++;
            fareSum += r.fare;
            int hour = (int)(r.completeTime / 60.0);
            if (hour >= 0 && hour < MAX_HOURS) {
                a.ridesPerHour[hour]++;
                if (hour + 1 > a.hourCount) a.hourCount = hour + 1;
            }
        } else if (r.status == RideStatus::CANCELLED) {
            a.cancelled++;
        } else {
            a.active++;
            if (r.status == RideStatus::WAITING) a.waiting++;
        }
        if (r.pickupTime >= 0) {
            waitSum += r.pickupTime - r.requestTime;
            waitCount++;
        }
    }
    a.revenue = fareSum;
    a.avgFare = a.completed > 0 ? fareSum / a.completed : 0;
    a.avgWaitMin = waitCount > 0 ? waitSum / waitCount : 0;

    // top 5 areas by request count: pick the biggest remaining one 5 times
    bool used[MAX_NODES];
    for (int i = 0; i < cityMap.areaCount(); i++) used[i] = false;
    for (int k = 0; k < TOP_AREAS; k++) {
        int bestArea = -1;
        int bestCount = 0;
        for (int area = 0; area < cityMap.areaCount(); area++) {
            int c = requestsPerArea.get(area);
            if (!used[area] && c > bestCount) {
                bestArea = area;
                bestCount = c;
            }
        }
        if (bestArea == -1) break;
        used[bestArea] = true;
        a.busiestArea[k] = bestArea;
        a.busiestCount[k] = bestCount;
        a.busiestTotal++;
    }

    a.topEarnerId = -1;
    a.topEarnings = 0;
    for (int i = 0; i < numDrivers; i++) {
        if (a.topEarnerId == -1 || drivers[i].earnings > a.topEarnings) {
            a.topEarnerId = drivers[i].id;
            a.topEarnings = drivers[i].earnings;
        }
    }
    return a;
}
