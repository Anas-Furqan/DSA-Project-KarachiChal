#include "core/Dispatcher.h"

#include "core/BFS.h"

namespace {

const char* kFirstNames[] = {"Ahmed", "Bilal", "Faisal", "Hamza", "Imran", "Junaid", "Kashif", "Usman",
                             "Naveed", "Owais", "Qasim", "Rizwan", "Salman", "Tariq", "Waqas", "Yasir",
                             "Zeeshan", "Adeel", "Danish", "Fahad", "Haris", "Irfan", "Kamran", "Moiz",
                             "Nadeem", "Rehan", "Sajid", "Taimoor", "Umair", "Zubair"};
const char* kLastInitials = "AKMRSZHQ";

// Reverse a path in place: Dijkstra from the pickup gives pickup -> driver,
// the car needs driver -> pickup. Roads are undirected so weights are equal.
PathResult reversed(const PathResult& p) {
    PathResult r = p;
    for (int i = 0, j = (int)r.nodes.size() - 1; i < j; ++i, --j) {
        int t = r.nodes[i]; r.nodes[i] = r.nodes[j]; r.nodes[j] = t;
    }
    for (int i = 0, j = (int)r.roads.size() - 1; i < j; ++i, --j) {
        int t = r.roads[i]; r.roads[i] = r.roads[j]; r.roads[j] = t;
    }
    return r;
}

std::string rideTag(int id) { return "ride #" + std::to_string(id); }

}  // namespace

Dispatcher::Dispatcher(CityMap& map, MinHeap<Event>& events, SimClock& clock)
    : map_(map), events_(events), clock_(clock), rng_(42) {}

// ------------------------------------------------------------------ setup

void Dispatcher::reset(int driverCount, unsigned seed) {
    rng_.seed(seed);
    drivers_.clear();
    rides_.clear();
    activeRides_.clear();
    freeIndex_.clear();
    activeByRider_.clear();
    history_.clear();
    pendingPerArea_.clear();
    requestsPerArea_.clear();
    queue_.clear();
    leaderboard_.clear();
    lastRotations_.clear();
    surge_.reset(map_.areaCount());
    nextRideId_ = 1001;
    nextDriverId_ = 1;
    if (map_.areaCount() == 0) return;

    const int nameCount = (int)(sizeof(kFirstNames) / sizeof(kFirstNames[0]));
    for (int i = 0; i < driverCount; ++i) {
        std::string name = std::string(kFirstNames[rng_() % nameCount]) + " " + kLastInitials[rng_() % 8] + ".";
        RideType type = (RideType)(rng_() % kRideTypeCount);
        int area = (int)(rng_() % (unsigned)map_.areaCount());
        int ratingCenti = 350 + (int)(rng_() % 151);   // 3.50 .. 5.00
        addDriver(name, type, area, ratingCenti);
    }
}

int Dispatcher::addDriver(const std::string& name, RideType type, int area, int ratingCenti) {
    if (!map_.isValidArea(area) || ratingCenti < 100 || ratingCenti > 500) return -1;
    Driver d;
    d.id = nextDriverId_++;
    d.name = name;
    d.type = type;
    d.areaId = area;
    d.ratingCount = 20;   // prior weight so one new rating moves the average gently
    d.ratingSum = ratingCenti / 100.0 * d.ratingCount;
    d.ratingCenti = ratingCenti;
    drivers_.insert(d.id, d);
    addFree(d, nullptr);
    leaderboard_.insert(DriverKey{d.ratingCenti, d.id});
    return d.id;
}

// ---------------------------------------------------------------- helpers

double Dispatcher::legMinutes(int roadId) const {
    return map_.graph().effectiveWeight(roadId) / kCarSpeedKmh * 60.0;
}

void Dispatcher::addFree(const Driver& d, DSTrace* trace) {
    freeIndex_.getOrInsert(freeKey(d.areaId, d.type), LinkedList<int>()).pushFront(d.id);
    if (trace) trace->add("HashTable", "insert", "O(1) avg",
                          d.name + " free at " + map_.areaName(d.areaId) + " [" + toString(d.type) + "]");
}

