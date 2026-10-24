#pragma once
#include <cstdio>
#include <string>
#include <vector>

/*
 * DSTrace — an append-only log of data-structure operations.
 *
 * Every API request gets a fresh DSTrace. Algorithms record what they did
 * ("MinHeap popMin O(log n) Popped Saddar (2.1 km)") and the server returns
 * the log to the frontend inspector so the viva audience can see which DS ran.
 *
 * Why a plain growable array? Entries are only appended and then read in
 * order once — no search or delete — so an array with O(1) amortized append
 * is the simplest fit. It is capped at kMaxEntries so one big request cannot
 * flood the response; `truncated()` tells the client that entries were cut.
 */

struct TraceEntry {
    std::string ds;          // e.g. "MinHeap"
    std::string op;          // e.g. "popMin"
    std::string complexity;  // e.g. "O(log n)"
    std::string detail;      // human readable
};

class DSTrace {
public:
    static const int kMaxEntries = 200;

    // O(1) amortized. Drops the entry (and sets truncated) once full.
    void add(const std::string& ds, const std::string& op,
             const std::string& complexity, const std::string& detail) {
        if ((int)entries_.size() >= kMaxEntries) {
            truncated_ = true;
            return;
        }
        entries_.push_back(TraceEntry{ds, op, complexity, detail});
    }

    // O(k) for k appended entries.
    void append(const DSTrace& other) {
        for (const TraceEntry& e : other.entries_) add(e.ds, e.op, e.complexity, e.detail);
        if (other.truncated_) truncated_ = true;
    }

    // O(1)
    const std::vector<TraceEntry>& entries() const { return entries_; }
    // O(1)
    bool truncated() const { return truncated_; }
    // O(1)
    int size() const { return (int)entries_.size(); }
    // O(n)
    void clear() {
        entries_.clear();
        truncated_ = false;
    }

    // O(1): format a number with fixed decimals, e.g. num(3.456, 1) -> "3.5".
    static std::string num(double value, int decimals = 1) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
        return buf;
    }

private:
    std::vector<TraceEntry> entries_;
    bool truncated_ = false;
};
