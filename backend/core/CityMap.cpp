#include "core/CityMap.h"

#include <fstream>
#include <sstream>

#include "json.hpp"

using nlohmann::json;

namespace {
const std::string kUnknown = "?";
const int kMaxQueryLength = 64;
}

std::string CityMap::normalize(const std::string& s) {
    std::string out;
    bool pendingSpace = false;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) out += ' ';
        pendingSpace = false;
        out += (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    return out;
}

void CityMap::clear() {
    graph_.clear();
    trie_.clear();
    nameToId_.clear();
}

const std::string& CityMap::areaName(int id) const {
    return isValidArea(id) ? graph_.node(id).name : kUnknown;
}

int CityMap::findAreaByName(const std::string& name) const {
    const int* id = nameToId_.find(normalize(name));
    return id ? *id : -1;
}

// Full name plus every later word as an alias: "Gulshan-e-Iqbal" is also
// found by "iqbal", "DHA Phase 6" by "phase 6".
void CityMap::indexName(const std::string& name, int id) {
    std::string norm = normalize(name);
    trie_.insert(norm, id, name);
    for (size_t i = 1; i < norm.size(); ++i) {
        bool boundary = (norm[i - 1] == ' ' || norm[i - 1] == '-');
        if (!boundary) continue;
        size_t end = norm.find_first_of(" -", i);
        size_t tokenLen = (end == std::string::npos ? norm.size() : end) - i;
        if (tokenLen >= 2) trie_.insert(norm.substr(i), id, name);
    }
}

bool CityMap::loadFromFile(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "Map file not found: " + path;
        clear();
        return false;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    return loadFromJsonText(buf.str(), error);
}

bool CityMap::loadFromJsonText(const std::string& text, std::string& error) {
    clear();
    json doc;
    try {
        doc = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("Map file is not valid JSON: ") + e.what();
        return false;
    }
    if (!doc.is_object() || !doc.contains("areas") || !doc["areas"].is_array() || !doc.contains("roads") ||
        !doc["roads"].is_array()) {
        error = "Map file must contain 'areas' and 'roads' arrays";
        return false;
    }
    if (doc["areas"].empty()) {
        error = "Map file has no areas";
        return false;
    }

    HashTable<int, int> fileIdToNode;   // ids in the file need not be 0..n-1
    for (const json& a : doc["areas"]) {
        if (!a.is_object() || !a.contains("id") || !a["id"].is_number_integer() || !a.contains("name") ||
            !a["name"].is_string() || !a.contains("lat") || !a["lat"].is_number() || !a.contains("lng") ||
            !a["lng"].is_number()) {
            error = "Every area needs integer 'id', string 'name', numeric 'lat' and 'lng'";
            clear();
            return false;
        }
        int fileId = a["id"].get<int>();
        std::string name = a["name"].get<std::string>();
        std::string norm = normalize(name);
        if (norm.empty()) {
            error = "Area " + std::to_string(fileId) + " has an empty name";
            clear();
            return false;
        }
        if (fileIdToNode.contains(fileId)) {
            error = "Duplicate area id: " + std::to_string(fileId);
            clear();
            return false;
        }
        if (nameToId_.contains(norm)) {
            error = "Duplicate area name: " + name;
            clear();
            return false;
        }
        int node = graph_.addNode(name, a["lat"].get<double>(), a["lng"].get<double>());
        fileIdToNode.insert(fileId, node);
        nameToId_.insert(norm, node);
        indexName(name, node);
    }

    for (const json& r : doc["roads"]) {
        if (!r.is_object() || !r.contains("from") || !r["from"].is_number_integer() || !r.contains("to") ||
            !r["to"].is_number_integer() || !r.contains("km") || !r["km"].is_number()) {
            error = "Every road needs integer 'from', 'to' and numeric 'km'";
            clear();
            return false;
        }
        int fromId = r["from"].get<int>(), toId = r["to"].get<int>();
        double km = r["km"].get<double>();
        const int* u = fileIdToNode.find(fromId);
        const int* v = fileIdToNode.find(toId);
        std::string label = std::to_string(fromId) + " -> " + std::to_string(toId);
        if (!u || !v) {
            error = "Road " + label + " points to an unknown area";
            clear();
            return false;
        }
        if (!(km > 0.0)) {
            error = "Road " + label + " has a non-positive distance";
            clear();
            return false;
        }
        if (*u == *v) {
            error = "Road " + label + " connects an area to itself";
            clear();
            return false;
        }
        if (graph_.addEdge(*u, *v, km) == -1) {
            error = "Duplicate road " + label;
            clear();
            return false;
        }
    }
    error.clear();
    return true;
}

std::vector<TrieMatch> CityMap::search(const std::string& query, int limit, DSTrace* trace) const {
    std::string q = normalize(query);
    if (q.empty() || (int)q.size() > kMaxQueryLength) return std::vector<TrieMatch>();
    std::vector<TrieMatch> found = trie_.startsWith(q, limit);
    if (trace) {
        trace->add("Trie", "startsWith", "O(L + k)",
                   "Prefix \"" + q + "\" matched " + std::to_string(trie_.matchedLength(q)) + "/" +
                       std::to_string(q.size()) + " chars, " + std::to_string(found.size()) + " suggestions");
    }
    return found;
}
