#pragma once
#include <string>

using namespace std;

const int MAX_TRACE = 200;

struct TraceNode {
    string ds;
    string op;
    string complexity;
    string detail;
    TraceNode* next;
};

// Log of data structure operations, sent to the frontend inspector.
class DSTrace {
public:
    DSTrace();
    ~DSTrace();

    void add(string ds, string op, string complexity, string detail);
    TraceNode* getHead() const;
    int size() const;
    bool isTruncated() const;
    void clear();

    static string num(double value, int decimals);

private:
    TraceNode* head;
    TraceNode* tail;
    int count;
    bool truncated;
};