void Dispatcher::removeFree(const Driver& d, DSTrace* trace) {
    LinkedList<int>* list = freeIndex_.find(freeKey(d.areaId, d.type));
    if (list) list->remove(d.id);
    if (trace) trace->add("HashTable", "erase", "O(1) avg", d.name + " no longer free at " + map_.areaName(d.areaId));
}

void Dispatcher::addPending(int area, int delta) {
    int& count = pendingPerArea_.getOrInsert(area, 0);
    count += delta;
    if (count < 0) count = 0;
}

int Dispatcher::freeDriversInArea(int area) const {
    int total = 0;
    for (int t = 0; t < kRideTypeCount; ++t) {
        const LinkedList<int>* l = freeIndex_.find(freeKey(area, (RideType)t));
        if (l) total += l->size();
    }
    return total;
}

int Dispatcher::pendingInArea(int area) const {
    const int* c = pendingPerArea_.find(area);
    return c ? *c : 0;
}

int Dispatcher::activeRideOf(const std::string& riderName) const {
    const int* id = activeByRider_.find(riderKey(riderName));
    return id ? *id : -1;
}

const LinkedList<int>* Dispatcher::historyOf(const std::string& riderName) const {
    return history_.find(riderKey(riderName));
}

void Dispatcher::startMotion(Driver& d, const PathResult& path, double start) {
    d.motion.clear();
    if (path.nodes.empty()) return;
    d.motion.nodes = path.nodes;
    d.motion.roads = path.roads;
    double t = start;
    for (int road : path.roads) {
        d.motion.legStart.push_back(t);
        t += legMinutes(road);
        d.motion.legEnd.push_back(t);
    }
}

void Dispatcher::schedule(EventType type, Ride& r, double time, DSTrace& trace) {
    events_.push(Event{time, type, r.id, r.eventVersion, clock_.nextSeq++});
    trace.add("MinHeap", "push", "O(log n)",
              std::string(toString(type)) + " for " + rideTag(r.id) + " at t=" + DSTrace::num(time, 1) + " min");
}

int Dispatcher::nearestNode(const Driver& d) const {
    if (!d.motion.active()) return d.areaId;
    double now = clock_.now;
    if (now >= d.motion.endTime()) return d.motion.nodes.back();
    int i = d.motion.legAt(now);
    double span = d.motion.legEnd[i] - d.motion.legStart[i];
    double progress = span > 0 ? (now - d.motion.legStart[i]) / span : 1.0;
    return progress < 0.5 ? d.motion.nodes[i] : d.motion.nodes[i + 1];
}

DriverPosition Dispatcher::positionOf(const Driver& d, double t) const {
    const Graph& g = map_.graph();
    if (!d.motion.active()) {
        const GraphNode& n = g.node(d.areaId);
        return DriverPosition{n.lat, n.lng, d.areaId, d.areaId, 0.0};
    }
    int i = d.motion.legAt(t);
    double span = d.motion.legEnd[i] - d.motion.legStart[i];
    double p = span > 0 ? (t - d.motion.legStart[i]) / span : 1.0;
    if (p < 0) p = 0;
    if (p > 1) p = 1;
    const GraphNode& a = g.node(d.motion.nodes[i]);
    const GraphNode& b = g.node(d.motion.nodes[i + 1]);
    return DriverPosition{a.lat + (b.lat - a.lat) * p, a.lng + (b.lng - a.lng) * p, a.id, b.id, p};
}

// Driver stops wherever it is (snapped to the closer end of its road) and
// becomes free again.
void Dispatcher::releaseDriver(Driver& d, DSTrace& trace) {
    d.areaId = nearestNode(d);
    d.motion.clear();
    d.status = DriverStatus::AVAILABLE;
    d.rideId = -1;
    addFree(d, &trace);
}

