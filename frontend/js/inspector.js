/*
 * inspector.js — right panel "DS Inspector".
 *   Live Log : every `trace` entry the backend returns (DS, op, Big-O, detail)
 *   Heap     : event-scheduler min-heap drawn as a binary tree (array order)
 *   Queue    : waiting requests, front -> rear, cards slide in/out
 *   Hash     : buckets with their chains, load factor, rehash count
 *   Trie     : branch for the prefix typed in Pickup/Drop
 *   AVL      : leaderboard tree with balance factors; nodes are keyed by
 *              driver id so rotations animate as nodes slide to new spots
 * Structure tabs poll /api/ds/snapshot once a second while visible.
 */
const SVG_NS = "http://www.w3.org/2000/svg";
const LOG_LIMIT = 250;
const DS_NAMES = ["MinHeap", "Graph", "Dijkstra", "BFS", "HashTable", "LinkedQueue", "LinkedList", "Trie", "AVLTree"];

function svgEl(tag, attrs = {}, parent = null) {
  const el = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, v);
  if (parent) parent.appendChild(el);
  return el;
}

const Inspector = {
  tab: "log",
  filter: "",
  pending: [],
  flushQueued: false,
  triePrefix: "",
  hashTable: "drivers",
  snapTimer: null,
  prev: { heapKeys: new Set(), rehash: null, queueIds: new Map() },

  init() {
    this.log = document.getElementById("traceLog");
    const sel = document.getElementById("logFilter");
    sel.innerHTML += DS_NAMES.map((d) => `<option value="${d}">${d}</option>`).join("");
    sel.addEventListener("change", () => { this.filter = sel.value; this.applyFilter(); });
    document.getElementById("logClear").addEventListener("click", () => { this.log.innerHTML = ""; });

    document.querySelectorAll(".panel-right .tab").forEach((t) =>
      t.addEventListener("click", () => this.showTab(t.dataset.itab)));
    document.querySelectorAll("#hashTableSel button").forEach((b) =>
      b.addEventListener("click", () => {
        this.hashTable = b.dataset.table;
        document.querySelectorAll("#hashTableSel button").forEach((x) => x.classList.toggle("active", x === b));
        this.prev.rehash = null;
        this.refresh();
      }));

    API.onTrace = (entries, truncated) => this.push(entries, truncated);
    setTimeout(() => { if (this.log.querySelector(".skeleton")) this.log.innerHTML = '<p class="empty">Operations will stream here as you use the app.</p>'; }, 1500);
  },

  showTab(name) {
    this.tab = name;
    document.querySelectorAll(".panel-right .tab").forEach((t) => t.classList.toggle("active", t.dataset.itab === name));
    document.querySelectorAll(".itab-body").forEach((b) => b.classList.toggle("active", b.dataset.itabBody === name));
    clearInterval(this.snapTimer);
    if (name !== "log") {
      this.refresh();
      this.snapTimer = setInterval(() => this.refresh(), 1000);
    }
  },

  onState() { /* trace arrives through API.onTrace */ },

  setTriePrefix(q) {
    this.triePrefix = q;
    if (this.tab === "trie") this.refresh();
  },

  // ------------------------------------------------------------ live log
  push(entries, truncated) {
    // a busy rush-hour tick can return 200 entries; keep the newest 60
    const batch = entries.length > 60 ? entries.slice(-60) : entries;
    if (truncated || entries.length > batch.length) {
      this.pending.push({ ds: "DSTrace", op: "truncated", complexity: "", detail: `${entries.length - batch.length} older entries hidden${truncated ? " (server capped at 200)" : ""}` });
    }
    this.pending.push(...batch);
    if (!this.flushQueued) {
      this.flushQueued = true;
      requestAnimationFrame(() => this.flush());
    }
  },

  flush() {
    this.flushQueued = false;
    if (!this.pending.length) return;
    const empty = this.log.querySelector(".empty, .skeleton");
    if (empty) this.log.innerHTML = "";
    const frag = document.createDocumentFragment();
    for (let i = this.pending.length - 1; i >= 0; i--) {   // newest on top
      const e = this.pending[i];
      const row = document.createElement("div");
      row.className = "log-row";
      row.dataset.ds = e.ds;
      if (this.filter && !this.matches(e.ds)) row.hidden = true;
      row.innerHTML = `<span class="ds-tag" data-ds="${UI.escape(e.ds)}">${UI.escape(e.ds)}</span>
        <span class="lr-op">${UI.escape(e.op)}</span><span class="lr-cx">${UI.escape(e.complexity)}</span>
        <span class="lr-detail">${UI.escape(e.detail)}</span>`;
      frag.appendChild(row);
    }
    this.pending = [];
    this.log.prepend(frag);
    while (this.log.children.length > LOG_LIMIT) this.log.lastElementChild.remove();
  },

  matches(ds) { return ds === this.filter || ds.split("+").includes(this.filter); },

  applyFilter() {
    this.log.querySelectorAll(".log-row").forEach((r) => (r.hidden = !!this.filter && !this.matches(r.dataset.ds)));
  },

  // ------------------------------------------------------------ snapshots
  async refresh() {
    if (this.tab === "log" || this.loading) return;
    this.loading = true;
    try {
      const q = `prefix=${encodeURIComponent(this.triePrefix.slice(0, 40))}&table=${this.hashTable}`;
      const snap = await API.get(`/api/ds/snapshot?${q}`);
      if (this.tab === "heap") this.renderHeap(snap.heap, snap.time);
      else if (this.tab === "queue") this.renderQueue(snap.queue);
      else if (this.tab === "hash") this.renderHash(snap.hash);
      else if (this.tab === "trie") this.renderTrie(snap.trie);
      else if (this.tab === "avl") this.renderAVL(snap.avl);
    } catch (e) {
      /* banner / toast handled by API; keep last drawing */
    } finally {
      this.loading = false;
    }
  },

  // ------------------------------------------------------------ heap
  renderHeap(heap, now) {
    const viz = document.getElementById("heapViz");
    const items = heap.items;
    document.getElementById("heapStat").textContent = `${heap.size} events · root = earliest`;
    if (!items.length) {
      viz.innerHTML = '<p class="empty">Heap is empty — no scheduled events. Book a ride or turn on rush hour.</p>';
      document.getElementById("heapArray").innerHTML = "";
      return;
    }
    const shown = items.slice(0, 15);   // 4 levels fit the panel; the array row lists the rest
    const levels = Math.floor(Math.log2(shown.length)) + 1;
    const width = Math.max(viz.clientWidth - 2, Math.pow(2, levels - 1) * 44);
    const height = levels * 62 + 20;
    const abbrev = { DRIVER_ARRIVED_PICKUP: "PICK", RIDE_COMPLETED: "DONE", DRIVER_FREE: "FREE" };
    const key = (e) => `${e.rideId}:${e.type}:${e.time.toFixed(3)}`;
    const pos = (i) => {
      const lvl = Math.floor(Math.log2(i + 1));
      const idx = i - (Math.pow(2, lvl) - 1);
      return [(idx + 0.5) * width / Math.pow(2, lvl), 28 + lvl * 62];
    };

    const svg = svgEl("svg", { width, height });
    for (let i = 1; i < shown.length; i++) {
      const [x1, y1] = pos(Math.floor((i - 1) / 2)), [x2, y2] = pos(i);
      svgEl("path", { class: "tedge", d: `M${x1},${y1} L${x2},${y2}` }, svg);
    }
    const keys = new Set();
    shown.forEach((e, i) => {
      const [x, y] = pos(i);
      const k = key(e);
      keys.add(k);
      const cls = ["tnode", i === 0 ? "root" : "", e.stale ? "stale" : "", this.prev.heapKeys.size && !this.prev.heapKeys.has(k) ? "fresh" : ""].join(" ");
      const g = svgEl("g", { class: cls, transform: `translate(${x},${y})` }, svg);
      svgEl("circle", { r: 19 }, g);
      svgEl("text", { y: -4 }, g).textContent = `t${e.time.toFixed(0)}`;
      svgEl("text", { y: 8, class: "sub" }, g).textContent = abbrev[e.type] || "?";
      svgEl("title", {}, g).textContent = `[${i}] ${e.type} ride #${e.rideId} at t=${e.time.toFixed(1)} min${e.stale ? " (stale — ignored when popped)" : ""}`;
    });
    this.prev.heapKeys = keys;
    viz.replaceChildren(svg);

    document.getElementById("heapArray").innerHTML = items.slice(0, 40)
      .map((e, i) => `<span title="${e.type} ride #${e.rideId}">[${i}] ${e.time.toFixed(1)}</span>`).join("") +
      (items.length > 40 ? `<span>… +${items.length - 40}</span>` : "");
  },

  // ------------------------------------------------------------ queue
  renderQueue(queue) {
    const box = document.getElementById("queueViz");
    document.getElementById("queueStat").textContent = `${queue.size} waiting`;
    if (!box.querySelector(".queue-cards")) {
      box.innerHTML = '<div class="queue-ends"><span>FRONT (dequeue)</span><span>REAR (enqueue)</span></div><div class="queue-cards"></div><p class="queue-ops"></p>';
    }
    const cards = box.querySelector(".queue-cards");
    const ops = box.querySelector(".queue-ops");
    const live = new Map(queue.items.map((q) => [q.rideId, q]));
    const known = this.prev.queueIds;

    for (const [id, el] of known) {
      if (live.has(id)) continue;
      known.delete(id);
      el.classList.add("leaving");
      setTimeout(() => el.remove(), 300);
    }
    queue.items.forEach((q) => {
      let el = known.get(q.rideId);
      if (!el) {
        el = document.createElement("div");
        el.className = "q-card";
        known.set(q.rideId, el);
      }
      el.innerHTML = `<div class="qc-id">#${q.rideId} · ${q.rideType}</div>
        <div class="qc-route">${UI.escape(q.pickupName)} → ${UI.escape(q.dropName)}</div>
        <div class="qc-meta">${UI.escape(q.rider)} · waiting ${Math.max(0, q.waitedMin).toFixed(0)} min</div>`;
      cards.appendChild(el);   // re-append keeps DOM order = FIFO order
    });
    ops.textContent = queue.size
      ? "Front is retried first whenever a driver becomes free. Cancel unlinks a card from anywhere in O(n)."
      : "Queue is empty — every request found a driver. Turn on rush hour to fill it.";
  },

  // ------------------------------------------------------------ hash table
  renderHash(h) {
    const stats = document.getElementById("hashStats");
    const rehashed = this.prev.rehash !== null && h.rehashCount !== this.prev.rehash;
    this.prev.rehash = h.rehashCount;
    stats.innerHTML = `
      <div><b>${h.size}</b><span>entries</span></div>
      <div><b>${h.buckets}</b><span>buckets</span></div>
      <div><b>${h.loadFactor.toFixed(2)}</b><span>load (max 0.75)</span></div>
      <div class="${rehashed ? "flash" : ""}"><b>${h.rehashCount}</b><span>rehashes</span></div>`;
    if (rehashed) UI.toast("Rehash", `${h.table} table grew to ${h.buckets} buckets (load factor passed 0.75)`, "info", 2500);

    const rows = h.chains.map((chain, b) => {
      const nodes = chain.map((k) => `<span class="b-node">${k}</span>`).join('<span class="b-arrow">→</span>');
      return `<div class="bucket"><span class="b-idx">${b}</span><span class="b-slot ${chain.length ? "used" : ""}"></span>${nodes}</div>`;
    }).join("");
    const more = h.buckets > h.shownBuckets ? `<div class="hash-more">Showing ${h.shownBuckets} of ${h.buckets} buckets · longest chain ${h.longestChain}</div>`
      : `<div class="hash-more">Longest chain: ${h.longestChain}</div>`;
    document.getElementById("hashViz").innerHTML = rows + more;
  },

  // ------------------------------------------------------------ trie
  renderTrie(t) {
    const viz = document.getElementById("trieViz");
    document.getElementById("trieStat").textContent = `${t.words} words · ${t.nodeCount} nodes`;
    if (!t.prefix) {
      viz.innerHTML = '<p class="empty">Type in Pickup or Drop to see the Trie walk for that prefix.</p>';
      return;
    }
    const nodes = t.nodes;
    const children = nodes.map(() => []);
    nodes.forEach((n, i) => { if (n.parent >= 0) children[n.parent].push(i); });

    // horizontal tidy layout: x by depth, y by leaf order
    const pos = new Array(nodes.length);
    let leaf = 0;
    const place = (i) => {
      if (!children[i].length) { pos[i] = [nodes[i].depth, leaf++]; return; }
      children[i].forEach(place);
      const ys = children[i].map((c) => pos[c][1]);
      pos[i] = [nodes[i].depth, (Math.min(...ys) + Math.max(...ys)) / 2];
    };
    place(0);
    const dx = 30, dy = 24, padX = 20, padY = 18;
    const maxDepth = Math.max(...nodes.map((n) => n.depth));
    const width = Math.max(viz.clientWidth - 2, padX * 2 + maxDepth * dx + 150);
    const height = Math.max(200, padY * 2 + Math.max(leaf - 1, 0) * dy);
    const xy = (i) => [padX + pos[i][0] * dx, padY + pos[i][1] * dy];

    const svg = svgEl("svg", { width, height });
    nodes.forEach((n, i) => {
      if (n.parent < 0) return;
      const [x1, y1] = xy(n.parent), [x2, y2] = xy(i);
      svgEl("path", { class: `tedge ${n.onPath ? "path" : ""}`, d: `M${x1},${y1} C${(x1 + x2) / 2},${y1} ${(x1 + x2) / 2},${y2} ${x2},${y2}` }, svg);
    });
    nodes.forEach((n, i) => {
      const [x, y] = xy(i);
      const g = svgEl("g", { class: `tnode ${n.onPath ? "path" : ""} ${n.isEnd ? "end" : ""}`, transform: `translate(${x},${y})` }, svg);
      svgEl("circle", { r: 9 }, g);
      svgEl("text", {}, g).textContent = i === 0 ? "•" : n.ch === " " ? "␣" : n.ch;
      if (n.isEnd) {
        const label = svgEl("text", { x: 14, class: "sub" }, g);
        label.setAttribute("text-anchor", "start");
        label.textContent = n.word;
      }
    });
    const words = nodes.filter((n) => n.isEnd).map((n) => n.word);
    const list = document.createElement("div");
    list.className = "trie-words";
    list.innerHTML = words.length
      ? `Completions: ${[...new Set(words)].map((w) => `<span>${UI.escape(w)}</span>`).join("")}`
      : "No stored name continues this prefix.";
    viz.replaceChildren(svg, list);
    const miss = t.matched < t.prefix.length;
    document.getElementById("trieStat").textContent =
      `"${t.prefix}" matched ${t.matched}/${t.prefix.length}${miss ? " — no completions" : ""} · ${t.words} words`;
  },

  // ------------------------------------------------------------ AVL
  renderAVL(avl) {
    const viz = document.getElementById("avlViz");
    const nodes = avl.nodes;
    document.getElementById("avlStat").textContent = `${avl.size} drivers · height ${avl.height} · LL ${avl.rotationCounts.LL} RR ${avl.rotationCounts.RR} LR ${avl.rotationCounts.LR} RL ${avl.rotationCounts.RL}`;
    if (!nodes.length) { viz.innerHTML = '<p class="empty">Tree is empty.</p>'; return; }

    // in-order index = x, depth = y
    const pos = new Map();
    let order = 0;
    const walk = (i, depth) => {
      if (i < 0) return;
      walk(nodes[i].left, depth + 1);
      pos.set(i, [order++, depth]);
      walk(nodes[i].right, depth + 1);
    };
    walk(0, 0);
    const dx = 30, dy = 54, pad = 24;
    const width = Math.max(viz.clientWidth - 2, pad * 2 + (order - 1) * dx);
    const height = pad * 2 + avl.height * dy;
    const xy = (i) => [pad + pos.get(i)[0] * dx, pad + pos.get(i)[1] * dy];

    let svg = viz.querySelector("svg");
    if (!svg) { viz.innerHTML = ""; svg = svgEl("svg", {}, viz); svgEl("g", { class: "edges" }, svg); svgEl("g", { class: "nodes" }, svg); }
    svg.setAttribute("width", width);
    svg.setAttribute("height", height);
    const edgeLayer = svg.querySelector(".edges"), nodeLayer = svg.querySelector(".nodes");
    if (!this.avlCentered) {   // first draw: scroll so the root is in view
      this.avlCentered = true;
      requestAnimationFrame(() => { viz.scrollLeft = xy(0)[0] - viz.clientWidth / 2; });
    }

    // keyed by driver id: existing <g> elements move (CSS transition) instead of being redrawn
    const seenNodes = new Set(), seenEdges = new Set();
    nodes.forEach((n, i) => {
      const [x, y] = xy(i);
      seenNodes.add(String(n.id));
      let g = nodeLayer.querySelector(`g[data-id="${n.id}"]`);
      if (!g) {
        g = svgEl("g", { "data-id": n.id }, nodeLayer);
        svgEl("circle", { r: 14 }, g);
        svgEl("text", { y: -3 }, g);
        svgEl("text", { y: 8, class: "sub" }, g);
        svgEl("title", {}, g);
      }
      g.setAttribute("class", `tnode ${i === 0 ? "root" : ""} ${Math.abs(n.balance) > 1 ? "bad" : ""}`);
      g.style.transform = `translate(${x}px, ${y}px)`;
      const texts = g.querySelectorAll("text");
      texts[0].textContent = n.rating.toFixed(2);
      texts[1].textContent = `bf ${n.balance > 0 ? "+" : ""}${n.balance}`;
      g.querySelector("title").textContent = `${n.name} (#${n.id}) rating ${n.rating.toFixed(2)}, height ${n.height}, balance ${n.balance}`;

      for (const child of [n.left, n.right]) {
        if (child < 0) continue;
        const key = `${n.id}-${nodes[child].id}`;
        seenEdges.add(key);
        const [cx, cy] = xy(child);
        let path = edgeLayer.querySelector(`path[data-key="${key}"]`);
        if (!path) path = svgEl("path", { class: "tedge", "data-key": key }, edgeLayer);
        path.setAttribute("d", `M${x},${y} L${cx},${cy}`);
      }
    });
    nodeLayer.querySelectorAll("g[data-id]").forEach((g) => { if (!seenNodes.has(g.dataset.id)) g.remove(); });
    edgeLayer.querySelectorAll("path[data-key]").forEach((p) => { if (!seenEdges.has(p.dataset.key)) p.remove(); });

    document.getElementById("avlRotations").innerHTML = avl.lastRotations.length
      ? avl.lastRotations.map((r) => `<div>↻ ${UI.escape(r)}</div>`).join("")
      : '<p class="empty">Last rating update needed no rotation. Rate a ride or run rush hour to see LL/RR/LR/RL rotations.</p>';
  },
};
