# Edge Cases

Every API error comes back as `{ "ok": false, "error": "...", "code": "..." }` with a 400 / 404 / 409 status. The server never crashes. Any unexpected exception in a handler is caught and returned as a 500 JSON error. The **Tested by** column names unit tests in `tests/test_main.cpp`. API cases were also checked with curl against the running server.

## Input validation

| Case | How it is handled | Code / status | Tested by |
|---|---|---|---|
| Malformed JSON body | parse error caught | `BAD_JSON` 400 | curl |
| Body is not an object (e.g. `[1,2]`) | rejected | `BAD_JSON` 400 | curl |
| Missing field (`riderName`, `rideType`, `rideId`, ...) | rejected with the field name | `MISSING_FIELD` 400 | curl |
| Empty or spaces-only rider name | name is trimmed first | `MISSING_FIELD` 400 | `dispatch_validation_errors` |
| Rider name over 40 characters | rejected | `INVALID_NAME` 400 | `dispatch_validation_errors` |
| Non-numeric id (`"x"`, `1.5`, 11-digit number) | strict integer parse | `INVALID_FIELD` 400 | curl |
| Negative or out-of-range area id | checked against the graph | `INVALID_AREA` 400 | `dispatch_validation_errors` |
| Pickup == drop | rejected | `SAME_LOCATION` 400 | `dispatch_validation_errors` |
| Unknown ride type | only BIKE / MINI / AC accepted (any case) | `INVALID_RIDE_TYPE` 400 | `models_fare_and_ride_type` |
| Unknown API route | | `NOT_FOUND` 404 | curl |

## Rides

| Case | How it is handled | Code / status | Tested by |
|---|---|---|---|
| No drivers at all / none of that type / none reachable | **not an error**: ride is saved as `WAITING` and put in the queue. It is retried in FIFO order whenever a driver becomes free | 200 | `dispatch_queue_then_served` |
| Same rider already has an active ride | name is normalised, so "Ali" and " ALI " are the same rider | `DUPLICATE_REQUEST` 409 | `dispatch_validation_errors` |
| Destination unreachable when booking | Dijkstra finds no path | `UNREACHABLE` 409 | `dispatch_unreachable_cancels` |
| Destination becomes unreachable during a ride (road block) | BFS check, then the ride is cancelled with reason `"Destination unreachable due to road block"` and the driver is freed | ride `CANCELLED` | `dispatch_unreachable_cancels` |
| Driver cut off from the pickup by a block | driver is freed and the ride goes back to the waiting queue | ride `WAITING` | (code path in `Dispatcher::reroute`) |
| Cancel a ride that does not exist | | `RIDE_NOT_FOUND` 404 | `dispatch_cancel_rules` |
| Cancel a completed ride | | `ALREADY_COMPLETED` 409 | `dispatch_cancel_rules` |
| Cancel an already cancelled ride | | `ALREADY_CANCELLED` 409 | `dispatch_cancel_rules` |
| Cancel a ride in progress | only WAITING and ASSIGNED can be cancelled | `CANCEL_NOT_ALLOWED` 409 | `dispatch_cancel_rules` |
| Rating outside 1–5 (0, 6, -1) | | `INVALID_RATING` 400 | `dispatch_rating_rules` |
| Rating a ride that is not completed | | `NOT_COMPLETED` 409 | `dispatch_rating_rules` |
| Rating the same ride twice | | `ALREADY_RATED` 409 | `dispatch_rating_rules` |
| Old events for a cancelled or re-routed ride still in the heap | each ride has an `eventVersion`. Events with an old version are ignored when popped | — | `dispatch_reroute_on_block` |

## Roads and traffic

