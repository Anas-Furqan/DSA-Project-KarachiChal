#include "ds/MinHeap.h"

using namespace std;

HeapItem makeHeapItem(double key, long long order, int value) {
    HeapItem item;
    item.key = key;
    item.order = order;
    item.value = value;
    item.type = 0;
    item.version = 0;
    return item;
}

MinHeap::MinHeap() {
    capacity = 16;
    count = 0;
    data = new HeapItem[capacity];
}

MinHeap::~MinHeap() {
    delete[] data;
}

bool MinHeap::isLess(const HeapItem& a, const HeapItem& b) const {
    if (a.key != b.key) return a.key < b.key;
    return a.order < b.order;
}

void MinHeap::swapItems(int i, int j) {
    HeapItem temp = data[i];
    data[i] = data[j];
    data[j] = temp;
}

void MinHeap::grow() {
    int newCapacity = capacity * 2;
    HeapItem* bigger = new HeapItem[newCapacity];
    for (int i = 0; i < count; i++) bigger[i] = data[i];
    delete[] data;
    data = bigger;
    capacity = newCapacity;
}

void MinHeap::siftUp(int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (!isLess(data[i], data[parent])) break;
        swapItems(i, parent);
        i = parent;
    }
}

void MinHeap::siftDown(int i) {
    while (true) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < count && isLess(data[left], data[smallest])) smallest = left;
        if (right < count && isLess(data[right], data[smallest])) smallest = right;
        if (smallest == i) break;
        swapItems(i, smallest);
        i = smallest;
    }
}

void MinHeap::push(HeapItem item) {
    if (count == capacity) grow();
    data[count] = item;
    count++;
    siftUp(count - 1);
}

bool MinHeap::popMin(HeapItem& out) {
    if (count == 0) return false;
    out = data[0];
    count--;
    if (count > 0) {
        data[0] = data[count];
        siftDown(0);
    }
    return true;
}

bool MinHeap::peek(HeapItem& out) const {
    if (count == 0) return false;
    out = data[0];
    return true;
}

bool MinHeap::isEmpty() const {
    return count == 0;
}

int MinHeap::size() const {
    return count;
}

HeapItem MinHeap::itemAt(int i) const {
    return data[i];
}

void MinHeap::clear() {
    count = 0;
}

bool MinHeap::isValidHeap() const {
    for (int i = 1; i < count; i++) {
        if (isLess(data[i], data[(i - 1) / 2])) return false;
    }
    return true;
}
