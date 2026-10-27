#include "ds/AVLTree.h"
#include <cstdio>

using namespace std;

AVLTree::AVLTree() {
    root = NULL;
    count = 0;
    numLastRotations = 0;
    countLL = 0;
    countRR = 0;
    countLR = 0;
    countRL = 0;
}

AVLTree::~AVLTree() {
    destroy(root);
}

void AVLTree::destroy(AVLNode* node) {
    if (node == NULL) return;
    destroy(node->left);
    destroy(node->right);
    delete node;
}

bool AVLTree::comesBefore(int rating1, int id1, int rating2, int id2) const {
    if (rating1 != rating2) return rating1 > rating2;
    return id1 < id2;
}

int AVLTree::heightOf(AVLNode* node) const {
    if (node == NULL) return 0;
    return node->height;
}

int AVLTree::balanceOf(AVLNode* node) const {
    if (node == NULL) return 0;
    return heightOf(node->left) - heightOf(node->right);
}

void AVLTree::updateHeight(AVLNode* node) {
    int hl = heightOf(node->left);
    int hr = heightOf(node->right);
    node->height = 1 + (hl > hr ? hl : hr);
}

string AVLTree::describe(AVLNode* node) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "#%d (%.2f)", node->driverId, node->rating / 100.0);
    return buf;
}

void AVLTree::recordRotation(string text) {
    if (numLastRotations < MAX_LAST_ROTATIONS) {
        lastRotations[numLastRotations] = text;
        numLastRotations++;
    }
}

AVLNode* AVLTree::rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* t2 = x->right;
    x->right = y;
    y->left = t2;
    updateHeight(y);
    updateHeight(x);
    return x;
}

AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* t2 = y->left;
    y->left = x;
    x->right = t2;
    updateHeight(x);
    updateHeight(y);
    return y;
}

AVLNode* AVLTree::rebalance(AVLNode* node) {
    updateHeight(node);
    int balance = balanceOf(node);

    if (balance > 1) {
        if (balanceOf(node->left) >= 0) {
            countLL++;
            recordRotation("LL: right rotate at " + describe(node));
            return rotateRight(node);
        }
        countLR++;
        recordRotation("LR: left rotate at " + describe(node->left) + ", right rotate at " + describe(node));
        node->left = rotateLeft(node->left);
        return rotateRight(node);
    }
    if (balance < -1) {
        if (balanceOf(node->right) <= 0) {
            countRR++;
            recordRotation("RR: left rotate at " + describe(node));
            return rotateLeft(node);
        }
        countRL++;
        recordRotation("RL: right rotate at " + describe(node->right) + ", left rotate at " + describe(node));
        node->right = rotateRight(node->right);
        return rotateLeft(node);
    }
    return node;
}

AVLNode* AVLTree::insertAt(AVLNode* node, int rating, int driverId, bool& inserted) {
    if (node == NULL) {
        AVLNode* fresh = new AVLNode;
        fresh->rating = rating;
        fresh->driverId = driverId;
        fresh->height = 1;
        fresh->left = NULL;
        fresh->right = NULL;
        inserted = true;
        return fresh;
    }
    if (rating == node->rating && driverId == node->driverId) {
        inserted = false;
        return node;
    }
    if (comesBefore(rating, driverId, node->rating, node->driverId)) {
        node->left = insertAt(node->left, rating, driverId, inserted);
    } else {
        node->right = insertAt(node->right, rating, driverId, inserted);
    }
    if (!inserted) return node;
    return rebalance(node);
}

bool AVLTree::insert(int rating, int driverId) {
    bool inserted = false;
    root = insertAt(root, rating, driverId, inserted);
    if (inserted) count++;
    return inserted;
}

AVLNode* AVLTree::removeAt(AVLNode* node, int rating, int driverId, bool& removed) {
    if (node == NULL) {
        removed = false;
        return NULL;
    }
    if (rating == node->rating && driverId == node->driverId) {
        removed = true;
        if (node->left == NULL || node->right == NULL) {
            AVLNode* child = node->left != NULL ? node->left : node->right;
            delete node;
            return child;
        }
        // two children: copy the in-order successor, then delete it from the right side
        AVLNode* next = node->right;
        while (next->left != NULL) next = next->left;
        node->rating = next->rating;
        node->driverId = next->driverId;
        bool dummy = false;
        node->right = removeAt(node->right, next->rating, next->driverId, dummy);
    } else if (comesBefore(rating, driverId, node->rating, node->driverId)) {
        node->left = removeAt(node->left, rating, driverId, removed);
    } else {
        node->right = removeAt(node->right, rating, driverId, removed);
    }
    if (!removed) return node;
    return rebalance(node);
}

