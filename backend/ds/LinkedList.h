#pragma once

/*
 * LinkedList<T> — singly linked list with a head pointer.
 *
 * Used for: each rider's ride history (newest first) and the list of free
 * driver ids parked in one area.
 *
 * Why a singly linked list? History only ever grows at the front and is read
 * front to back, so pushFront is O(1) with no resizing or shifting (an array
 * would shift every element to insert at index 0). No backward walk is
 * needed, so the extra pointer of a doubly linked list buys nothing.
 *
 * Copying makes a deep copy, so a list can safely live inside a HashTable.
 */

template <typename T>
class LinkedList {
public:
    LinkedList() {}
    ~LinkedList() { clear(); }

    // O(n) deep copy, order preserved
    LinkedList(const LinkedList& other) { copyFrom(other); }

    // O(n + m)
    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    // O(1)
    void pushFront(const T& value) {
        head_ = new Node{value, head_};
        ++size_;
    }

    // O(1). Returns false on an empty list.
    bool popFront(T& out) {
        if (!head_) return false;
        Node* n = head_;
        out = n->data;
        head_ = n->next;
        delete n;
        --size_;
        return true;
    }

    // O(1). Returns false on an empty list.
    bool front(T& out) const {
        if (!head_) return false;
        out = head_->data;
        return true;
    }

    // O(n): unlink the first node equal to value.
    bool remove(const T& value) {
        Node* prev = nullptr;
        for (Node* cur = head_; cur; prev = cur, cur = cur->next) {
            if (!(cur->data == value)) continue;
            if (prev) prev->next = cur->next;
            else head_ = cur->next;
            delete cur;
            --size_;
            return true;
        }
        return false;
    }

    // O(n)
    bool contains(const T& value) const {
        for (Node* cur = head_; cur; cur = cur->next)
            if (cur->data == value) return true;
        return false;
    }

    // O(n): visit head -> tail (newest -> oldest for history).
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
        size_ = 0;
    }

private:
    struct Node {
        T data;
        Node* next;
    };
    Node* head_ = nullptr;
    int size_ = 0;

    // keep order: append at a moving tail while copying
    void copyFrom(const LinkedList& other) {
        Node** tail = &head_;
        for (Node* cur = other.head_; cur; cur = cur->next) {
            *tail = new Node{cur->data, nullptr};
            tail = &(*tail)->next;
            ++size_;
        }
    }
};
