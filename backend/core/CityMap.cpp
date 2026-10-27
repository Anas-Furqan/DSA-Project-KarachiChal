#include "core/CityMap.h"
#include <fstream>
#include <sstream>

#include "json.hpp"

using namespace std;
using json = nlohmann::json;

const Graph& CityMap::graph() const {
    return roadGraph;
}

Graph& CityMap::graph() {
    return roadGraph;
}

const Trie& CityMap::trie() const {
    return nameTrie;
}

int CityMap::areaCount() const {
    return roadGraph.nodeCount();
}

bool CityMap::isValidArea(int id) const {
    return roadGraph.isValidNode(id);
}

string CityMap::areaName(int id) const {
    if (!isValidArea(id)) return "?";
    return roadGraph.getNode(id).name;
}

// lowercase, trim and collapse spaces
string CityMap::normalize(string s) {
    string out = "";
    bool pendingSpace = false;
    for (int i = 0; i < (int)s.size(); i++) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) out += ' ';
        pendingSpace = false;
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        out += c;
    }
    return out;
}

void CityMap::clear() {
    roadGraph.clear();
    nameTrie.clear();
    nameToId.clear();
}

int CityMap::findAreaByName(string name) const {
    int id = -1;
    if (!nameToId.find(normalize(name), id)) return -1;
    return id;
}

// also index every later word, so "iqbal" finds "Gulshan-e-Iqbal"
void CityMap::indexName(string name, int id) {
    string norm = normalize(name);
    nameTrie.insert(norm, id, name);
    for (int i = 1; i < (int)norm.size(); i++) {
        if (norm[i - 1] != ' ' && norm[i - 1] != '-') continue;
        int end = i;
        while (end < (int)norm.size() && norm[end] != ' ' && norm[end] != '-') end++;
        if (end - i >= 2) nameTrie.insert(norm.substr(i), id, name);
    }
}

bool CityMap::loadFromFile(string path, string& error) {
    ifstream in(path.c_str());
    if (!in) {
        clear();
        error = "Map file not found: " + path;
        return false;
    }
    stringstream buffer;
    buffer << in.rdbuf();
    return loadFromJsonText(buffer.str(), error);
}

bool CityMap::loadFromJsonText(string text, string& error) {
    clear();
    json doc;
    try {
        doc = json::parse(text);
    } catch (exception& e) {
        error = string("Map file is not valid JSON: ") + e.what();
        return false;
    }
    if (!doc.is_object() || !doc.contains("areas") || !doc["areas"].is_array() || !doc.contains("roads") ||
        !doc["roads"].is_array()) {
        error = "Map file must contain 'areas' and 'roads' arrays";
        return false;
    }
    json areas = doc["areas"];
    json roads = doc["roads"];
    if (areas.size() == 0) {
        error = "Map file has no areas";
        return false;
    }
    if ((int)areas.size() > MAX_NODES || (int)roads.size() > MAX_ROADS) {
        error = "Map file is too large (max " + to_string(MAX_NODES) + " areas, " + to_string(MAX_ROADS) + " roads)";
        return false;
    }

    HashTable fileIdToNode;
    for (int i = 0; i < (int)areas.size(); i++) {
        json a = areas[i];
        if (!a.is_object() || !a.contains("id") || !a["id"].is_number_integer() || !a.contains("name") ||
            !a["name"].is_string() || !a.contains("lat") || !a["lat"].is_number() || !a.contains("lng") ||
            !a["lng"].is_number()) {
            clear();
            error = "Every area needs integer 'id', string 'name', numeric 'lat' and 'lng'";
            return false;
        }
        int fileId = a["id"].get<int>();
        string name = a["name"].get<string>();
        string norm = normalize(name);
        if (norm.empty()) {
            clear();
            error = "Area " + to_string(fileId) + " has an empty name";
            return false;
        }
        if (fileIdToNode.contains(fileId)) {
            clear();
            error = "Duplicate area id: " + to_string(fileId);
            return false;
        }
        if (nameToId.contains(norm)) {
            clear();
            error = "Duplicate area name: " + name;
            return false;
        }
        int node = roadGraph.addNode(name, a["lat"].get<double>(), a["lng"].get<double>());
        fileIdToNode.insert(fileId, node);
        nameToId.insert(norm, node);
        indexName(name, node);
    }

    for (int i = 0; i < (int)roads.size(); i++) {
        json r = roads[i];
        if (!r.is_object() || !r.contains("from") || !r["from"].is_number_integer() || !r.contains("to") ||
            !r["to"].is_number_integer() || !r.contains("km") || !r["km"].is_number()) {
            clear();
            error = "Every road needs integer 'from', 'to' and numeric 'km'";
            return false;
        }
        int fromId = r["from"].get<int>();
        int toId = r["to"].get<int>();
        double km = r["km"].get<double>();
        string label = to_string(fromId) + " -> " + to_string(toId);
        int u = -1;
        int v = -1;
        if (!fileIdToNode.find(fromId, u) || !fileIdToNode.find(toId, v)) {
            clear();
            error = "Road " + label + " points to an unknown area";
            return false;
        }
        if (!(km > 0)) {
            clear();
            error = "Road " + label + " has a non-positive distance";
            return false;
        }
        if (u == v) {
            clear();
            error = "Road " + label + " connects an area to itself";
            return false;
        }
        if (roadGraph.addEdge(u, v, km) == -1) {
            clear();
            error = "Duplicate road " + label;
            return false;
        }
    }
    error = "";
    return true;
}

int CityMap::search(string query, TrieMatch results[], int limit, DSTrace* trace) const {
    string q = normalize(query);
    if (q.empty() || (int)q.size() > MAX_QUERY_LENGTH) return 0;
    int found = nameTrie.startsWith(q, results, limit);
    if (trace) {
        trace->add("Trie", "startsWith", "O(L + k)",
                   "Prefix \"" + q + "\" matched " + to_string(nameTrie.matchedLength(q)) + "/" + to_string(q.size()) +
                       " chars, " + to_string(found) + " suggestions");
    }
    return found;
}
