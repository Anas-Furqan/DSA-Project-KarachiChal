#pragma once
#include <string>

#include "core/DSTrace.h"
#include "ds/Graph.h"
#include "ds/HashTable.h"
#include "ds/Trie.h"

using namespace std;

const int MAX_QUERY_LENGTH = 64;

// Owns the road Graph, the Trie of area names and a name -> id HashTable.
class CityMap {
public:
    bool loadFromFile(string path, string& error);
    bool loadFromJsonText(string text, string& error);

    const Graph& graph() const;
    Graph& graph();
    const Trie& trie() const;

    int areaCount() const;
    bool isValidArea(int id) const;
    string areaName(int id) const;
    int findAreaByName(string name) const;
    int search(string query, TrieMatch results[], int limit, DSTrace* trace = NULL) const;

    static string normalize(string s);
    void clear();

private:
    Graph roadGraph;
    Trie nameTrie;
    HashTable nameToId;

    void indexName(string name, int id);
};