| Case | How it is handled | Code / status | Tested by |
|---|---|---|---|
| Traffic factor 0, negative, or > 3 | only 1.0 – 3.0 accepted | `INVALID_TRAFFIC` 400 | `graph_traffic_and_blocking`, `sim_road_edits_validate` |
| Traffic factor sent as a string | must be a number | `INVALID_TRAFFIC` 400 | curl |
| Block / traffic on a road that does not exist | | `ROAD_NOT_FOUND` 404 | `sim_road_edits_validate` |
| Blocking an already blocked road | idempotent: returns ok with "Road already blocked", nothing is re-routed | 200 | `sim_road_edits_validate` |
| Missing `blocked` flag | | `MISSING_FIELD` 400 | curl |
| Road blocked under an active ride | the rest of the route is recomputed with Dijkstra from the end of the current road | ride continues | `dispatch_reroute_on_block` |
| Blocked road on a planned (not started) trip | planned trip is recomputed before pickup | — | `dispatch_block_updates_planned_trip` |

## Search

| Case | How it is handled | Tested by |
|---|---|---|
| Empty query or missing `q` | empty list, not an error | `citymap_real_karachi_file` |
| Very long query (500 characters) | queries over 64 characters return an empty list | `trie_edge_inputs`, curl |
| Special characters (`@#$%`) | characters outside the trie alphabet give no match | `trie_edge_inputs` |
| Prefix with no match | empty list. Inspector shows how much of the prefix matched | `trie_case3_no_match` |
| Upper/lower case, extra spaces | normalised (lower case, trimmed, single spaces) | `trie_case2_case_insensitive` |

## Data structures

| Case | How it is handled | Tested by |
|---|---|---|
| popMin / peek on an empty heap | returns `false`, output untouched | `heap_empty_and_single` |
| dequeue / peek / removeById on an empty queue | returns `false` | `queue_case3_empty_and_single` |
| popFront / remove on an empty list | returns `false` | `list_remove_and_empty` |
| Heap with duplicate and negative keys | tie broken by insertion order | `heap_duplicates_and_negatives` |
| Large input | 10,000 heap pushes, 10,000 queue items, 10,000 hash inserts with rehash, 10,000 list items, 1,000 AVL keys | `heap_large_10k`, `queue_large_10k`, `hash_rehash_10k`, `list_large`, `avl_large_stays_balanced` |
| Many hash collisions | table created with 1 bucket. Rehashing keeps it correct | `hash_many_collisions` |
| AVL duplicate ratings | key is (rating, driver id), so ties are unique. An exact duplicate is rejected | `avl_duplicate_ratings` |
| AVL delete of a key that does not exist | returns `false`, tree unchanged | `avl_delete_and_update` |
| Graph: self-loop, duplicate road, zero/negative km, invalid node | `addEdge` returns -1 | `graph_rejects_bad_edges` |
| Trace log too long | capped at 200 entries, `truncated: true` | `trace_cap` |

## Map file

| Case | Message | Tested by |
|---|---|---|
| File missing | `Map file not found: ...` | `citymap_rejects_bad_files` |
| Not valid JSON | `Map file is not valid JSON: ...` | `citymap_rejects_bad_files` |
| No `areas` / `roads` arrays, or no areas | clear message | `citymap_rejects_bad_files` |
| Duplicate area name (case/space-insensitive) or id | `Duplicate area name: ...` | `citymap_rejects_bad_files` |
| Road to an unknown area | `Road x -> y points to an unknown area` | `citymap_rejects_bad_files` |
| Zero or negative distance | `Road x -> y has a non-positive distance` | `citymap_rejects_bad_files` |
| Self-loop or duplicate road | rejected | (CityMap loader) |
| More than 64 areas or 256 roads | rejected (fixed array sizes) | (CityMap loader) |

On any error, loading is all-or-nothing: the map is cleared and the server refuses to start with the message.

## Frontend

- Every `fetch` is wrapped in try/catch, and server errors are shown as toasts with the server's message.
- If the backend is down, a "Server not running" banner with a Retry button appears. Polling slows down and recovers by itself.
- Buttons are disabled while a request is in flight.
- The form is checked before sending (name, pickup/drop chosen from suggestions, pickup ≠ drop), but the backend is still the source of truth.
- Stale autocomplete responses are dropped using a sequence number.
