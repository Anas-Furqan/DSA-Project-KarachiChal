#pragma once
#include <string>
#include <vector>

/*
 * AVLTree — self-balancing binary search tree for the driver leaderboard.
 *
 * Key = (rating desc, driverId asc). The id tie-break makes every key unique
 * even when two drivers share a rating. Ratings are stored as integer
 * hundredths (4.73 -> 473) so comparisons never suffer float error.
 *
 * Why AVL? The leaderboard changes after every completed ride (delete old
 * key + insert new key) and is read in sorted order. A sorted array needs
 * O(n) shifting per update; a plain BST can degrade to a linked list (O(n));
 * AVL keeps height <= 1.44 log n via rotations, so insert/delete/search are
 * O(log n) and an in-order walk gives the top-N directly.
 *
 * Balance factor = height(left) - height(right), kept in [-1, 1].
 * The four imbalance cases are fixed by LL, RR, LR and RL rotations.
 */

struct DriverKey {
    int ratingCenti;   // rating * 100, higher = better
    int driverId;

    bool operator==(const DriverKey& o) const {
        return ratingCenti == o.ratingCenti && driverId == o.driverId;
    }
    // "comes before" in leaderboard order: higher rating first, then lower id
    bool before(const DriverKey& o) const {
        if (ratingCenti != o.ratingCenti) return ratingCenti > o.ratingCenti;
        return driverId < o.driverId;
    }
};

struct AVLViewNode {
    DriverKey key;
    int height;
    int balance;
    int left;    // index in the view array, -1 if none
    int right;
};

class AVLTree {
public:
    AVLTree() {}
    ~AVLTree();
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    // O(log n). Returns false for a duplicate key (tree unchanged).
    bool insert(const DriverKey& key);

    // O(log n). Returns false if the key does not exist.
    bool remove(const DriverKey& key);

    // O(log n)
    bool contains(const DriverKey& key) const;

    // O(log n + N): first N keys of an in-order walk (best rating first).
    std::vector<DriverKey> topN(int n) const;

    // O(n)
    std::vector<DriverKey> inOrder() const { return topN(size_); }

    // O(1)
    int size() const { return size_; }
    // O(1)
    int height() const { return heightOf(root_); }
    // O(n)
    void clear();

    // O(1): rotations done by the most recent insert/remove, e.g.
    // "LL: right rotate at #12 (4.80)". Used by the inspector animation.
    const std::vector<std::string>& lastRotations() const { return lastRotations_; }
    // O(1): total rotations of one case since creation ("LL","RR","LR","RL").
    int rotationCount(const std::string& kind) const;

    // O(n): pre-order export with heights and balance factors.
    std::vector<AVLViewNode> view() const;

    // O(n): test helper — checks BST order, stored heights and |balance| <= 1.
    bool isValidAVL() const;

private:
    struct Node {
        DriverKey key;
        int height;
        Node* left;
        Node* right;
    };

    Node* root_ = nullptr;
    int size_ = 0;
    std::vector<std::string> lastRotations_;
    int countLL_ = 0, countRR_ = 0, countLR_ = 0, countRL_ = 0;

    static int heightOf(const Node* n) { return n ? n->height : 0; }
    static int balanceOf(const Node* n) { return n ? heightOf(n->left) - heightOf(n->right) : 0; }
    static void updateHeight(Node* n);
    static void destroy(Node* n);
    static std::string describe(const DriverKey& k);

    Node* rotateRight(Node* y);
    Node* rotateLeft(Node* x);
    Node* rebalance(Node* n);
    Node* insertAt(Node* n, const DriverKey& key, bool& inserted);
    Node* removeAt(Node* n, const DriverKey& key, bool& removed);
    void collectTop(const Node* n, int limit, std::vector<DriverKey>& out) const;
    int exportNode(const Node* n, std::vector<AVLViewNode>& out) const;
    bool validate(const Node* n, const DriverKey* lo, const DriverKey* hi, int& h) const;
};
