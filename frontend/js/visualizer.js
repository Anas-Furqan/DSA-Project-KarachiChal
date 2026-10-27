// visualizer.js: step-by-step Dijkstra dry run (Prev / Next / Play)
const Visualizer = {
  data: null,
  i: 0,
  playTimer: null,

  init(mapData) {
    this.areas = mapData.areas;
    this.sheet = document.getElementById("dryRunSheet");
    const sorted = [...this.areas].sort((a, b) => a.name.localeCompare(b.name));
    const opts = sorted.map((a) => `<option value="${a.id}">${UI.escape(a.name)}</option>`).join("");
    document.getElementById("drFrom").innerHTML = opts;
    document.getElementById("drTo").innerHTML = opts;
    const byName = (n) => (this.areas.find((a) => a.name === n) || this.areas[0]).id;
    document.getElementById("drFrom").value = byName("Saddar");
    document.getElementById("drTo").value = byName("Gulshan-e-Iqbal");

    document.getElementById("closeDryRun").addEventListener("click", () => this.close());
    document.getElementById("drLoad").addEventListener("click", () => this.load());
    document.getElementById("drPrev").addEventListener("click", () => this.go(this.i - 1));
    document.getElementById("drNext").addEventListener("click", () => this.go(this.i + 1));
    document.getElementById("drPlay").addEventListener("click", () => this.togglePlay());
    document.getElementById("drScrub").addEventListener("input", (e) => this.go(Number(e.target.value)));
    document.addEventListener("keydown", (e) => {
      if (!this.sheet.classList.contains("open") || e.target.tagName === "SELECT" || e.target.tagName === "INPUT") return;
      if (e.key === "ArrowRight") this.go(this.i + 1);
      else if (e.key === "ArrowLeft") this.go(this.i - 1);
      else if (e.key === "Escape") this.close();
    });
    this.setControls(false);
  },

  open() {
    this.sheet.classList.add("open");
    this.sheet.setAttribute("aria-hidden", "false");
    if (this.data) this.render();
  },

  close() {
    this.stop();
    this.sheet.classList.remove("open");
    this.sheet.setAttribute("aria-hidden", "true");
    MapView.clearDryRun();
  },

  setControls(enabled) {
    ["drPrev", "drNext", "drPlay", "drScrub"].forEach((id) => (document.getElementById(id).disabled = !enabled));
  },

  async load() {
    const from = Number(document.getElementById("drFrom").value);
    const to = Number(document.getElementById("drTo").value);
    if (from === to) { UI.toast("Pick two different areas", "", "warn"); return; }
    const btn = document.getElementById("drLoad");
    btn.disabled = true;
    this.stop();
    try {
      this.data = await API.get(`/api/dryrun/dijkstra?from=${from}&to=${to}`);
      this.i = 0;
      const scrub = document.getElementById("drScrub");
      scrub.max = Math.max(0, this.data.steps.length - 1);
      this.setControls(this.data.steps.length > 0);
      this.render();
      if (!this.data.found) UI.toast("Unreachable", "No path exists — a road block disconnected these areas", "warn");
    } catch (e) {
      UI.error(e, "Dry run failed");
    } finally {
      btn.disabled = false;
    }
  },

  go(i) {
    if (!this.data) return;
    this.i = Math.max(0, Math.min(this.data.steps.length - 1, i));
    this.render();
  },

  togglePlay() {
    if (this.playTimer) { this.stop(); return; }
    if (this.data && this.i >= this.data.steps.length - 1) this.i = -1;
    document.getElementById("drPlay").textContent = "Pause";
    this.playTimer = setInterval(() => {
      if (this.i >= this.data.steps.length - 1) { this.stop(); return; }
      this.go(this.i + 1);
    }, 900);
  },

  stop() {
    clearInterval(this.playTimer);
    this.playTimer = null;
    document.getElementById("drPlay").textContent = "Play";
  },

  name(id) { return this.areas[id] ? this.areas[id].name : "?"; },
  fmt(d) { return d < 0 ? "∞" : d.toFixed(1); },

  render() {
    const d = this.data;
    const steps = d.steps;
    if (!steps.length) return;
    const step = steps[this.i];
    const last = this.i === steps.length - 1;

    // replay settled set up to this step
    const settled = new Set();
    for (let k = 0; k <= this.i; k++) if (!steps[k].stale) settled.add(steps[k].popped);
    const changed = new Set(step.relaxed.map((r) => r.to));

    document.getElementById("drScrub").value = this.i;
    document.getElementById("drStep").textContent = `Step ${this.i + 1} / ${steps.length}`;
    const explain = document.getElementById("drExplain");
    let text = `Step ${this.i + 1}: ${step.explanation}`;
    if (last && d.found) text += ` Shortest path: ${d.pathNames.join(" → ")} = ${d.distanceKm.toFixed(1)} km.`;
    else if (last && !d.found) text += " Heap is empty — destination unreachable.";
    explain.textContent = text;
    explain.classList.toggle("final", last);

    // distance table (fixed order by name so rows do not jump)
    const order = [...this.areas].sort((a, b) => a.name.localeCompare(b.name));
    document.getElementById("drTable").innerHTML =
      "<tbody>" + order.map((a) => {
        const dist = step.distances[a.id];
        const cls = a.id === step.popped ? "current" : changed.has(a.id) ? "changed" : settled.has(a.id) ? "settled" : dist < 0 ? "inf" : "";
        return `<tr class="${cls}"><td>${settled.has(a.id) ? "✓ " : ""}${UI.escape(a.name)}</td><td>${this.fmt(dist)}</td></tr>`;
      }).join("") + "</tbody>";

    const heap = [...step.heap].sort((a, b) => a.dist - b.dist || a.node - b.node);
    document.getElementById("drHeap").innerHTML = heap.length
      ? heap.map((h) => `<div><span>${UI.escape(h.name)}${step.distances[h.node] >= 0 && h.dist > step.distances[h.node] + 1e-9 ? " <em>(stale)</em>" : ""}</span><b>${h.dist.toFixed(1)}</b></div>`).join("")
      : '<p class="empty">Heap is empty.</p>';

    MapView.showDryRun({
      popped: step.popped,
      poppedDist: step.dist,
      settled: [...settled],
      frontier: step.heap.map((h) => h.node),
      relaxed: step.relaxed,
      path: last && d.found ? d.path : null,
      from: d.from,
      to: d.to,
    });
  },
};
