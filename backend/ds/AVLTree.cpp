#include "ds/AVLTree.h"

#include <cstdio>

AVLTree::~AVLTree() { destroy(root_); }

void AVLTree::destroy(Node* n) {
    if (!n) return;
    destroy(n->left);
    destroy(n->right);
    delete n;
}

void AVLTree::clear() {
    destroy(root_);
    root_ = nullptr;
    size_ = 0;
    lastRotations_.clear();
}

void AVLTree::updateHeight(Node* n) {
    int hl = heightOf(n->left), hr = heightOf(n->right);
    n->height = 1 + (hl > hr ? hl : hr);
}

std::string AVLTree::describe(const DriverKey& k) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "#%d (%.2f)", k.driverId, k.ratingCenti / 100.0);
    return buf;
}

int AVLTree::rotationCount(const std::string& kind) const {
    if (kind == "LL") return countLL_;
    if (kind == "RR") return countRR_;
    if (kind == "LR") return countLR_;
    if (kind == "RL") return countRL_;
    return 0;
}

/* O(1)
 *       y            x
 *      / \          / \
 *     x   C  -->   A   y
 *    / \              / \
 *   A   B            B   C
 */
AVLTree::Node* AVLTree::rotateRight(Node* y) {
    Node* x = y->left;
    y->left = x->right;
    x->right = y;
    updateHeight(y);
    updateHeight(x);
    return x;
}

// O(1): mirror of rotateRight.
AVLTree::Node* AVLTree::rotateLeft(Node* x) {
    Node* y = x->right;
    x->right = y->left;
    y->left = x;
    updateHeight(x);
    updateHeight(y);
    return y;
}

// O(1): restore |balance| <= 1 at n using one of the four rotation cases.
AVLTree::Node* AVLTree::rebalance(Node* n) {
    updateHeight(n);
    int bf = balanceOf(n);
    if (bf > 1) {
        if (balanceOf(n->left) >= 0) {
            lastRotations_.push_back("LL: right rotate at " + describe(n->key));
            ++countLL_;
        } else {
            lastRotations_.push_back("LR: left rotate at " + describe(n->left->key) +
                                     ", right rotate at " + describe(n->key));
            ++countLR_;
            n->left = rotateLeft(n->left);
        }
        return rotateRight(n);
    }
    if (bf < -1) {
        if (balanceOf(n->right) <= 0) {
            lastRotations_.push_back("RR: left rotate at " + describe(n->key));
            ++countRR_;
        } else {
            lastRotations_.push_back("RL: right rotate at " + describe(n->right->key) +
                                     ", left rotate at " + describe(n->key));
            ++countRL_;
            n->right = rotateRight(n->right);
        }
        return rotateLeft(n);
    }
    return n;
}

AVLTree::Node* AVLTree::insertAt(Node* n, const DriverKey& key, bool& inserted) {
    if (!n) {
        inserted = true;
        return new Node{key, 1, nullptr, nullptr};
    }
    if (key == n->key) return n;   // duplicate: unchanged
    if (key.before(n->key)) n->left = insertAt(n->left, key, inserted);
    else n->right = insertAt(n->right, key, inserted);
    return rebalance(n);
}

bool AVLTree::insert(const DriverKey& key) {
    lastRotations_.clear();
    bool inserted = false;
    root_ = insertAt(root_, key, inserted);
    if (inserted) ++size_;
    return inserted;
}

AVLTree::Node* AVLTree::removeAt(Node* n, const DriverKey& key, bool& removed) {
    if (!n) return nullptr;
    if (key == n->key) {
        removed = true;
        if (!n->left || !n->right) {
            Node* child = n->left ? n->left : n->right;
            delete n;
            return child;
        }
        // two children: copy in-order successor, then delete it from the right
        Node* succ = n->right;
        while (succ->left) succ = succ->left;
        n->key = succ->key;
        bool dummy = false;
        n->right = removeAt(n->right, succ->key, dummy);
    } else if (key.before(n->key)) {
        n->left = removeAt(n->left, key, removed);
    } else {
        n->right = removeAt(n->right, key, removed);
    }
    return rebalance(n);
}

bool AVLTree::remove(const DriverKey& key) {
    lastRotations_.clear();
    bool removed = false;
    root_ = removeAt(root_, key, removed);
    if (removed) --size_;
    return removed;
}

bool AVLTree::contains(const DriverKey& key) const {
    const Node* cur = root_;
    while (cur) {
        if (key == cur->key) return true;
        cur = key.before(cur->key) ? cur->left : cur->right;
    }
    return false;
}

// In-order walk that stops as soon as `limit` keys are collected.
void AVLTree::collectTop(const Node* n, int limit, std::vector<DriverKey>& out) const {
    if (!n || (int)out.size() >= limit) return;
    collectTop(n->left, limit, out);
    if ((int)out.size() >= limit) return;
    out.push_back(n->key);
    collectTop(n->right, limit, out);
}

std::vector<DriverKey> AVLTree::topN(int n) const {
    std::vector<DriverKey> out;
    if (n > 0) collectTop(root_, n, out);
    return out;
}

int AVLTree::exportNode(const Node* n, std::vector<AVLViewNode>& out) const {
    if (!n) return -1;
    int idx = (int)out.size();
    out.push_back(AVLViewNode{n->key, n->height, balanceOf(n), -1, -1});
    int l = exportNode(n->left, out);
    int r = exportNode(n->right, out);
    out[idx].left = l;
    out[idx].right = r;
    return idx;
}

std::vector<AVLViewNode> AVLTree::view() const {
    std::vector<AVLViewNode> out;
    exportNode(root_, out);
    return out;
}

bool AVLTree::validate(const Node* n, const DriverKey* lo, const DriverKey* hi, int& h) const {
    if (!n) {
        h = 0;
        return true;
    }
    if (lo && !lo->before(n->key)) return false;
    if (hi && !n->key.before(*hi)) return false;
    int hl = 0, hr = 0;
    if (!validate(n->left, lo, &n->key, hl)) return false;
    if (!validate(n->right, &n->key, hi, hr)) return false;
    h = 1 + (hl > hr ? hl : hr);
    int bf = hl - hr;
    return h == n->height && bf >= -1 && bf <= 1;
}

bool AVLTree::isValidAVL() const {
    int h = 0;
    return validate(root_, nullptr, nullptr, h);
}