void Dispatcher::finishRide(Ride& r, RideStatus status, DSTrace& trace) {
    r.status = status;
    r.endTime = clock_.now;
    activeRides_.erase(r.id);
    activeByRider_.erase(riderKey(r.riderName));
    history_.getOrInsert(riderKey(r.riderName), LinkedList<int>()).pushFront(r.id);
    trace.add("LinkedList", "pushFront", "O(1)", rideTag(r.id) + " added to " + r.riderName + "'s history");
}

void Dispatcher::forceCancel(Ride& r, const std::string& reason, DSTrace& trace) {
    if (r.status == RideStatus::WAITING) {
        if (queue_.removeById(r.id))
            trace.add("LinkedQueue", "removeById", "O(n)", rideTag(r.id) + " removed from waiting queue");
        addPending(r.pickup, -1);
    } else if (r.status == RideStatus::ASSIGNED || r.status == RideStatus::IN_PROGRESS) {
        if (r.status == RideStatus::ASSIGNED) addPending(r.pickup, -1);
        Driver* d = drivers_.find(r.driverId);
        if (d) releaseDriver(*d, trace);
    }
    r.eventVersion++;   // any scheduled event for this ride is now stale
    r.cancelReason = reason;
    finishRide(r, RideStatus::CANCELLED, trace);
}

void Dispatcher::applyRating(Driver& d, int stars, DSTrace& trace) {
    DriverKey oldKey{d.ratingCenti, d.id};
    leaderboard_.remove(oldKey);
    lastRotations_ = leaderboard_.lastRotations();
    d.ratingSum += stars;
    d.ratingCount += 1;
    d.ratingCenti = (int)std::lround(d.ratingSum / d.ratingCount * 100.0);
    leaderboard_.insert(DriverKey{d.ratingCenti, d.id});
    for (const std::string& rot : leaderboard_.lastRotations()) lastRotations_.push_back(rot);
    trace.add("AVLTree", "delete+insert", "O(log n)",
              d.name + " rated " + std::to_string(stars) + "★: " + DSTrace::num(oldKey.ratingCenti / 100.0, 2) +
                  " → " + DSTrace::num(d.ratingCenti / 100.0, 2));
    for (const std::string& rot : lastRotations_) trace.add("AVLTree", "rotate", "O(1)", rot);
}

// ---------------------------------------------------------------- matching

bool Dispatcher::tryAssign(Ride& r, DSTrace& trace) {
    RideType type = r.type;
    std::function<bool(int)> hasFreeDriver = [this, type, &trace](int area) {
        const LinkedList<int>* l = freeIndex_.find(freeKey(area, type));
        bool found = l && !l->isEmpty();
        trace.add("HashTable", "find", "O(1) avg",
                  std::string("Free ") + toString(type) + " at " + map_.areaName(area) + "? " + (found ? "yes" : "no"));
        return found;
    };
    NearestResult nearest = Dijkstra::findNearest(map_.graph(), r.pickup, hasFreeDriver, &trace);
    if (nearest.node == -1) return false;

    // several drivers in the winning area: pick the best rated
    Driver* best = nullptr;
    freeIndex_.find(freeKey(nearest.node, type))->forEach([this, &best](int id) {
        Driver* d = drivers_.find(id);
        if (d && (!best || d->ratingCenti > best->ratingCenti)) best = d;
    });
    if (!best) return false;
    trace.add("Dijkstra", "nearestDriver", "O((V+E) log V)",
              best->name + " at " + map_.areaName(best->areaId) + ", " + DSTrace::num(nearest.path.distance, 1) +
                  " km from pickup");
    assignDriver(r, *best, reversed(nearest.path), trace);
    return true;
}

