# Dry Runs

Every case below can be traced by hand, and the same case is a test in `tests/test_main.cpp` (the test name is listed with each case). Run `make test` or `build.bat test` to check them.

## 1. Dijkstra

All Dijkstra cases use the same 6-node graph:

```
A-B 4   A-C 2   B-C 1   B-D 5   C-D 8
C-E 10  D-E 2   D-F 6   E-F 3
```

Adjacency list (neighbour, km), in the order the roads were added:

| Node | Neighbours |
|---|---|
| A | B 4, C 2 |
| B | A 4, C 1, D 5 |
| C | A 2, B 1, D 8, E 10 |
| D | B 5, C 8, E 2, F 6 |
| E | C 10, D 2, F 3 |
| F | D 6, E 3 |

How to read the tables:

- **Pop** is the `(dist, node)` taken out of the min-heap in that step.
- The distance columns show the distance table after the step (∞ = not reached yet).
- **Heap after step** lists what is left in the heap, sorted so it is easy to read. The real heap is an array, so the order inside it is different.
- We never decrease a key inside the heap. When a shorter distance is found, a new entry is pushed and the old one stays. When an old entry is popped later it is **stale** (its node is already settled), so we skip it.
- Ties in the heap are broken by the smaller node id.

### Dijkstra case 1: A → F (normal graph)
Test: `dijkstra_case1_basic`, `dijkstra_trace_steps`

| Step | Pop | A | B | C | D | E | F | Heap after step | Note |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A (0) | 0 | 4 | 2 | ∞ | ∞ | ∞ | (2,C), (4,B) | relax B ∞→4, C ∞→2 |
| 2 | C (2) | 0 | 3 | 2 | 10 | 12 | ∞ | (3,B), (4,B), (10,D), (12,E) | relax B 4→3, D ∞→10, E ∞→12 |
| 3 | B (3) | 0 | 3 | 2 | 8 | 12 | ∞ | (4,B), (8,D), (10,D), (12,E) | relax D 10→8 |
| 4 | B (4) | 0 | 3 | 2 | 8 | 12 | ∞ | (8,D), (10,D), (12,E) | stale, skip |
| 5 | D (8) | 0 | 3 | 2 | 8 | 10 | 14 | (10,D), (10,E), (12,E), (14,F) | relax E 12→10, F ∞→14 |
| 6 | D (10) | 0 | 3 | 2 | 8 | 10 | 14 | (10,E), (12,E), (14,F) | stale, skip |
| 7 | E (10) | 0 | 3 | 2 | 8 | 10 | 13 | (12,E), (13,F), (14,F) | relax F 14→13 |
| 8 | E (12) | 0 | 3 | 2 | 8 | 10 | 13 | (13,F), (14,F) | stale, skip |
| 9 | F (13) | 0 | 3 | 2 | 8 | 10 | 13 | (14,F) | target popped, stop |

**Expected output:** `A → C → B → D → E → F`, distance **13 km**, 9 heap pops (3 of them stale).

### Dijkstra case 2: A → E with road D-E blocked
Test: `dijkstra_case2_blocked_edge`

Blocked roads are skipped while scanning neighbours, as if they were not in the graph.

| Step | Pop | A | B | C | D | E | F | Heap after step | Note |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A (0) | 0 | 4 | 2 | ∞ | ∞ | ∞ | (2,C), (4,B) | relax B ∞→4, C ∞→2 |
| 2 | C (2) | 0 | 3 | 2 | 10 | 12 | ∞ | (3,B), (4,B), (10,D), (12,E) | relax B 4→3, D ∞→10, E ∞→12 |
| 3 | B (3) | 0 | 3 | 2 | 8 | 12 | ∞ | (4,B), (8,D), (10,D), (12,E) | relax D 10→8 |
| 4 | B (4) | 0 | 3 | 2 | 8 | 12 | ∞ | (8,D), (10,D), (12,E) | stale, skip |
| 5 | D (8) | 0 | 3 | 2 | 8 | 12 | 14 | (10,D), (12,E), (14,F) | D-E blocked, skipped. relax F ∞→14 |
| 6 | D (10) | 0 | 3 | 2 | 8 | 12 | 14 | (12,E), (14,F) | stale, skip |
| 7 | E (12) | 0 | 3 | 2 | 8 | 12 | 14 | (14,F) | target popped, stop |

**Expected output:** `A → C → E`, distance **12 km**. Without the block the answer would be A C B D E = 10 km.

### Dijkstra case 3: A → G where G has no roads
Test: `dijkstra_case3_unreachable`

G is node 6 and has no roads, so it is never pushed. The algorithm runs until the heap is empty.

