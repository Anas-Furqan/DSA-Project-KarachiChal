#pragma once

using namespace std;

struct ListNode {
    int value;
    ListNode* next;
};

class LinkedList {
public:
    LinkedList();
    ~LinkedList();

    void pushFront(int value);
    bool popFront(int& out);
    bool remove(int value);
    bool contains(int value) const;
    int size() const;
    bool isEmpty() const;
    ListNode* getHead() const;
    void clear();

private:
    ListNode* head;
    int count;
};
