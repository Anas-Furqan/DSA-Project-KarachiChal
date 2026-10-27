#include "ds/Trie.h"

using namespace std;

Trie::Trie() {
    words = 0;
    nodes = 0;
    root = newNode();
}

Trie::~Trie() {
    destroy(root);
}

TrieNode* Trie::newNode() {
    TrieNode* node = new TrieNode;
    for (int i = 0; i < TRIE_ALPHABET; i++) node->child[i] = NULL;
    node->isEnd = false;
    node->value = -1;
    nodes++;
    return node;
}

void Trie::destroy(TrieNode* node) {
    if (node == NULL) return;
    for (int i = 0; i < TRIE_ALPHABET; i++) destroy(node->child[i]);
    delete node;
}

// a-z, 0-9, space, '-', '.', '\''  (case-insensitive)
int Trie::indexOf(char c) const {
    if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    if (c == ' ') return 36;
    if (c == '-') return 37;
    if (c == '.') return 38;
    if (c == '\'') return 39;
    return -1;
}

char Trie::charAt(int index) const {
    if (index < 26) return 'a' + index;
    if (index < 36) return '0' + (index - 26);
    if (index == 36) return ' ';
    if (index == 37) return '-';
    if (index == 38) return '.';
    return '\'';
}

bool Trie::insert(string word, int value, string displayName) {
    if (word.empty()) return false;
    for (int i = 0; i < (int)word.size(); i++) {
        if (indexOf(word[i]) == -1) return false;
    }
    TrieNode* cur = root;
    for (int i = 0; i < (int)word.size(); i++) {
        int c = indexOf(word[i]);
        if (cur->child[c] == NULL) cur->child[c] = newNode();
        cur = cur->child[c];
    }
    if (!cur->isEnd) words++;
    cur->isEnd = true;
    cur->value = value;
    cur->word = displayName.empty() ? word : displayName;
    return true;
}

TrieNode* Trie::walk(string prefix) const {
    TrieNode* cur = root;
    for (int i = 0; i < (int)prefix.size(); i++) {
        int c = indexOf(prefix[i]);
        if (c == -1 || cur->child[c] == NULL) return NULL;
        cur = cur->child[c];
    }
    return cur;
}

bool Trie::contains(string word) const {
    if (word.empty()) return false;
    TrieNode* node = walk(word);
    return node != NULL && node->isEnd;
}

// DFS in alphabet order, skips words that point to an area already found
void Trie::collect(TrieNode* node, TrieMatch results[], int& found, int limit) const {
    if (found >= limit) return;
    if (node->isEnd) {
        bool duplicate = false;
        for (int i = 0; i < found; i++) {
            if (results[i].value == node->value) duplicate = true;
        }
        if (!duplicate) {
            results[found].word = node->word;
            results[found].value = node->value;
            found++;
        }
    }
    for (int i = 0; i < TRIE_ALPHABET && found < limit; i++) {
        if (node->child[i] != NULL) collect(node->child[i], results, found, limit);
    }
}

int Trie::startsWith(string prefix, TrieMatch results[], int limit) const {
    if (prefix.empty() || limit <= 0) return 0;
    TrieNode* start = walk(prefix);
    if (start == NULL) return 0;
    int found = 0;
    collect(start, results, found, limit);
    return found;
}

int Trie::matchedLength(string prefix) const {
    TrieNode* cur = root;
    int matched = 0;
    for (int i = 0; i < (int)prefix.size(); i++) {
        int c = indexOf(prefix[i]);
        if (c == -1 || cur->child[c] == NULL) break;
        cur = cur->child[c];
        matched++;
    }
    return matched;
}

void Trie::addSubtree(TrieNode* node, int parentIndex, int depth, TrieViewNode out[], int& count, int maxNodes) const {
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        TrieNode* c = node->child[i];
        if (c == NULL) continue;
        if (count >= maxNodes) return;
        out[count].ch = charAt(i);
        out[count].parent = parentIndex;
        out[count].depth = depth;
        out[count].isEnd = c->isEnd;
        out[count].onPath = false;
        out[count].word = c->isEnd ? c->word : "";
        count++;
        addSubtree(c, count - 1, depth + 1, out, count, maxNodes);
    }
}

// root, the typed path with its siblings, then the words below the prefix
int Trie::branchView(string prefix, TrieViewNode out[], int maxNodes) const {
    if (maxNodes <= 0) return 0;
    int count = 0;
    out[0].ch = '\0';
    out[0].parent = -1;
    out[0].depth = 0;
    out[0].isEnd = false;
    out[0].onPath = true;
    out[0].word = "";
    count = 1;

    int matched = matchedLength(prefix);
    TrieNode* cur = root;
    int curIndex = 0;
    for (int d = 0; d < matched; d++) {
        int pathChild = indexOf(prefix[d]);
        int nextIndex = -1;
        for (int i = 0; i < TRIE_ALPHABET; i++) {
            TrieNode* c = cur->child[i];
            if (c == NULL) continue;
            bool onPath = (i == pathChild);
            if (!onPath && count >= maxNodes - (matched - d)) continue;
            out[count].ch = charAt(i);
            out[count].parent = curIndex;
            out[count].depth = d + 1;
            out[count].isEnd = c->isEnd;
            out[count].onPath = onPath;
            out[count].word = c->isEnd ? c->word : "";
            if (onPath) nextIndex = count;
            count++;
        }
        cur = cur->child[pathChild];
        curIndex = nextIndex;
    }
    if (matched > 0 && matched == (int)prefix.size()) {
        addSubtree(cur, curIndex, matched + 1, out, count, maxNodes);
    }
    return count;
}

int Trie::wordCount() const {
    return words;
}

int Trie::nodeCount() const {
    return nodes;
}

void Trie::clear() {
    destroy(root);
    words = 0;
    nodes = 0;
    root = newNode();
}
