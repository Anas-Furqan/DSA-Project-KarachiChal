#include "core/DSTrace.h"
#include <cstdio>

using namespace std;

DSTrace::DSTrace() {
    head = NULL;
    tail = NULL;
    count = 0;
    truncated = false;
}

DSTrace::~DSTrace() {
    clear();
}

void DSTrace::add(string ds, string op, string complexity, string detail) {
    if (count >= MAX_TRACE) {
        truncated = true;
        return;
    }
    TraceNode* node = new TraceNode;
    node->ds = ds;
    node->op = op;
    node->complexity = complexity;
    node->detail = detail;
    node->next = NULL;
    if (tail == NULL) {
        head = node;
    } else {
        tail->next = node;
    }
    tail = node;
    count++;
}

TraceNode* DSTrace::getHead() const {
    return head;
}

int DSTrace::size() const {
    return count;
}

bool DSTrace::isTruncated() const {
    return truncated;
}

void DSTrace::clear() {
    while (head != NULL) {
        TraceNode* next = head->next;
        delete head;
        head = next;
    }
    tail = NULL;
    count = 0;
    truncated = false;
}

string DSTrace::num(double value, int decimals) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return buf;
}
