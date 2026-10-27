#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "api/Server.h"
#include "core/Simulator.h"

using namespace std;

bool fileExists(string path) {
    ifstream f(path.c_str());
    return f.good();
}

// works from the project root and from build/
string findPath(string relative) {
    string prefixes[3] = {"", "../", "../../"};
    for (int i = 0; i < 3; i++) {
        if (fileExists(prefixes[i] + relative)) return prefixes[i] + relative;
    }
    return "";
}

int main(int argc, char** argv) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            cerr << "Invalid port: " << argv[1] << endl;
            return 1;
        }
    }

    string mapPath = findPath("data/karachi_map.json");
    string indexPath = findPath("frontend/index.html");
    if (mapPath.empty()) {
        cerr << "Cannot find data/karachi_map.json, run from the project root." << endl;
        return 1;
    }
    string staticDir = "";
    if (!indexPath.empty()) staticDir = indexPath.substr(0, indexPath.size() - string("/index.html").size());
    if (staticDir.empty()) cerr << "Warning: frontend/ not found, serving API only." << endl;

    Simulator* sim = new Simulator();
    string error;
    if (!sim->init(mapPath, error)) {
        cerr << "Map error: " << error << endl;
        delete sim;
        return 1;
    }
    cout << "KarachiChal: " << sim->map().areaCount() << " areas, " << sim->map().graph().roadCount() << " roads, "
         << sim->dispatcher().driverCount() << " drivers" << endl;
    cout << "Open http://localhost:" << port << endl;

    ApiServer server(*sim, staticDir);
    bool ok = server.run("0.0.0.0", port);
    delete sim;
    if (!ok) {
        cerr << "Could not start server on port " << port << " (already in use?)" << endl;
        return 1;
    }
    return 0;
}
