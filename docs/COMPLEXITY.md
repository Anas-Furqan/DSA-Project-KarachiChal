# Time Complexity

Symbols: **V** = areas (30), **E** = roads (51), **n** = items in the structure, **L** = length of the text, **k** = results returned, **D** = drivers (40), **R** = rides so far, **deg** = number of roads at one area.

## Graph (adjacency list): `backend/ds/Graph`

Storage: an array of areas, an array of roads, and for each area a linked list of `EdgeNode` (`head[u]`).

| Operation | Big-O | Why |
|---|---|---|
| addNode | O(1) | write into the next free array slot |
| addEdge | O(deg) | first checks for a duplicate road and walks to the tail of both lists. Linking itself is O(1) |
| neighbors(u) | O(1) to get, O(deg) to walk | returns the head of u's list |
| findRoad(u, v) | O(deg(u)) | walks u's list |
| setTraffic / setBlocked | O(deg) | findRoad, then one array write |
| effectiveWeight / isUsable | O(1) | array lookup: `baseKm × trafficFactor` |
| clear | O(V + E) | frees every edge node |
| Memory | O(V + E) | a matrix would need O(V²) and O(V) to list neighbours |

**Why a list and not a matrix:** the city is sparse (each area has about 3–4 roads). Dijkstra and BFS only ever ask "what are the neighbours of u", and a list answers that in O(deg).

## Min-Heap: `backend/ds/MinHeap`

A binary heap in a dynamic array (`new` + double when full).

| Operation | Big-O | Why |
|---|---|---|
| push | O(log n) | add at the end, sift up at most the height of the tree |
| popMin | O(log n) | move the last item to the root, sift down |
| peek | O(1) | root is `data[0]` |
| isEmpty / size | O(1) | counter |
| grow | O(n), amortised O(1) per push | copy into an array twice the size |
| itemAt (snapshot) | O(1) each | the inspector draws the array as it is |

**Why a heap:** a sorted array has O(1) pop but O(n) insert, and an unsorted list has O(1) insert but O(n) pop. Dijkstra and the event scheduler mix pushes and pops, and the heap does both in O(log n).

## Linked Queue: `backend/ds/LinkedQueue`

| Operation | Big-O | Why |
|---|---|---|
| enqueue | O(1) | link after `rear` |
| dequeue | O(1) | unlink `front` |
| peekFront / size / isEmpty | O(1) | pointer / counter |
| removeById (cancel) | O(n) | walk to find the node, then unlink in O(1) |
| contains | O(n) | walk |

**Why a linked queue:** FIFO is fair to riders. Both ends are O(1) with no shifting and no fixed size, and a cancelled ride can be unlinked from the middle without moving the others.

## Hash Table (separate chaining): `backend/ds/HashTable`

String keys (integer keys are stored as text), int values. Buckets are an array of `HashNode` linked lists. The hash function is FNV-1a. When the load factor goes over 0.75, the table doubles and rehashes.

| Operation | Big-O | Why |
|---|---|---|
| insert | O(1) average, O(n) worst | hash, then push at the front of one short chain |
| find / contains / get | O(1) average, O(n) worst | hash, then walk one chain |
| erase | O(1) average | unlink from the chain |
| addTo (counter) | O(1) average | find + update |
| rehash | O(n) | every node is re-linked into the new bucket array. It is rare, so amortised O(1) per insert |
| loadFactor / size | O(1) | counters |
| longestChain | O(n + buckets) | for the inspector only |

Where it is used: drivers by id, rides by id, riders by name, area name → id, free drivers per (area, type), pending requests per area (surge demand), requests per area (analytics).

**Why a hash table:** these are all "look up by key" questions. A BST would be O(log n), a hash table is O(1) on average.

## Trie: `backend/ds/Trie`

| Operation | Big-O | Why |
|---|---|---|
| insert | O(L) | one node per character |
| contains | O(L) | walk L edges |
| startsWith(prefix) | O(L + k) | walk L edges, then a DFS that stops after k results (k ≤ 5) |
| matchedLength | O(L) | walk until a character is missing |
| branchView | O(L × 40 + m) | path plus siblings, then m subtree nodes (capped at 60) |

**Why a trie:** checking every area name for a prefix costs O(V × L) on each keystroke. The trie costs O(L) no matter how many names are stored.

## AVL Tree: `backend/ds/AVLTree`

| Operation | Big-O | Why |
|---|---|---|
| insert | O(log n) | BST insert + at most one rotation (single or double) per level going back up |
| remove | O(log n) | BST delete + rebalance on the way up |
| contains | O(log n) | height is at most about 1.44 log₂ n |
| topN(N) | O(log n + N) | in-order walk that stops after N |
| rotation | O(1) | changes three pointers and two heights |
| rating update | O(log n) | remove the old key + insert the new key |

**Why AVL:** the leaderboard changes after every rated ride and is always read in sorted order. A sorted array needs O(n) shifting per update. A plain BST can turn into a linked list (O(n)) when ratings arrive in sorted order. AVL rotations stop that from happening.

## Singly Linked List: `backend/ds/LinkedList`

| Operation | Big-O | Why |
|---|---|---|
| pushFront | O(1) | new head |
| popFront | O(1) | unlink head |
| traverse (history) | O(n) | head → tail = newest → oldest |
| remove / contains | O(n) | walk |

Used for each rider's history (newest first) and for the list of active ride ids.

## Algorithms

| Algorithm | Big-O | Notes |
|---|---|---|
| Dijkstra shortestPath | O((V + E) log V) | each road can push at most once from each side, and every push/pop is O(log) on the heap. Stops early when the target is popped |
| Dijkstra findNearest (driver) | O((V + E) log V) worst case | stops at the first popped area where the free-driver hash table says count > 0 (O(1) average check per pop) |
| Dijkstra traceSteps (dry run) | O((V + E) log V) time, O(steps × (V + heap)) memory | saves the distance table and heap for every pop |
| BFS hopsFrom / isReachable | O(V + E) | our LinkedQueue + a plain `int hops[]` visited array |
| BFS sumWithinHops (surge supply) | O(V + E) | stops expanding at depth 2 |
| Surge recompute (each tick) | O(V × (V + E)) worst case | one BFS per area, but only for areas where someone is waiting |
| Fare | O(1) | (base + perKm × km) × surge |

## Dispatcher operations

| Operation | Big-O |
|---|---|
| requestRide | O(L) name check + 2 × Dijkstra + O(1) hash work + O(D) to pick the best-rated driver in the nearest area |
| cancelRide | O(1) average lookup, + O(q) to unlink a waiting ride, + processQueue if a driver became free |
| rateRide | O(1) lookup + O(log D) AVL update |
| processQueue | O(q × (V + E) log V): every waiting ride is retried once, in FIFO order |
| onRoadChanged | O(A × (V + E) log V) for A active rides that use the road |
| handleEvent (scheduler tick) | O(k log n) for k due events |
| analytics | O(R + D + 5V) (top 5 areas found by 5 linear scans) |
| leaderboard | O(log D + 10) |
