#pragma once
#include <string>

using namespace std;

const int MAX_LAST_ROTATIONS = 10;

// Ordered by rating (high first), then driver id (low first), so keys are unique.
// Ratings are kept as hundredths: 4.73 -> 473.
struct AVLNode {
    int rating;
    int driverId;
    int height;
    AVLNode* left;
    AVLNode* right;
};

struct AVLViewNode {
    int rating;
    int driverId;
    int height;
    int balance;
    int left;
    int right;
};

class AVLTree {
public:
    AVLTree();
    ~AVLTree();

    bool insert(int rating, int driverId);
    bool remove(int rating, int driverId);
    bool contains(int rating, int driverId) const;
    int topN(int ratings[], int driverIds[], int n) const;
    int size() const;
    int height() const;
    void clear();

    void clearLastRotations();
    int lastRotationCount() const;
    string lastRotation(int i) const;
    int rotationCount(string kind) const;

    int view(AVLViewNode out[], int maxNodes) const;
    bool isValidAVL() const;

private:
    AVLNode* root;
    int count;
    string lastRotations[MAX_LAST_ROTATIONS];
    int numLastRotations;
    int countLL;
    int countRR;
    int countLR;
    int countRL;

    bool comesBefore(int rating1, int id1, int rating2, int id2) const;
    int heightOf(AVLNode* node) const;
    int balanceOf(AVLNode* node) const;
    void updateHeight(AVLNode* node);
    void destroy(AVLNode* node);
    void recordRotation(string text);
    string describe(AVLNode* node) const;

    AVLNode* rotateRight(AVLNode* y);
    AVLNode* rotateLeft(AVLNode* x);
    AVLNode* rebalance(AVLNode* node);
    AVLNode* insertAt(AVLNode* node, int rating, int driverId, bool& inserted);
    AVLNode* removeAt(AVLNode* node, int rating, int driverId, bool& removed);
    void collectTop(AVLNode* node, int ratings[], int driverIds[], int& found, int n) const;
    int exportNode(AVLNode* node, AVLViewNode out[], int& count, int maxNodes) const;
    bool validate(AVLNode* node, int& h) const;
    bool inOrderCheck(AVLNode* node, int& lastRating, int& lastId, bool& first) const;
};