void Dispatcher::assignDriver(Ride& r, Driver& d, const PathResult& driverToPickup, DSTrace& trace) {
    removeFree(d, &trace);
    d.status = DriverStatus::TO_PICKUP;
    d.rideId = r.id;
    startMotion(d, driverToPickup, clock_.now);
    double arrival = d.motion.active() ? d.motion.endTime() : clock_.now + kPickupDelayMin;

    r.status = RideStatus::ASSIGNED;
    r.driverId = d.id;
    r.assignTime = clock_.now;
    r.etaPickup = arrival;
    r.eventVersion++;
    schedule(EventType::DRIVER_ARRIVED_PICKUP, r, arrival, trace);
}

OpResult Dispatcher::requestRide(const std::string& riderName, int pickup, int drop, RideType type,
                                 bool autoGenerated, DSTrace& trace) {
    std::string name = CityMap::normalize(riderName);
    if (name.empty()) return OpResult::fail(400, "MISSING_FIELD", "Rider name is required");
    if ((int)riderName.size() > kMaxNameLength)
        return OpResult::fail(400, "INVALID_NAME", "Rider name must be at most 40 characters");
    if (!map_.isValidArea(pickup)) return OpResult::fail(400, "INVALID_AREA", "Unknown pickup area");
    if (!map_.isValidArea(drop)) return OpResult::fail(400, "INVALID_AREA", "Unknown drop area");
    if (pickup == drop) return OpResult::fail(400, "SAME_LOCATION", "Pickup and drop must be different areas");

    int active = activeRideOf(name);
    trace.add("HashTable", "find", "O(1) avg", "Rider \"" + riderName + "\" active ride? " + (active == -1 ? "none" : rideTag(active)));
    if (active != -1)
        return OpResult::fail(409, "DUPLICATE_REQUEST", "This rider already has an active ride (" + rideTag(active) + ")");

    PathResult trip = Dijkstra::shortestPath(map_.graph(), pickup, drop, &trace);
    if (!trip.found)
        return OpResult::fail(409, "UNREACHABLE", "Destination unreachable due to road block");

    Ride r;
    r.id = nextRideId_++;
    r.riderName = riderName;
    r.pickup = pickup;
    r.drop = drop;
    r.type = type;
    r.requestTime = clock_.now;
    r.distanceKm = trip.distance;
    r.tripNodes = trip.nodes;
    r.surge = surge_.multiplierAt(pickup);
    r.fare = computeFare(type, trip.distance, r.surge);
    r.autoGenerated = autoGenerated;
    trace.add("BFS+HashTable", "surgeLookup", "O(1)",
              map_.areaName(pickup) + " surge " + DSTrace::num(r.surge, 2) + "x, fare Rs " + std::to_string(r.fare));

    rides_.insert(r.id, r);
    activeRides_.insert(r.id, 1);
    activeByRider_.insert(riderKey(riderName), r.id);
    addPending(pickup, +1);
    requestsPerArea_.getOrInsert(pickup, 0) += 1;
    trace.add("HashTable", "insert", "O(1) avg", rideTag(r.id) + " stored");

    Ride* stored = rides_.find(r.id);
    if (!tryAssign(*stored, trace)) {
        queue_.enqueue(PendingRequest{stored->id, pickup});
        trace.add("LinkedQueue", "enqueue", "O(1)",
                  "No free " + std::string(toString(type)) + " reachable — " + rideTag(r.id) + " waits (position " +
                      std::to_string(queue_.size()) + ")");
    }
    return OpResult::success(r.id);
}

void Dispatcher::processQueue(DSTrace& trace) {
    int n = queue_.size();
    for (int i = 0; i < n; ++i) {
        PendingRequest p{0, 0};
        if (!queue_.dequeue(p)) break;
        trace.add("LinkedQueue", "dequeue", "O(1)", "Retry " + rideTag(p.id));
        Ride* r = rides_.find(p.id);
        if (!r || r->status != RideStatus::WAITING) continue;
        if (!BFS::isReachable(map_.graph(), r->pickup, r->drop, &trace)) {
            forceCancel(*r, "Destination unreachable due to road block", trace);
            continue;
        }
        if (!tryAssign(*r, trace)) {
            queue_.enqueue(p);   // keep FIFO order for the next retry
            trace.add("LinkedQueue", "enqueue", "O(1)", rideTag(p.id) + " still waiting");
        }
    }
}

