#pragma once
#include <string>
#include <vector>

/*
 * HashTable<K, V> — hash map with SEPARATE CHAINING (own linked chains).
 *
 * Used for: drivers by id, rides by id, riders by name, area name -> id,
 * free drivers per (area, type), pending requests per area (surge).
 *
 * Why a hash table? These are pure "look up by key" questions. A sorted
 * array or BST would cost O(log n); hashing gives O(1) average.
 *
 * Why chaining (not open addressing)? Deletes are trivial (unlink a node),
 * the table never "fills up", and performance degrades gracefully under
 * collisions. When load factor (size / buckets) passes 0.75 we double the
 * bucket array and re-link every node (rehash), keeping chains short.
 *
 * Nodes are heap-allocated and only re-linked on rehash, so a V* returned by
 * find() stays valid until that key is erased.
 */

// Own hash functions — no std::hash.
struct KeyHasher {
    // O(1): integer bit mixer (spreads sequential ids across buckets).
    unsigned int operator()(int key) const {
        unsigned int x = (unsigned int)key;
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    }
    // O(L): FNV-1a over the characters.
    unsigned int operator()(const std::string& key) const {
        unsigned int h = 2166136261U;
        for (unsigned char c : key) {
            h ^= c;
            h *= 16777619U;
        }
        return h;
    }
};

template <typename K, typename V, typename Hasher = KeyHasher>
class HashTable {
public:
    static constexpr double kMaxLoad = 0.75;

    explicit HashTable(int initialBuckets = 8) {
        if (initialBuckets < 1) initialBuckets = 1;
        buckets_.assign(initialBuckets, nullptr);
    }
    ~HashTable() { clear(); }

    // O(n) deep copy
    HashTable(const HashTable& other) : buckets_(other.buckets_.size(), nullptr) { copyFrom(other); }

    // O(n + m)
    HashTable& operator=(const HashTable& other) {
        if (this != &other) {
            clear();
            buckets_.assign(other.buckets_.size(), nullptr);
            copyFrom(other);
        }
        return *this;
    }

    // O(1) average, O(n) worst. Inserts or overwrites.
    // Returns true if the key was new. May trigger an O(n) rehash.
    bool insert(const K& key, const V& value) {
        Node* existing = findNode(key);
        if (existing) {
            existing->value = value;
            return false;
        }
        if ((double)(size_ + 1) / (double)buckets_.size() > kMaxLoad) rehash((int)buckets_.size() * 2);
        int b = indexOf(key);
        buckets_[b] = new Node{key, value, buckets_[b]};
        ++size_;
        return true;
    }

    // O(1) average, O(n) worst. nullptr if absent.
    V* find(const K& key) {
        Node* n = findNode(key);
        return n ? &n->value : nullptr;
    }
    const V* find(const K& key) const {
        Node* n = findNode(key);
        return n ? &n->value : nullptr;
    }

    // O(1) average
    bool contains(const K& key) const { return findNode(key) != nullptr; }

    // O(1) average: returns existing value or inserts `initial` first.
    V& getOrInsert(const K& key, const V& initial) {
        Node* n = findNode(key);
        if (n) return n->value;
        insert(key, initial);
        return findNode(key)->value;
    }

    // O(1) average, O(n) worst. Returns false if the key was absent.
    bool erase(const K& key) {
        int b = indexOf(key);
        Node* prev = nullptr;
        for (Node* cur = buckets_[b]; cur; prev = cur, cur = cur->next) {
            if (!(cur->key == key)) continue;
            if (prev) prev->next = cur->next;
            else buckets_[b] = cur->next;
            delete cur;
            --size_;
            return true;
        }
        return false;
    }

    // O(1)
    int size() const { return size_; }
    // O(1)
    bool isEmpty() const { return size_ == 0; }
    // O(1)
    int bucketCount() const { return (int)buckets_.size(); }
    // O(1)
    double loadFactor() const { return (double)size_ / (double)buckets_.size(); }
    // O(1)
    int rehashCount() const { return rehashCount_; }

    // O(n + buckets): visit(key, value&) for every entry.
    template <typename Visit>
    void forEach(Visit visit) {
        for (Node* head : buckets_)
            for (Node* cur = head; cur; cur = cur->next) visit(cur->key, cur->value);
    }
    template <typename Visit>
    void forEach(Visit visit) const {
        for (Node* head : buckets_)
            for (Node* cur = head; cur; cur = cur->next) visit(cur->key, (const V&)cur->value);
    }

    // O(chain length): visit(key) for each node in one bucket (inspector view).
    template <typename Visit>
    void forEachInBucket(int b, Visit visit) const {
        if (b < 0 || b >= bucketCount()) return;
        for (Node* cur = buckets_[b]; cur; cur = cur->next) visit(cur->key);
    }

    // O(n + buckets): length of the longest chain (collision quality check).
    int longestChain() const {
        int best = 0;
        for (Node* head : buckets_) {
            int len = 0;
            for (Node* cur = head; cur; cur = cur->next) ++len;
            if (len > best) best = len;
        }
        return best;
    }

    // O(n + buckets)
    void clear() {
        for (Node*& head : buckets_) {
            while (head) {
                Node* n = head;
                head = head->next;
                delete n;
            }
        }
        size_ = 0;
    }

private:
    struct Node {
        K key;
        V value;
        Node* next;
    };

    std::vector<Node*> buckets_;
    int size_ = 0;
    int rehashCount_ = 0;
    Hasher hasher_;

    int indexOf(const K& key) const { return (int)(hasher_(key) % (unsigned int)buckets_.size()); }

    Node* findNode(const K& key) const {
        for (Node* cur = buckets_[indexOf(key)]; cur; cur = cur->next)
            if (cur->key == key) return cur;
        return nullptr;
    }

    // O(n): re-link every existing node into a bigger bucket array.
    void rehash(int newBucketCount) {
        std::vector<Node*> old = buckets_;
        buckets_.assign(newBucketCount, nullptr);
        for (Node* head : old) {
            Node* cur = head;
            while (cur) {
                Node* next = cur->next;
                int b = indexOf(cur->key);
                cur->next = buckets_[b];
                buckets_[b] = cur;
                cur = next;
            }
        }
        ++rehashCount_;
    }

    void copyFrom(const HashTable& other) {
        for (Node* head : other.buckets_)
            for (Node* cur = head; cur; cur = cur->next) insert(cur->key, cur->value);
    }
};
