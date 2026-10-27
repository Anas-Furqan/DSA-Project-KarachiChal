#include "ds/HashTable.h"

using namespace std;

HashTable::HashTable(int initialBuckets) {
    if (initialBuckets < 1) initialBuckets = 1;
    numBuckets = initialBuckets;
    buckets = new HashNode*[numBuckets];
    for (int i = 0; i < numBuckets; i++) buckets[i] = NULL;
    count = 0;
    rehashes = 0;
}

HashTable::~HashTable() {
    clear();
    delete[] buckets;
}

// FNV-1a hash
unsigned int HashTable::hashString(const string& key) const {
    unsigned int h = 2166136261u;
    for (int i = 0; i < (int)key.size(); i++) {
        h = h ^ (unsigned char)key[i];
        h = h * 16777619u;
    }
    return h;
}

int HashTable::indexOf(const string& key) const {
    return (int)(hashString(key) % (unsigned int)numBuckets);
}

HashNode* HashTable::findNode(const string& key) const {
    HashNode* cur = buckets[indexOf(key)];
    while (cur != NULL) {
        if (cur->key == key) return cur;
        cur = cur->next;
    }
    return NULL;
}

void HashTable::rehash() {
    int oldCount = numBuckets;
    HashNode** old = buckets;
    numBuckets = numBuckets * 2;
    buckets = new HashNode*[numBuckets];
    for (int i = 0; i < numBuckets; i++) buckets[i] = NULL;

    for (int i = 0; i < oldCount; i++) {
        HashNode* cur = old[i];
        while (cur != NULL) {
            HashNode* next = cur->next;
            int b = indexOf(cur->key);
            cur->next = buckets[b];
            buckets[b] = cur;
            cur = next;
        }
    }
    delete[] old;
    rehashes++;
}

bool HashTable::insert(string key, int value) {
    HashNode* existing = findNode(key);
    if (existing != NULL) {
        existing->value = value;
        return false;
    }
    if ((double)(count + 1) / numBuckets > 0.75) rehash();
    int b = indexOf(key);
    HashNode* node = new HashNode;
    node->key = key;
    node->value = value;
    node->next = buckets[b];
    buckets[b] = node;
    count++;
    return true;
}

bool HashTable::insert(int key, int value) {
    return insert(to_string(key), value);
}

bool HashTable::find(string key, int& value) const {
    HashNode* node = findNode(key);
    if (node == NULL) return false;
    value = node->value;
    return true;
}

bool HashTable::find(int key, int& value) const {
    return find(to_string(key), value);
}

bool HashTable::contains(string key) const {
    return findNode(key) != NULL;
}

bool HashTable::contains(int key) const {
    return contains(to_string(key));
}

bool HashTable::erase(string key) {
    int b = indexOf(key);
    HashNode* prev = NULL;
    HashNode* cur = buckets[b];
    while (cur != NULL) {
        if (cur->key == key) {
            if (prev == NULL) {
                buckets[b] = cur->next;
            } else {
                prev->next = cur->next;
            }
            delete cur;
            count--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

bool HashTable::erase(int key) {
    return erase(to_string(key));
}

int HashTable::get(string key) const {
    int value = 0;
    find(key, value);
    return value;
}

int HashTable::get(int key) const {
    return get(to_string(key));
}

void HashTable::addTo(string key, int delta) {
    HashNode* node = findNode(key);
    if (node == NULL) {
        insert(key, delta);
    } else {
        node->value += delta;
    }
}

void HashTable::addTo(int key, int delta) {
    addTo(to_string(key), delta);
}

int HashTable::size() const {
    return count;
}

bool HashTable::isEmpty() const {
    return count == 0;
}

int HashTable::bucketCount() const {
    return numBuckets;
}

double HashTable::loadFactor() const {
    return (double)count / numBuckets;
}

int HashTable::rehashCount() const {
    return rehashes;
}

int HashTable::longestChain() const {
    int best = 0;
    for (int i = 0; i < numBuckets; i++) {
        int length = 0;
        for (HashNode* cur = buckets[i]; cur != NULL; cur = cur->next) length++;
        if (length > best) best = length;
    }
    return best;
}

HashNode* HashTable::bucketHead(int b) const {
    if (b < 0 || b >= numBuckets) return NULL;
    return buckets[b];
}

void HashTable::clear() {
    for (int i = 0; i < numBuckets; i++) {
        HashNode* cur = buckets[i];
        while (cur != NULL) {
            HashNode* next = cur->next;
            delete cur;
            cur = next;
        }
        buckets[i] = NULL;
    }
    count = 0;
}
