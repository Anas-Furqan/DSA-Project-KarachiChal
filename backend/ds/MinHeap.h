#pragma once

using namespace std;

// Dijkstra uses key = distance, value = node.
// The event scheduler uses key = time, value = ride id, type = event type.
struct HeapItem {
    double key;
    long long order;
    int value;
    int type;
    int version;
};

HeapItem makeHeapItem(double key, long long order, int value);

class MinHeap {
public:
    MinHeap();
    ~MinHeap();

    void push(HeapItem item);
    bool popMin(HeapItem& out);
    bool peek(HeapItem& out) const;
    bool isEmpty() const;
    int size() const;
    HeapItem itemAt(int i) const;
    void clear();
    bool isValidHeap() const;

private:
    HeapItem* data;
    int count;
    int capacity;

    bool isLess(const HeapItem& a, const HeapItem& b) const;
    void swapItems(int i, int j);
    void grow();
    void siftUp(int i);
    void siftDown(int i);
};