// --------------------------------------------------------------- lifecycle

OpResult Dispatcher::cancelRide(int rideId, DSTrace& trace) {
    Ride* r = rides_.find(rideId);
    trace.add("HashTable", "find", "O(1) avg", rideTag(rideId) + (r ? " found" : " not found"));
    if (!r) return OpResult::fail(404, "RIDE_NOT_FOUND", "No ride with id " + std::to_string(rideId));
    if (r->status == RideStatus::COMPLETED)
        return OpResult::fail(409, "ALREADY_COMPLETED", "Ride is already completed");
    if (r->status == RideStatus::CANCELLED)
        return OpResult::fail(409, "ALREADY_CANCELLED", "Ride is already cancelled");
    if (r->status == RideStatus::IN_PROGRESS)
        return OpResult::fail(409, "CANCEL_NOT_ALLOWED", "Ride is in progress and cannot be cancelled");

    bool freesDriver = r->status == RideStatus::ASSIGNED;
    forceCancel(*r, "Cancelled by rider", trace);
    if (freesDriver) processQueue(trace);
    return OpResult::success(rideId, "Ride cancelled");
}

OpResult Dispatcher::rateRide(int rideId, int stars, DSTrace& trace) {
    if (stars < 1 || stars > 5) return OpResult::fail(400, "INVALID_RATING", "Rating must be between 1 and 5");
    Ride* r = rides_.find(rideId);
    trace.add("HashTable", "find", "O(1) avg", rideTag(rideId) + (r ? " found" : " not found"));
    if (!r) return OpResult::fail(404, "RIDE_NOT_FOUND", "No ride with id " + std::to_string(rideId));
    if (r->status != RideStatus::COMPLETED)
        return OpResult::fail(409, "NOT_COMPLETED", "Only completed rides can be rated");
    if (r->rating != 0) return OpResult::fail(409, "ALREADY_RATED", "This ride was already rated");
    Driver* d = drivers_.find(r->driverId);
    if (!d) return OpResult::fail(404, "DRIVER_NOT_FOUND", "Driver no longer exists");
    r->rating = stars;
    applyRating(*d, stars, trace);
    return OpResult::success(rideId, "Thanks for rating");
}

void Dispatcher::handleEvent(const Event& e, DSTrace& trace) {
    Ride* r = rides_.find(e.rideId);
    if (!r || r->eventVersion != e.version) {
        trace.add("MinHeap", "popMin", "O(log n)", std::string("Stale ") + toString(e.type) + " for " + rideTag(e.rideId) + " ignored");
        return;
    }
    trace.add("MinHeap", "popMin", "O(log n)", std::string(toString(e.type)) + " for " + rideTag(r->id));
    if (e.type == EventType::DRIVER_ARRIVED_PICKUP && r->status == RideStatus::ASSIGNED) onArrivedPickup(*r, trace);
    else if (e.type == EventType::RIDE_COMPLETED && r->status == RideStatus::IN_PROGRESS) onRideCompleted(*r, trace);
    else if (e.type == EventType::DRIVER_FREE && r->status == RideStatus::COMPLETED) onDriverFree(*r, trace);
}

void Dispatcher::onArrivedPickup(Ride& r, DSTrace& trace) {
    Driver* d = drivers_.find(r.driverId);
    if (!d) return;
    d->areaId = r.pickup;
    d->motion.clear();

    PathResult trip = Dijkstra::shortestPath(map_.graph(), r.pickup, r.drop, &trace);
    if (!trip.found) {
        forceCancel(r, "Destination unreachable due to road block", trace);
        processQueue(trace);
        return;
    }
    addPending(r.pickup, -1);   // no longer waiting at the pickup
    r.status = RideStatus::IN_PROGRESS;
    r.pickupTime = clock_.now;
    r.tripNodes = trip.nodes;
    d->status = DriverStatus::ON_TRIP;
    startMotion(*d, trip, clock_.now);
    schedule(EventType::RIDE_COMPLETED, r, d->motion.endTime(), trace);
}

