#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "core/Simulator.h"

namespace httplib {
class Server;
struct Request;
struct Response;
}

/*
 * Server — REST API (cpp-httplib) + static file host for the frontend.
 *
 * One mutex guards the Simulator: the background simulation thread and
 * every HTTP handler lock it, so no request ever sees a half-updated state.
 *
 * Every handler validates its input and answers with
 *   { "ok": true,  "data": ..., "trace": [...], "truncated": bool }   or
 *   { "ok": false, "error": "...", "code": "..." } + HTTP 400/404/409/500
 * Uncaught exceptions are turned into a 500 JSON error, never a crash.
 */
class ApiServer {
public:
    ApiServer(Simulator& sim, const std::string& staticDir);
    ~ApiServer();

    // Blocks until stop(). Returns false if the port could not be bound.
    bool run(const std::string& host, int port);
    void stop();

private:
    Simulator& sim_;
    std::string staticDir_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread simThread_;
    httplib::Server* http_;

    void registerRoutes();
    void simulationLoop();

    // route handlers
    void getMap(const httplib::Request&, httplib::Response&);
    void getState(const httplib::Request&, httplib::Response&);
    void getRide(const httplib::Request&, httplib::Response&);
    void getSearch(const httplib::Request&, httplib::Response&);
    void postRideRequest(const httplib::Request&, httplib::Response&);
    void postRideCancel(const httplib::Request&, httplib::Response&);
    void postRideRate(const httplib::Request&, httplib::Response&);
    void getRiderHistory(const httplib::Request&, httplib::Response&);
    void postRoadTraffic(const httplib::Request&, httplib::Response&);
    void postRoadBlock(const httplib::Request&, httplib::Response&);
    void getLeaderboard(const httplib::Request&, httplib::Response&);
    void getAnalytics(const httplib::Request&, httplib::Response&);
    void postSimControl(const httplib::Request&, httplib::Response&);
    void getRoute(const httplib::Request&, httplib::Response&);
    void getDryRun(const httplib::Request&, httplib::Response&);
    void getSnapshot(const httplib::Request&, httplib::Response&);
};
