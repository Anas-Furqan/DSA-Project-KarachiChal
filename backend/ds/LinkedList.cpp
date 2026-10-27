#include "ds/LinkedList.h"
#include <cstddef>

using namespace std;

LinkedList::LinkedList() {
    head = NULL;
    count = 0;
}

LinkedList::~LinkedList() {
    clear();
}

void LinkedList::pushFront(int value) {
    ListNode* node = new ListNode;
    node->value = value;
    node->next = head;
    head = node;
    count++;
}

bool LinkedList::popFront(int& out) {
    if (head == NULL) return false;
    ListNode* node = head;
    out = node->value;
    head = node->next;
    delete node;
    count--;
    return true;
}

bool LinkedList::remove(int value) {
    ListNode* prev = NULL;
    ListNode* cur = head;
    while (cur != NULL) {
        if (cur->value == value) {
            if (prev == NULL) {
                head = cur->next;
            } else {
                prev->next = cur->next;
            }
            delete cur;
            count--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

bool LinkedList::contains(int value) const {
    for (ListNode* cur = head; cur != NULL; cur = cur->next) {
        if (cur->value == value) return true;
    }
    return false;
}

int LinkedList::size() const {
    return count;
}

bool LinkedList::isEmpty() const {
    return count == 0;
}

ListNode* LinkedList::getHead() const {
    return head;
}

void LinkedList::clear() {
    while (head != NULL) {
        ListNode* next = head->next;
        delete head;
        head = next;
    }
    count = 0;
}
