#pragma once
#include <string>
#include <vector>

#include "core/DSTrace.h"
#include "ds/Graph.h"
#include "ds/HashTable.h"
#include "ds/Trie.h"

/*
 * CityMap — loads data/karachi_map.json and owns the three structures that
 * describe the city:
 *   Graph     areas + roads (routing, BFS)
 *   Trie      area names + word aliases (autocomplete)
 *   HashTable normalized name -> area id (exact name lookup in O(1) avg)
 *
 * Loading is all-or-nothing: on any error (missing file, bad JSON, duplicate
 * name, road to an unknown area, non-positive distance, ...) the map stays
 * empty and a clear message is returned.
 */
class CityMap {
public:
    // O(V + E + total name length). False + error message on failure.
    bool loadFromFile(const std::string& path, std::string& error);

    // Same as loadFromFile but from JSON text (used by tests).
    bool loadFromJsonText(const std::string& text, std::string& error);

    // O(1)
    const Graph& graph() const { return graph_; }
    Graph& graph() { return graph_; }
    // O(1)
    const Trie& trie() const { return trie_; }

    // O(1)
    int areaCount() const { return graph_.nodeCount(); }
    // O(1)
    bool isValidArea(int id) const { return graph_.isValidNode(id); }
    // O(1). "?" for an invalid id.
    const std::string& areaName(int id) const;

    // O(L) average: exact case-insensitive lookup. -1 if unknown.
    int findAreaByName(const std::string& name) const;

    // O(L + k): Trie autocomplete, at most `limit` suggestions.
    std::vector<TrieMatch> search(const std::string& query, int limit = 5, DSTrace* trace = nullptr) const;

    // O(L): lowercase, trim, collapse inner whitespace.
    static std::string normalize(const std::string& s);

    // O(V + E)
    void clear();

private:
    Graph graph_;
    Trie trie_;
    HashTable<std::string, int> nameToId_;

    void indexName(const std::string& name, int id);
};
