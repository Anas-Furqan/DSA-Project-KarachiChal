#include "ds/LinkedQueue.h"
#include <cstddef>

using namespace std;

LinkedQueue::LinkedQueue() {
    front = NULL;
    rear = NULL;
    count = 0;
}

LinkedQueue::~LinkedQueue() {
    clear();
}

void LinkedQueue::enqueue(int value) {
    QueueNode* node = new QueueNode;
    node->value = value;
    node->next = NULL;
    if (rear == NULL) {
        front = node;
    } else {
        rear->next = node;
    }
    rear = node;
    count++;
}

bool LinkedQueue::dequeue(int& out) {
    if (front == NULL) return false;
    QueueNode* node = front;
    out = node->value;
    front = node->next;
    if (front == NULL) rear = NULL;
    delete node;
    count--;
    return true;
}

bool LinkedQueue::peekFront(int& out) const {
    if (front == NULL) return false;
    out = front->value;
    return true;
}

bool LinkedQueue::removeById(int value) {
    QueueNode* prev = NULL;
    QueueNode* cur = front;
    while (cur != NULL) {
        if (cur->value == value) {
            if (prev == NULL) {
                front = cur->next;
            } else {
                prev->next = cur->next;
            }
            if (cur == rear) rear = prev;
            delete cur;
            count--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

bool LinkedQueue::contains(int value) const {
    for (QueueNode* cur = front; cur != NULL; cur = cur->next) {
        if (cur->value == value) return true;
    }
    return false;
}

int LinkedQueue::size() const {
    return count;
}

bool LinkedQueue::isEmpty() const {
    return count == 0;
}

QueueNode* LinkedQueue::getFront() const {
    return front;
}

void LinkedQueue::clear() {
    while (front != NULL) {
        QueueNode* next = front->next;
        delete front;
        front = next;
    }
    rear = NULL;
    count = 0;
}