void Dispatcher::onRideCompleted(Ride& r, DSTrace& trace) {
    Driver* d = drivers_.find(r.driverId);
    if (!d) return;
    d->areaId = r.drop;
    d->motion.clear();
    d->earnings += r.fare;
    d->tripsCompleted += 1;
    r.completeTime = clock_.now;
    finishRide(r, RideStatus::COMPLETED, trace);
    if (r.autoGenerated) {   // simulated riders rate automatically (3..5)
        r.rating = 3 + (int)(rng_() % 3);
        applyRating(*d, r.rating, trace);
    }
    schedule(EventType::DRIVER_FREE, r, clock_.now + kDropHandoverMin, trace);
}

void Dispatcher::onDriverFree(Ride& r, DSTrace& trace) {
    Driver* d = drivers_.find(r.driverId);
    if (!d || d->rideId != r.id) return;
    releaseDriver(*d, trace);
    processQueue(trace);
}

// ----------------------------------------------------------------- traffic

bool Dispatcher::pathUsesRoad(const std::vector<int>& nodes, int roadId) const {
    for (size_t i = 0; i + 1 < nodes.size(); ++i)
        if (map_.graph().findRoad(nodes[i], nodes[i + 1]) == roadId) return true;
    return false;
}

void Dispatcher::reroute(Ride& r, Driver& d, DSTrace& trace) {
    double now = clock_.now;
    if (!d.motion.active() || now >= d.motion.endTime()) return;
    int i = d.motion.legAt(now);
    int startNode = d.motion.nodes[i + 1];   // finish the current road first
    double startTime = d.motion.legEnd[i];
    bool toPickup = r.status == RideStatus::ASSIGNED;
    int target = toPickup ? r.pickup : r.drop;

    if (!BFS::isReachable(map_.graph(), r.pickup, r.drop, &trace) && toPickup) {
        forceCancel(r, "Destination unreachable due to road block", trace);
        return;
    }
    if (!BFS::isReachable(map_.graph(), startNode, target, &trace)) {
        if (!toPickup) {
            forceCancel(r, "Destination unreachable due to road block", trace);
            return;
        }
        // this driver is cut off from the pickup: release and wait again
        r.eventVersion++;
        r.status = RideStatus::WAITING;
        r.driverId = -1;
        r.etaPickup = -1;
        releaseDriver(d, trace);
        queue_.enqueue(PendingRequest{r.id, r.pickup});
        trace.add("LinkedQueue", "enqueue", "O(1)", rideTag(r.id) + " back to waiting (driver cut off)");
        return;
    }

    PathResult rest = Dijkstra::shortestPath(map_.graph(), startNode, target, &trace);
    Motion m;
    m.nodes.push_back(d.motion.nodes[i]);
    m.nodes.push_back(startNode);
    m.roads.push_back(d.motion.roads[i]);
    m.legStart.push_back(d.motion.legStart[i]);
    m.legEnd.push_back(startTime);
    double t = startTime;
    for (size_t k = 0; k < rest.roads.size(); ++k) {
        m.nodes.push_back(rest.nodes[k + 1]);
        m.roads.push_back(rest.roads[k]);
        m.legStart.push_back(t);
        t += legMinutes(rest.roads[k]);
        m.legEnd.push_back(t);
    }
    d.motion = m;
    r.eventVersion++;
    if (toPickup) {
        r.etaPickup = t;
        schedule(EventType::DRIVER_ARRIVED_PICKUP, r, t, trace);
    } else {
        r.tripNodes = m.nodes;
        schedule(EventType::RIDE_COMPLETED, r, t, trace);
    }
    trace.add("Dijkstra", "reroute", "O((V+E) log V)",
              rideTag(r.id) + " re-routed from " + map_.areaName(startNode) + ", new ETA t=" + DSTrace::num(t, 1));
}

