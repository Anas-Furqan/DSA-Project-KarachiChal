#include "ds/Trie.h"

Trie::Node::Node() : isEnd(false), value(-1) {
    for (int i = 0; i < kAlphabet; ++i) child[i] = nullptr;
}

Trie::Trie() : root_(new Node()) {}

Trie::~Trie() { destroy(root_); }

// O(1): map a character to a child slot, case-insensitive. -1 = unsupported.
int Trie::indexOf(char c) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ' ') return 36;
    if (c == '-') return 37;
    if (c == '.') return 38;
    if (c == '\'') return 39;
    return -1;
}

char Trie::charAt(int index) {
    if (index < 26) return (char)('a' + index);
    if (index < 36) return (char)('0' + index - 26);
    const char extra[] = {' ', '-', '.', '\''};
    return extra[index - 36];
}

void Trie::destroy(Node* n) {
    if (!n) return;
    for (int i = 0; i < kAlphabet; ++i) destroy(n->child[i]);
    delete n;
}

void Trie::clear() {
    destroy(root_);
    root_ = new Node();
    wordCount_ = 0;
    nodeCount_ = 1;
}

bool Trie::insert(const std::string& word, int value, const std::string& displayName) {
    if (word.empty()) return false;
    for (char c : word)
        if (indexOf(c) == -1) return false;   // validate before touching the tree

    Node* cur = root_;
    for (char c : word) {
        int i = indexOf(c);
        if (!cur->child[i]) {
            cur->child[i] = new Node();
            ++nodeCount_;
        }
        cur = cur->child[i];
    }
    if (!cur->isEnd) ++wordCount_;
    cur->isEnd = true;
    cur->value = value;
    cur->word = displayName.empty() ? word : displayName;
    return true;
}

const Trie::Node* Trie::walk(const std::string& prefix) const {
    const Node* cur = root_;
    for (char c : prefix) {
        int i = indexOf(c);
        if (i == -1 || !cur->child[i]) return nullptr;
        cur = cur->child[i];
    }
    return cur;
}

bool Trie::contains(const std::string& word) const {
    if (word.empty()) return false;
    const Node* n = walk(word);
    return n && n->isEnd;
}

// Pre-order DFS in alphabet order; stops once `limit` distinct values found.
void Trie::collect(const Node* n, int limit, std::vector<TrieMatch>& out) const {
    if ((int)out.size() >= limit) return;
    if (n->isEnd) {
        bool duplicate = false;
        for (const TrieMatch& m : out)
            if (m.value == n->value) duplicate = true;
        if (!duplicate) out.push_back(TrieMatch{n->word, n->value});
    }
    for (int i = 0; i < kAlphabet && (int)out.size() < limit; ++i)
        if (n->child[i]) collect(n->child[i], limit, out);
}

std::vector<TrieMatch> Trie::startsWith(const std::string& prefix, int limit) const {
    std::vector<TrieMatch> out;
    if (prefix.empty() || limit <= 0) return out;
    const Node* start = walk(prefix);
    if (start) collect(start, limit, out);
    return out;
}

int Trie::matchedLength(const std::string& prefix) const {
    const Node* cur = root_;
    int matched = 0;
    for (char c : prefix) {
        int i = indexOf(c);
        if (i == -1 || !cur->child[i]) break;
        cur = cur->child[i];
        ++matched;
    }
    return matched;
}

void Trie::addSubtree(const Node* n, int parentIdx, int depth, int maxNodes,
                      std::vector<TrieViewNode>& out) const {
    for (int i = 0; i < kAlphabet; ++i) {
        if (!n->child[i]) continue;
        if ((int)out.size() >= maxNodes) return;
        const Node* c = n->child[i];
        out.push_back(TrieViewNode{charAt(i), parentIdx, depth, c->isEnd, false, c->isEnd ? c->word : ""});
        addSubtree(c, (int)out.size() - 1, depth + 1, maxNodes, out);
    }
}

std::vector<TrieViewNode> Trie::branchView(const std::string& prefix, int maxNodes) const {
    std::vector<TrieViewNode> out;
    out.push_back(TrieViewNode{'\0', -1, 0, false, true, ""});
    int matched = matchedLength(prefix);

    // 1) the matched path, plus every sibling one level off the path
    const Node* cur = root_;
    int curIdx = 0;
    for (int d = 0; d < matched; ++d) {
        int pathChild = indexOf(prefix[d]);
        int nextIdx = -1;
        for (int i = 0; i < kAlphabet; ++i) {
            const Node* c = cur->child[i];
            if (!c) continue;
            bool onPath = (i == pathChild);
            if (!onPath && (int)out.size() >= maxNodes) continue;
            out.push_back(TrieViewNode{charAt(i), curIdx, d + 1, c->isEnd, onPath, c->isEnd ? c->word : ""});
            if (onPath) nextIdx = (int)out.size() - 1;
        }
        cur = cur->child[pathChild];
        curIdx = nextIdx;
    }
    // 2) the subtree under the prefix node (all completions), capped
    if (matched > 0 && matched == (int)prefix.size()) addSubtree(cur, curIdx, matched + 1, maxNodes, out);
    return out;
}
