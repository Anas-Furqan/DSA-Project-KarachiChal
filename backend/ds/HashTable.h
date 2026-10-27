#pragma once
#include <string>

using namespace std;

struct HashNode {
    string key;
    int value;
    HashNode* next;
};

// String keys, int values. Integer keys are stored as their text ("1001").
class HashTable {
public:
    HashTable(int initialBuckets = 8);
    ~HashTable();

    bool insert(string key, int value);
    bool insert(int key, int value);
    bool find(string key, int& value) const;
    bool find(int key, int& value) const;
    bool contains(string key) const;
    bool contains(int key) const;
    bool erase(string key);
    bool erase(int key);

    int get(string key) const;
    int get(int key) const;
    void addTo(string key, int delta);
    void addTo(int key, int delta);

    int size() const;
    bool isEmpty() const;
    int bucketCount() const;
    double loadFactor() const;
    int rehashCount() const;
    int longestChain() const;
    HashNode* bucketHead(int b) const;
    void clear();

private:
    HashNode** buckets;
    int numBuckets;
    int count;
    int rehashes;

    unsigned int hashString(const string& key) const;
    int indexOf(const string& key) const;
    HashNode* findNode(const string& key) const;
    void rehash();
};
