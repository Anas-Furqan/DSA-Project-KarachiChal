#pragma once
#include <string>

using namespace std;

const int TRIE_ALPHABET = 40;

struct TrieNode {
    TrieNode* child[TRIE_ALPHABET];
    bool isEnd;
    int value;
    string word;
};

struct TrieMatch {
    string word;
    int value;
};

struct TrieViewNode {
    char ch;
    int parent;
    int depth;
    bool isEnd;
    bool onPath;
    string word;
};

class Trie {
public:
    Trie();
    ~Trie();

    bool insert(string word, int value, string displayName);
    bool contains(string word) const;
    int startsWith(string prefix, TrieMatch results[], int limit) const;
    int matchedLength(string prefix) const;
    int branchView(string prefix, TrieViewNode out[], int maxNodes) const;
    int wordCount() const;
    int nodeCount() const;
    void clear();

private:
    TrieNode* root;
    int words;
    int nodes;

    TrieNode* newNode();
    void destroy(TrieNode* node);
    int indexOf(char c) const;
    char charAt(int index) const;
    TrieNode* walk(string prefix) const;
    void collect(TrieNode* node, TrieMatch results[], int& found, int limit) const;
    void addSubtree(TrieNode* node, int parentIndex, int depth, TrieViewNode out[], int& count, int maxNodes) const;
};