void Dispatcher::onRoadChanged(int roadId, DSTrace& trace) {
    // collect first: rerouting may erase ids from activeRides_
    std::vector<int> ids;
    activeRides_.forEach([&ids](int id, int) { ids.push_back(id); });
    double now = clock_.now;
    for (int id : ids) {
        Ride* r = rides_.find(id);
        if (!r) continue;
        // planned trip (pickup -> drop) of rides not yet picked up
        if ((r->status == RideStatus::ASSIGNED || r->status == RideStatus::WAITING) && pathUsesRoad(r->tripNodes, roadId)) {
            PathResult trip = Dijkstra::shortestPath(map_.graph(), r->pickup, r->drop, &trace);
            if (!trip.found) {
                bool freesDriver = r->status == RideStatus::ASSIGNED;
                forceCancel(*r, "Destination unreachable due to road block", trace);
                if (freesDriver) processQueue(trace);
                continue;
            }
            r->tripNodes = trip.nodes;
        }
        if (r->status != RideStatus::ASSIGNED && r->status != RideStatus::IN_PROGRESS) continue;
        Driver* d = drivers_.find(r->driverId);
        if (!d || !d->motion.active() || now >= d->motion.endTime()) continue;
        int current = d->motion.legAt(now);
        bool usesRoad = false;
        for (int k = current + 1; k < (int)d->motion.roads.size(); ++k)
            if (d->motion.roads[k] == roadId) usesRoad = true;
        if (usesRoad) reroute(*r, *d, trace);
    }
    processQueue(trace);   // waiting rides may now be reachable (or not)
}

void Dispatcher::recomputeSurge(DSTrace* trace) {
    surge_.recompute(
        map_.graph(), [this](int a) { return pendingInArea(a); }, [this](int a) { return freeDriversInArea(a); },
        trace);
}

// --------------------------------------------------------------- analytics

namespace {
struct FewerRequests {
    bool operator()(const AreaStat& a, const AreaStat& b) const {
        if (a.requests != b.requests) return a.requests < b.requests;
        return a.area > b.area;
    }
};
}  // namespace

Analytics Dispatcher::analytics() const {
    Analytics a;
    double waitSum = 0, fareSum = 0;
    int waitCount = 0;
    rides_.forEach([&](int, const Ride& r) {
        a.totalRides++;
        a.byType[(int)r.type]++;
        if (r.status == RideStatus::COMPLETED) {
            a.completed++;
            fareSum += r.fare;
            int hour = (int)(r.completeTime / 60.0);
            while ((int)a.ridesPerHour.size() <= hour) a.ridesPerHour.push_back(0);
            a.ridesPerHour[hour]++;
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
    });
    a.revenue = fareSum;
    a.avgFare = a.completed ? fareSum / a.completed : 0.0;
    a.avgWaitMin = waitCount ? waitSum / waitCount : 0.0;

    // top 5 busiest areas: keep a size-5 min-heap, evict the smallest
    MinHeap<AreaStat, FewerRequests> top;
    requestsPerArea_.forEach([&top](int area, const int& count) {
        top.push(AreaStat{area, count});
        AreaStat dropped{0, 0};
        if (top.size() > 5) top.popMin(dropped);
    });
    AreaStat s{0, 0};
    std::vector<AreaStat> ascending;
    while (top.popMin(s)) ascending.push_back(s);
    for (int i = (int)ascending.size() - 1; i >= 0; --i) a.busiest.push_back(ascending[i]);

    drivers_.forEach([&a](int id, const Driver& d) {
        if (a.topEarnerId == -1 || d.earnings > a.topEarnings) {
            a.topEarnerId = id;
            a.topEarnings = d.earnings;
        }
    });
    return a;
}
