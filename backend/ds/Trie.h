#pragma once
#include <string>
#include <vector>

/*
 * Trie (prefix tree) — case-insensitive autocomplete for area names.
 *
 * Each edge is one character; a path from the root spells a prefix. All
 * names that share a prefix share the same nodes, so finding every name that
 * starts with "gul" means walking 3 edges and then collecting the subtree.
 *
 * Why a trie? Searching a list of names for a prefix costs O(n * L) per
 * keystroke. A trie costs O(L) to reach the prefix node, independent of how
 * many names are stored, plus O(k) to collect k suggestions.
 *
 * Alphabet: a-z, 0-9, space, '-', '.', '\'' (40 children per node).
 * Several stored words may carry the same value (aliases such as "iqbal" for
 * "Gulshan-e-Iqbal"); suggestions are de-duplicated by value.
 */

struct TrieMatch {
    std::string word;   // display name (original casing)
    int value;          // e.g. area id
};

struct TrieViewNode {
    char ch;            // '\0' for the root
    int parent;         // index into the view array, -1 for root
    int depth;
    bool isEnd;
    bool onPath;        // part of the typed prefix
    std::string word;   // stored word if isEnd
};

class Trie {
public:
    static const int kAlphabet = 40;

    Trie();
    ~Trie();
    Trie(const Trie&) = delete;
    Trie& operator=(const Trie&) = delete;

    // O(L). Returns false if word is empty or has an unsupported character.
    // Re-inserting a word overwrites its value.
    bool insert(const std::string& word, int value, const std::string& displayName = "");

    // O(L). Exact (case-insensitive) membership.
    bool contains(const std::string& word) const;

    // O(L + k) to reach the prefix node and collect up to `limit` results
    // (the DFS stops as soon as `limit` distinct values are found).
    // Empty prefix or unsupported character -> empty list.
    std::vector<TrieMatch> startsWith(const std::string& prefix, int limit = 5) const;

    // O(L): number of leading prefix characters that exist in the trie.
    int matchedLength(const std::string& prefix) const;

    // O(L * 40 + maxNodes): root -> prefix path, the children of each path
    // node, and the subtree below the prefix (capped) for the inspector.
    std::vector<TrieViewNode> branchView(const std::string& prefix, int maxNodes = 60) const;

    // O(1)
    int wordCount() const { return wordCount_; }
    // O(1)
    int nodeCount() const { return nodeCount_; }

    // O(total nodes)
    void clear();

private:
    struct Node {
        Node* child[kAlphabet];
        bool isEnd;
        int value;
        std::string word;
        Node();
    };

    Node* root_;
    int wordCount_ = 0;
    int nodeCount_ = 1;

    static int indexOf(char c);
    static char charAt(int index);
    static void destroy(Node* n);
    const Node* walk(const std::string& prefix) const;
    void collect(const Node* n, int limit, std::vector<TrieMatch>& out) const;
    void addSubtree(const Node* n, int parentIdx, int depth, int maxNodes,
                    std::vector<TrieViewNode>& out) const;
};