| Step | Pop | A | B | C | D | E | F | G | Note |
|---|---|---|---|---|---|---|---|---|---|
| 1–8 | same as case 1 | | | | | | | ∞ | |
| 9 | F (13) | 0 | 3 | 2 | 8 | 10 | 13 | ∞ | no neighbour improves |
| 10 | F (14) | 0 | 3 | 2 | 8 | 10 | 13 | ∞ | stale, skip. Heap is now empty |

**Expected output:** not found (`found = false`, empty path). In the app this gives the error `"Destination unreachable due to road block"`.

### Dijkstra case 4: A → D with traffic ×3 on B-C
Test: `dijkstra_case4_traffic`

Effective weight = base km × traffic factor, so B-C becomes 1 × 3 = 3.

| Step | Pop | A | B | C | D | E | F | Heap after step | Note |
|---|---|---|---|---|---|---|---|---|---|
| 1 | A (0) | 0 | 4 | 2 | ∞ | ∞ | ∞ | (2,C), (4,B) | relax B ∞→4, C ∞→2 |
| 2 | C (2) | 0 | 4 | 2 | 10 | 12 | ∞ | (4,B), (10,D), (12,E) | C→B is 2+3 = 5, not better than 4. relax D ∞→10, E ∞→12 |
| 3 | B (4) | 0 | 4 | 2 | 9 | 12 | ∞ | (9,D), (10,D), (12,E) | relax D 10→9 |
| 4 | D (9) | 0 | 4 | 2 | 9 | 12 | ∞ | (10,D), (12,E) | target popped, stop |

**Expected output:** `A → B → D`, distance **9 km** (without traffic it is A C B D = 8 km).

### Nearest driver (bonus)
Test: `dijkstra_find_nearest`, `dispatch_nearest_driver_and_fare`

Free MINI drivers are at D and E. Dijkstra starts from pickup A and checks the hash table each time it pops an area. Pops go A(0), C(2), B(3), B(4, stale), D(8). D has a free MINI, so the search stops there. The driver at D is 8 km away and the driver at E is 10 km away. Fare for A → F: 120 + 40 × 13 = **Rs 640**.

## 2. Min-Heap

The heap is stored in an array. For index `i`, the parent is `(i-1)/2` and the children are `2i+1` and `2i+2`. When two keys are equal, the item with the smaller `order` comes out first.

### Heap case 1: push 5, 3, 8, 1, 9, 2 then pop everything
Test: `heap_push_pop_order`

| Operation | Array after | What happened |
|---|---|---|
| push 5 | [5] | |
| push 3 | [3, 5] | 3 < parent 5, swap |
| push 8 | [3, 5, 8] | 8 > parent 3, stays |
| push 1 | [1, 3, 8, 5] | 1 < 5 swap, 1 < 3 swap |
| push 9 | [1, 3, 8, 5, 9] | 9 > parent 3, stays |
| push 2 | [1, 3, 2, 5, 9, 8] | 2 < 8 swap, 2 > 1 stop |
| pop → 1 | [2, 3, 8, 5, 9] | last (8) moved to root, sifted down past 2 |
| pop → 2 | [3, 5, 8, 9] | |
| pop → 3 | [5, 9, 8] | |
| pop → 5 | [8, 9] | |
| pop → 8 | [9] | |
| pop → 9 | [] | |

**Expected output:** pops come out in order 1, 2, 3, 5, 8, 9, and `isValidHeap()` is true after every step.

### Heap case 2: empty and single element
Test: `heap_empty_and_single`

| Operation | Result |
|---|---|
| popMin on empty | returns `false`, output is not changed |
| peek on empty | returns `false` |
| push key 7 (value 42) | [7] |
| popMin | returns `true`, value 42 |
| popMin again | returns `false` |

### Heap case 3: duplicate and negative keys (event ties)
Test: `heap_duplicates_and_negatives`

Items are (key, order) → value: (4, 2) → v0, (-3, 0) → v1, (4, 1) → v2, (-3, 1) → v3.

| Operation | Array after (key/order) |
|---|---|
| push (4,2) | [4/2] |
| push (-3,0) | [-3/0, 4/2] |
| push (4,1) | [-3/0, 4/2, 4/1] |
| push (-3,1) | [-3/0, -3/1, 4/1, 4/2] (swapped with 4/2, then stops because -3/0 comes first) |
| pop → v1 | [-3/1, 4/2, 4/1] |
| pop → v3 | [4/1, 4/2] |
| pop → v2 | [4/2] |
| pop → v0 | [] |

**Expected output:** v1, v3, v2, v0. This is why two simulation events at the same time always come out in the order they were scheduled.

## 3. Linked Queue (waiting requests)

### Queue case 1: plain FIFO
Test: `queue_case1_fifo`

| Operation | Queue (front → rear) | Returned |
|---|---|---|
| enqueue 10, 20, 30 | 10 → 20 → 30 | |
| peekFront | 10 → 20 → 30 | 10 |
| dequeue | 20 → 30 | 10 |
| dequeue | 30 | 20 |
| enqueue 40 | 30 → 40 | |
| dequeue | 40 | 30 |
| dequeue | (empty) | 40 |

