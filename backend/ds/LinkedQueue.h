#pragma once

/*
 * LinkedQueue<T> — FIFO queue built on a singly linked list with head + tail.
 *
 * Used for: pending ride requests (first come, first served) and as the
 * frontier of BFS.
 *
 * Why a linked list (not an array)?
 *   enqueue at tail and dequeue at head are both O(1) with no shifting and
 *   no fixed capacity. A cancelled request can be unlinked from the middle
 *   (removeById) without moving any other element.
 *
 * Copying makes a deep copy, so a queue can safely be stored as a value.
 */

template <typename T>
class LinkedQueue {
public:
    LinkedQueue() {}
    ~LinkedQueue() { clear(); }

    // O(n) deep copy
    LinkedQueue(const LinkedQueue& other) { copyFrom(other); }

    // O(n + m)
    LinkedQueue& operator=(const LinkedQueue& other) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    // O(1): link new node after tail.
    void enqueue(const T& value) {
        Node* n = new Node{value, nullptr};
        if (tail_) tail_->next = n;
        else head_ = n;
        tail_ = n;
        ++size_;
    }

    // O(1): unlink head. Returns false on an empty queue (safe, no UB).
    bool dequeue(T& out) {
        if (!head_) return false;
        Node* n = head_;
        out = n->data;
        head_ = n->next;
        if (!head_) tail_ = nullptr;
        delete n;
        --size_;
        return true;
    }

    // O(1). Returns false if empty.
    bool peekFront(T& out) const {
        if (!head_) return false;
        out = head_->data;
        return true;
    }

    // O(n): unlink the first element for which match(element) is true.
    // Copies it to *removed when given. Returns false if nothing matched.
    template <typename Match>
    bool removeFirstWhere(Match match, T* removed = nullptr) {
        Node* prev = nullptr;
        for (Node* cur = head_; cur; prev = cur, cur = cur->next) {
            if (!match(cur->data)) continue;
            if (removed) *removed = cur->data;
            if (prev) prev->next = cur->next;
            else head_ = cur->next;
            if (cur == tail_) tail_ = prev;
            delete cur;
            --size_;
            return true;
        }
        return false;
    }

    // O(n): cancellation by id. Requires T to have an `id` field.
    bool removeById(int id) {
        return removeFirstWhere([id](const T& item) { return item.id == id; });
    }

    // O(n): visit front -> rear.
    template <typename Visit>
    void forEach(Visit visit) const {
        for (Node* cur = head_; cur; cur = cur->next) visit(cur->data);
    }

    // O(1)
    int size() const { return size_; }
    // O(1)
    bool isEmpty() const { return size_ == 0; }

    // O(n)
    void clear() {
        while (head_) {
            Node* n = head_;
            head_ = head_->next;
            delete n;
        }
        tail_ = nullptr;
        size_ = 0;
    }

private:
    struct Node {
        T data;
        Node* next;
    };
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    int size_ = 0;

    void copyFrom(const LinkedQueue& other) {
        for (Node* cur = other.head_; cur; cur = cur->next) enqueue(cur->data);
    }
};