bool AVLTree::remove(int rating, int driverId) {
    bool removed = false;
    root = removeAt(root, rating, driverId, removed);
    if (removed) count--;
    return removed;
}

bool AVLTree::contains(int rating, int driverId) const {
    AVLNode* cur = root;
    while (cur != NULL) {
        if (cur->rating == rating && cur->driverId == driverId) return true;
        if (comesBefore(rating, driverId, cur->rating, cur->driverId)) {
            cur = cur->left;
        } else {
            cur = cur->right;
        }
    }
    return false;
}

void AVLTree::collectTop(AVLNode* node, int ratings[], int driverIds[], int& found, int n) const {
    if (node == NULL || found >= n) return;
    collectTop(node->left, ratings, driverIds, found, n);
    if (found < n) {
        ratings[found] = node->rating;
        driverIds[found] = node->driverId;
        found++;
    }
    collectTop(node->right, ratings, driverIds, found, n);
}

int AVLTree::topN(int ratings[], int driverIds[], int n) const {
    int found = 0;
    collectTop(root, ratings, driverIds, found, n);
    return found;
}

int AVLTree::size() const {
    return count;
}

int AVLTree::height() const {
    return heightOf(root);
}

void AVLTree::clear() {
    destroy(root);
    root = NULL;
    count = 0;
    numLastRotations = 0;
}

void AVLTree::clearLastRotations() {
    numLastRotations = 0;
}

int AVLTree::lastRotationCount() const {
    return numLastRotations;
}

string AVLTree::lastRotation(int i) const {
    if (i < 0 || i >= numLastRotations) return "";
    return lastRotations[i];
}

int AVLTree::rotationCount(string kind) const {
    if (kind == "LL") return countLL;
    if (kind == "RR") return countRR;
    if (kind == "LR") return countLR;
    if (kind == "RL") return countRL;
    return 0;
}

// pre-order, so out[0] is the root
int AVLTree::exportNode(AVLNode* node, AVLViewNode out[], int& count, int maxNodes) const {
    if (node == NULL || count >= maxNodes) return -1;
    int index = count;
    out[index].rating = node->rating;
    out[index].driverId = node->driverId;
    out[index].height = node->height;
    out[index].balance = balanceOf(node);
    count++;
    out[index].left = exportNode(node->left, out, count, maxNodes);
    out[index].right = exportNode(node->right, out, count, maxNodes);
    return index;
}

int AVLTree::view(AVLViewNode out[], int maxNodes) const {
    int count = 0;
    exportNode(root, out, count, maxNodes);
    return count;
}

bool AVLTree::validate(AVLNode* node, int& h) const {
    if (node == NULL) {
        h = 0;
        return true;
    }
    int hl = 0;
    int hr = 0;
    if (!validate(node->left, hl)) return false;
    if (!validate(node->right, hr)) return false;
    h = 1 + (hl > hr ? hl : hr);
    int balance = hl - hr;
    return h == node->height && balance >= -1 && balance <= 1;
}

bool AVLTree::inOrderCheck(AVLNode* node, int& lastRating, int& lastId, bool& first) const {
    if (node == NULL) return true;
    if (!inOrderCheck(node->left, lastRating, lastId, first)) return false;
    if (!first && !comesBefore(lastRating, lastId, node->rating, node->driverId)) return false;
    first = false;
    lastRating = node->rating;
    lastId = node->driverId;
    return inOrderCheck(node->right, lastRating, lastId, first);
}

bool AVLTree::isValidAVL() const {
    int h = 0;
    if (!validate(root, h)) return false;
    int lastRating = 0;
    int lastId = 0;
    bool first = true;
    return inOrderCheck(root, lastRating, lastId, first);
}