### Queue case 2: cancellation with removeById
Test: `queue_case2_cancel_by_id`

| Operation | Queue (front → rear) | Note |
|---|---|---|
| enqueue 101..104 | 101 → 102 → 103 → 104 | |
| removeById 102 | 101 → 103 → 104 | middle node unlinked: `prev->next = cur->next` |
| removeById 102 | 101 → 103 → 104 | returns `false`, already gone |
| removeById 104 | 101 → 103 | rear removed, `rear` moves back to 103 |
| enqueue 105 | 101 → 103 → 105 | new node linked after the correct rear |

**Expected output:** front-to-rear walk gives 101, 103, 105.

### Queue case 3: empty and single element
Test: `queue_case3_empty_and_single`

| Operation | Result |
|---|---|
| dequeue on empty | `false` |
| peekFront on empty | `false` |
| removeById 5 on empty | `false` |
| enqueue 5, removeById 5 | queue empty again, `front = rear = NULL` |
| enqueue 6, dequeue | returns 6, `getFront()` is NULL |

## 4. Trie (area search)

Stored words: `gulshan-e-iqbal`, `gulistan-e-johar`, `gulberg`, `clifton`. Each node has 40 child slots: a-z, 0-9, space, `-`, `.` and `'`.

### Trie case 1: prefix "gul"
Test: `trie_case1_shared_prefix`

| Step | Char | Child index | Node exists? |
|---|---|---|---|
| 1 | g | 6 | yes |
| 2 | u | 20 | yes |
| 3 | l | 11 | yes, this is the prefix node |

Then a DFS below the prefix node visits the children in alphabet order: `b` (gulberg), `i` (gulistan…), `s` (gulshan…).

**Expected output:** `Gulberg`, `Gulistan-e-Johar`, `Gulshan-e-Iqbal` (3 results, limit 5).

### Trie case 2: prefix "CLIF" (upper case)
Test: `trie_case2_case_insensitive`

`indexOf` turns `C` into `c`, so the walk is c(2) → l(11) → i(8) → f(5). The DFS finds `clifton`.

**Expected output:** 1 result, `Clifton`. `contains("Clif")` is false because that node is not the end of a word.

### Trie case 3: prefix "gux" (no match)
Test: `trie_case3_no_match`

| Step | Char | Node exists? |
|---|---|---|
| 1 | g | yes |
| 2 | u | yes |
| 3 | x | **no**, walk stops |

**Expected output:** 0 results and `matchedLength("gux") = 2`. The inspector shows the matched part "gu".

## 5. AVL Tree (leaderboard)

The key is (rating, driver id). A **higher rating comes first**, so it goes to the left. Ratings are stored as hundredths (3.00 → 300). Balance factor = height(left) − height(right).

### AVL case 1: RR, insert 3.00, 2.00, 1.00
Test: `avl_case1_rr`

```
insert 3.00      insert 2.00        insert 1.00 (bf at 3.00 = -2)     after left rotate at 3.00
  3.00             3.00               3.00                               2.00
                      \                  \                             /      \
                      2.00               2.00                        3.00    1.00
                                            \
                                            1.00
```
**Expected output:** one RR rotation (left rotate at #1), root 2.00, height 2.

### AVL case 2: LL, insert 1.00, 2.00, 3.00
Test: `avl_case2_ll`

```
     1.00   (bf +2)                 2.00
     /                             /    \
   2.00         right rotate     3.00   1.00
   /            at 1.00  →
 3.00
```
**Expected output:** one LL rotation, root 2.00.

### AVL case 3: LR, insert 1.00, 3.00, 2.00
Test: `avl_case3_lr`

```
   1.00 (bf +2)        1.00                    2.00
   /                   /                      /    \
 3.00 (bf -1)  →     2.00         →         3.00   1.00
    \                /
    2.00           3.00
   left rotate at 3.00   then right rotate at 1.00
```
**Expected output:** one LR rotation, root 2.00.

### AVL case 4: RL, insert 3.00, 1.00, 2.00
Test: `avl_case4_rl`

```
 3.00 (bf -2)        3.00                      2.00
    \                   \                     /    \
    1.00 (bf +1) →      2.00         →      3.00   1.00
    /                      \
  2.00                     1.00
  right rotate at 1.00   then left rotate at 3.00
```
**Expected output:** one RL rotation, root 2.00.

### Rating update = delete + insert
Test: `dispatch_rating_rules`

A driver starts at 4.00 with 20 ratings counted (sum 80). A 5★ rating gives 85 / 21 = 4.047 → stored as 405. The AVL removes key (400, id) and inserts (405, id). `contains(400, id)` is then false and `contains(405, id)` is true.
