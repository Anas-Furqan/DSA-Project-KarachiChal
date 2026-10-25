#pragma once
#include <random>
#include <string>

#include "core/CityMap.h"
#include "core/DSTrace.h"
#include "core/Dispatcher.h"
#include "core/Models.h"
#include "ds/MinHeap.h"

/*
 * Simulator — the simulation clock and the Event Scheduler.
 *
 * Time unit: 1 real second = 1 sim minute at 1x (2x / 5x speed it up).
 * The Event Scheduler is a MinHeap<Event> ordered by event time, so every
 * tick only has to look at the root: pop while root.time <= now. That is
 * O(k log n) for k due events, instead of scanning every scheduled event.
 *
 * The Simulator is single-threaded on purpose; the HTTP server owns the
 * background thread and a mutex and calls advanceReal() under that lock.
 *
 * Owns: CityMap (graph + trie), the event heap, the clock, the Dispatcher.
 */
class Simulator {
public:
    static const int kDriverCount = 40;
    static const unsigned kSeed = 42;
    static constexpr double kRushSpawnEveryMin = 2.0;
    static const int kRushQueueCap = 25;

    Simulator();

    // O(V + E + D log D). Loads the map and seeds drivers.
    bool init(const std::string& mapPath, std::string& error);

    // O(V + E + D log D): reload map (clears traffic/blocks), reseed drivers.
    bool reset(std::string& error);

    // O(k log n + surge): scale real seconds by speed (no-op when paused).
    void advanceReal(double realSeconds);

    // O(k log n + surge): move the clock forward by sim minutes, handling
    // every due event in time order.
    void advanceSim(double minutes);

    // ---------------------------------------------------------- controls
    void setPaused(bool p) { paused_ = p; }
    bool paused() const { return paused_; }
    // O(1). Only 1, 2 or 5 are accepted.
    bool setSpeed(int s);
    int speed() const { return speed_; }
    void setRushHour(bool on);
    bool rushHour() const { return rushHour_; }

    // O(deg + A (V + E) log V): validated road edits followed by re-routing.
    OpResult setTraffic(int from, int to, double factor, DSTrace& trace);
    OpResult setBlocked(int from, int to, bool blocked, DSTrace& trace);

    // ----------------------------------------------------------- access
    double now() const { return clock_.now; }
    CityMap& map() { return map_; }
    const CityMap& map() const { return map_; }
    Dispatcher& dispatcher() { return dispatcher_; }
    const Dispatcher& dispatcher() const { return dispatcher_; }
    const MinHeap<Event>& events() const { return events_; }

    // O(k): returns and clears what background ticks did since the last call.
    DSTrace takeBackgroundTrace();

private:
    std::string mapPath_;
    CityMap map_;
    MinHeap<Event> events_;
    SimClock clock_;
    Dispatcher dispatcher_;   // declared after the members it references

    bool paused_ = false;
    int speed_ = 1;
    bool rushHour_ = false;
    double nextSpawnAt_ = 0.0;
    int rushRiderCounter_ = 0;
    std::mt19937 rng_;
    DSTrace background_;

    void spawnRushHourRequests();
};
