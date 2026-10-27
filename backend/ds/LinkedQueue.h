#pragma once

using namespace std;

struct QueueNode {
    int value;
    QueueNode* next;
};

class LinkedQueue {
public:
    LinkedQueue();
    ~LinkedQueue();

    void enqueue(int value);
    bool dequeue(int& out);
    bool peekFront(int& out) const;
    bool removeById(int value);
    bool contains(int value) const;
    int size() const;
    bool isEmpty() const;
    QueueNode* getFront() const;
    void clear();

private:
    QueueNode* front;
    QueueNode* rear;
    int count;
};
