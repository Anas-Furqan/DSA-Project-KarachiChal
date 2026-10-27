#pragma once
#include <mutex>
#include <string>
#include <thread>

#include "core/Simulator.h"

using namespace std;

namespace httplib {
class Server;
struct Request;
struct Response;
}

// REST API + static frontend. One mutex guards the simulator for the
// background clock thread and every request handler.
class ApiServer {
public:
    ApiServer(Simulator& simRef, string staticFolder);
    ~ApiServer();

    bool run(string host, int port);
    void stop();

private:
    Simulator& sim;
    string staticDir;
    mutex simMutex;
    bool running;
    thread simThread;
    httplib::Server* http;

    bool isRunning();
    void registerRoutes();
    void simulationLoop();

    void getMap(const httplib::Request& req, httplib::Response& res);
    void getState(const httplib::Request& req, httplib::Response& res);
    void getRide(const httplib::Request& req, httplib::Response& res);
    void getSearch(const httplib::Request& req, httplib::Response& res);
    void postRideRequest(const httplib::Request& req, httplib::Response& res);
    void postRideCancel(const httplib::Request& req, httplib::Response& res);
    void postRideRate(const httplib::Request& req, httplib::Response& res);
    void getRiderHistory(const httplib::Request& req, httplib::Response& res);
    void postRoadTraffic(const httplib::Request& req, httplib::Response& res);
    void postRoadBlock(const httplib::Request& req, httplib::Response& res);
    void getLeaderboard(const httplib::Request& req, httplib::Response& res);
    void getAnalytics(const httplib::Request& req, httplib::Response& res);
    void postSimControl(const httplib::Request& req, httplib::Response& res);
    void getRoute(const httplib::Request& req, httplib::Response& res);
    void getDryRun(const httplib::Request& req, httplib::Response& res);
    void getSnapshot(const httplib::Request& req, httplib::Response& res);
};
