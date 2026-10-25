/*
 * KarachiChal — entry point.
 * Loads the map, seeds the simulation and serves API + frontend on :8080.
 *
 *   karachichal [port]
 *
 * Run it from the project root (or build/) so data/ and frontend/ are found.
 */
#include <fstream>
#include <iostream>
#include <string>

#include "api/Server.h"
#include "core/Simulator.h"

namespace {

bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return (bool)f;
}

// Look in ., .. and ../.. so the exe works from the root or from build/.
std::string findPath(const std::string& relative) {
    const char* prefixes[] = {"", "../", "../../"};
    for (const char* p : prefixes)
        if (fileExists(std::string(p) + relative)) return std::string(p) + relative;
    return "";
}

}  // namespace

int main(int argc, char** argv) {
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port: " << argv[1] << "\n";
            return 1;
        }
    }

    std::string mapPath = findPath("data/karachi_map.json");
    std::string indexPath = findPath("frontend/index.html");
    if (mapPath.empty()) {
        std::cerr << "Cannot find data/karachi_map.json — run from the project root.\n";
        return 1;
    }
    std::string staticDir = indexPath.empty() ? "" : indexPath.substr(0, indexPath.size() - std::string("/index.html").size());
    if (staticDir.empty()) std::cerr << "Warning: frontend/ not found, serving API only.\n";

    Simulator sim;
    std::string error;
    if (!sim.init(mapPath, error)) {
        std::cerr << "Map error: " << error << "\n";
        return 1;
    }
    std::cout << "KarachiChal: " << sim.map().areaCount() << " areas, " << sim.map().graph().roadCount()
              << " roads, " << sim.dispatcher().drivers().size() << " drivers\n";
    std::cout << "Open http://localhost:" << port << "\n";

    ApiServer server(sim, staticDir);
    if (!server.run("0.0.0.0", port)) {
        std::cerr << "Could not start server on port " << port << " (already in use?)\n";
        return 1;
    }
    return 0;
}
