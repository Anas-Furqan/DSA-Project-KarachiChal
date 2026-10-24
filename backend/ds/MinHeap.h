#pragma once
#include <vector>

/*
 * MinHeap<T, Less> — array-based binary min-heap (priority queue).
 *
 * Used for (a) the Dijkstra frontier (smallest tentative distance first) and
 * (b) the simulation Event Scheduler (earliest event time first).
 *
 * Why a heap instead of a sorted array or linked list?
 *   Sorted array: popMin O(1) but insert O(n) (shifting).
 *   Unsorted list: insert O(1) but popMin O(n) (scan).
 *   Binary heap: both insert and popMin O(log n), peek O(1) — the best
 *   balance when pushes and pops are interleaved, as in Dijkstra.
 *
 * Layout: node i has children 2i+1 and 2i+2, parent (i-1)/2.
 * `Less(a, b)` returns true when a should come out before b.
 */

template <typename T>
struct DefaultLess {
    bool operator()(const T& a, const T& b) const { return a < b; }
};

template <typename T, typename Less = DefaultLess<T>>
class MinHeap {
public:
    // O(log n): append at the end, then sift up.
    void push(const T& value) {
        data_.push_back(value);
        siftUp((int)data_.size() - 1);
    }

    // O(log n): move last to root, then sift down.
    // Returns false (and leaves `out` untouched) if the heap is empty.
    bool popMin(T& out) {
        if (data_.empty()) return false;
        out = data_[0];
        data_[0] = data_.back();
        data_.pop_back();
        if (!data_.empty()) siftDown(0);
        return true;
    }

    // O(1). Returns false if empty.
    bool peek(T& out) const {
        if (data_.empty()) return false;
        out = data_[0];
        return true;
    }

    // O(1)
    bool isEmpty() const { return data_.empty(); }
    // O(1)
    int size() const { return (int)data_.size(); }
    // O(n)
    void clear() { data_.clear(); }

    // O(1) to return, O(n) to copy. Raw array order (index 0 = root) so the
    // frontend can draw the tree exactly as it is stored.
    const std::vector<T>& snapshot() const { return data_; }

    // O(n): debug/test helper — checks the heap property everywhere.
    bool isValidHeap() const {
        for (int i = 1; i < (int)data_.size(); ++i) {
            if (less_(data_[i], data_[parent(i)])) return false;
        }
        return true;
    }

private:
    std::vector<T> data_;
    Less less_;

    static int parent(int i) { return (i - 1) / 2; }
    static int left(int i) { return 2 * i + 1; }
    static int right(int i) { return 2 * i + 2; }

    void swapAt(int i, int j) {
        T tmp = data_[i];
        data_[i] = data_[j];
        data_[j] = tmp;
    }

    // O(log n): bubble element up while it is smaller than its parent.
    void siftUp(int i) {
        while (i > 0 && less_(data_[i], data_[parent(i)])) {
            swapAt(i, parent(i));
            i = parent(i);
        }
    }

    // O(log n): push element down toward its smaller child.
    void siftDown(int i) {
        int n = (int)data_.size();
        while (true) {
            int smallest = i;
            if (left(i) < n && less_(data_[left(i)], data_[smallest])) smallest = left(i);
            if (right(i) < n && less_(data_[right(i)], data_[smallest])) smallest = right(i);
            if (smallest == i) return;
            swapAt(i, smallest);
            i = smallest;
        }
    }
};
