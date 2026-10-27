# KarachiChal

A ride dispatch and route optimization simulator for Karachi, built for a Data Structures course project.

Riders book a Bike, Mini or AC car between areas of Karachi. A C++ backend finds the nearest free driver with Dijkstra, works out the route and the fare, and runs a live simulation. The browser shows the map, moving cars, routes, surge zones, analytics and, most importantly, **which data structure is working, what operation ran and its Big-O**.

Every data structure is written by hand. No STL containers are used (no `vector`, `map`, `queue`, `priority_queue`, `pair`, `sort`, ...). Only plain arrays and our own node-based lists are used.

**Member:** Anas Furqan (25K-0577)

## Features

- 30 real Karachi areas and 51 roads. Coordinates are **approximate**. Road length = straight-line (haversine) distance × 1.3, rounded to 1 decimal.
- Nearest-driver matching with Dijkstra. If several drivers are in the closest area, the best-rated one is picked.
- Fares: Bike 60 + 25/km, Mini 120 + 40/km, AC 180 + 55/km, times the surge multiplier.
- Waiting queue (FIFO) when no driver is free, with cancellation.
- Live simulation: 1 real second = 1 sim minute at 1x (2x / 5x and pause). Cars move at 30 km/h along the roads.
- Event scheduler on a min-heap: driver arrives at pickup, ride completed, driver free.
- Traffic (1x / 2x / 3x) and road blocks with live re-routing. A ride is cancelled if its destination becomes unreachable.
- Surge pricing from waiting requests vs. free drivers within 2 hops (BFS).
- Driver leaderboard (AVL tree), Trie autocomplete, ride history (linked list), analytics dashboard.
- Rush hour mode that keeps adding random requests so the queue, heap and surge fill up during a demo.
- **DS Inspector** panel: live log of every operation with its Big-O, plus drawings of the heap, queue, hash table, trie and AVL tree.
- **Dijkstra dry-run visualizer**: step through Dijkstra one pop at a time on the real map.

## Data structures

| Structure | File | Used for | Main operations |
|---|---|---|---|
| Graph (adjacency list) | `backend/ds/Graph` | road network | addEdge O(deg), neighbours O(deg), setTraffic / block O(deg) |
| Min-Heap | `backend/ds/MinHeap` | Dijkstra frontier, event scheduler | push / popMin O(log n), peek O(1) |
| Linked Queue | `backend/ds/LinkedQueue` | waiting requests, BFS | enqueue / dequeue O(1), removeById O(n) |
| Hash Table (chaining) | `backend/ds/HashTable` | drivers, rides, riders, area names, free drivers per area, surge counts | insert / find / erase O(1) avg, rehash O(n) |
| Trie | `backend/ds/Trie` | area search | insert O(L), startsWith O(L + k) |
| AVL Tree | `backend/ds/AVLTree` | driver leaderboard | insert / delete O(log n), top N O(log n + N) |
| Singly Linked List | `backend/ds/LinkedList` | ride history, active rides | pushFront O(1), traverse O(n) |
| BFS | `backend/core/BFS` | surge supply (2 hops), reachability | O(V + E) |
| Dijkstra | `backend/core/Dijkstra` | routes, nearest driver, re-routing, dry run | O((V + E) log V) |

Full table with reasons: [docs/COMPLEXITY.md](docs/COMPLEXITY.md). Hand-traced test cases: [docs/DRY_RUNS.md](docs/DRY_RUNS.md). Edge cases: [docs/EDGE_CASES.md](docs/EDGE_CASES.md).

## Build and run

You need a C++17 compiler. On Windows use MinGW-w64 g++ with **posix threads** (for example the WinLibs build). Older MinGW (GCC 6) does not support `std::thread`.

### Windows

```bat
build.bat          :: build the server  -> build\karachichal.exe
build.bat test     :: build and run the tests
build.bat run      :: build and start the server
build.bat clean
```

### Linux / MinGW make

```sh
make               # build the server -> build/karachichal
make test          # build and run the tests
make run           # build and start the server
make clean
```

Start the server **from the project root** (it looks for `data/` and `frontend/`), then open **http://localhost:8080**. An optional port can be given: `build/karachichal 9000`.

The frontend loads Leaflet 1.9.4, Chart.js 4.4.1 and the Inter font from CDNs, and map tiles from OpenStreetMap, so the browser needs internet access. (CARTO's dark tiles now need an API key, so standard OSM tiles are darkened with a CSS filter instead. Routes never come from a map service. They always come from our Dijkstra.)

## Using it

1. **Book a ride:** type a name, pick pickup and drop from the suggestions (or click two areas on the map), choose the ride type and press *Find Driver*.
2. Watch the driver come to the pickup (blue dashed line) and then the trip (green). The ride card shows the status, ETA and fare, and lets you cancel or rate the ride.
3. Turn on **Admin**, click any road, and set traffic or block it while a ride is active to see re-routing.
4. Turn on **Rush hour** and use **5x** speed to fill the queue, the heap and the surge zones.
5. Open the inspector tabs, **Dry Run Dijkstra** and **Analytics**.

## API

All responses are JSON: `{ "ok": true, "data": ..., "trace": [...], "truncated": false }` or `{ "ok": false, "error": "...", "code": "..." }`.

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/map` | areas and roads |
| GET | `/api/state` | clock, drivers, active rides, queue, surge, changed roads |
| GET | `/api/search?q=` | Trie autocomplete |
| GET | `/api/ride?id=` | one ride |
| POST | `/api/ride/request` | `{riderName, pickupAreaId, dropAreaId, rideType}` |
| POST | `/api/ride/cancel` | `{rideId}` |
| POST | `/api/ride/rate` | `{rideId, rating}` |
| GET | `/api/rider/history?name=` | history, newest first |
| POST | `/api/road/traffic` | `{from, to, factor}` |
| POST | `/api/road/block` | `{from, to, blocked}` |
| GET | `/api/leaderboard` | top 10 drivers |
| GET | `/api/analytics` | stats for the charts |
| POST | `/api/sim/control` | `{action: pause, resume, speed, rushHour, reset, value}` |
| GET | `/api/route?from=&to=` | route preview with fares |
| GET | `/api/dryrun/dijkstra?from=&to=` | step-by-step Dijkstra |
| GET | `/api/ds/snapshot?prefix=&table=` | heap, queue, hash table, trie and AVL state |

## Screenshots

_Add screenshots of the main screen, the DS inspector, the dry-run visualizer and the analytics drawer here._

## Folder structure

```
backend/
  include/     httplib.h, json.hpp (third-party, only for HTTP and JSON)
  ds/          Graph, MinHeap, LinkedQueue, HashTable, Trie, AVLTree, LinkedList
  core/        Models, DSTrace, Dijkstra, BFS, CityMap, SurgeEngine, Dispatcher, Simulator
  api/         Server (all HTTP routes)
  main.cpp
frontend/
  index.html, css/styles.css
  js/          api.js, app.js, map.js, panels.js, inspector.js, visualizer.js, charts.js
data/karachi_map.json
tests/test_main.cpp      console test runner (63 tests)
docs/                    COMPLEXITY.md, DRY_RUNS.md, EDGE_CASES.md
build.bat, Makefile
```
