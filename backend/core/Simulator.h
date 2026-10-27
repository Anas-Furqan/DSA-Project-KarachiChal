#pragma once
#include <random>
#include <string>

#include "core/CityMap.h"
#include "core/DSTrace.h"
#include "core/Dispatcher.h"
#include "core/Models.h"
#include "ds/MinHeap.h"

using namespace std;

const int DRIVER_COUNT = 40;
const unsigned SIM_SEED = 42;
const double RUSH_SPAWN_EVERY_MIN = 2.0;
const int RUSH_QUEUE_CAP = 25;

// Sim clock + event scheduler (MinHeap by event time). 1 real second = 1 sim minute at 1x.
class Simulator {
public:
    Simulator();

    bool init(string mapFile, string& error);
    bool reset(string& error);
    void advanceReal(double realSeconds);
    void advanceSim(double minutes);

    void setPaused(bool p);
    bool isPaused() const;
    bool setSpeed(int s);
    int getSpeed() const;
    void setRushHour(bool on);
    bool isRushHour() const;

    OpResult setTraffic(int from, int to, double factor, DSTrace& trace);
    OpResult setBlocked(int from, int to, bool blocked, DSTrace& trace);

    double now() const;
    CityMap& map();
    const CityMap& map() const;
    Dispatcher& dispatcher();
    const Dispatcher& dispatcher() const;
    const MinHeap& events() const;
    DSTrace& backgroundTrace();

private:
    string mapPath;
    CityMap cityMap;
    MinHeap eventHeap;
    SimClock simClock;
    Dispatcher rides;
    bool paused;
    int speed;
    bool rushHour;
    double nextSpawnAt;
    int rushRiderCounter;
    mt19937 rng;
    DSTrace background;

    void spawnRushHourRequests();
};
