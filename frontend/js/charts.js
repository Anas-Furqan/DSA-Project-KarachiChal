/*
 * charts.js — bottom analytics drawer: stat tiles, Chart.js charts and the
 * driver leaderboard (AVL in-order top 10). Refreshes every 3 s while open.
 */
// Ride-type colours validated for the dark surface (dataviz palette check)
const TYPE_COLORS = { BIKE: "#d97706", MINI: "#059669", AC: "#3b82f6" };
const SERIES = "#10b981";
const INK = "#8b97ad";
const GRID = "rgba(255,255,255,0.06)";

const Analytics = {
  open: false,
  charts: {},
  timer: null,
  prevRanks: new Map(),   // driver id -> rank, to highlight movers

  init() {
    this.drawer = document.getElementById("analyticsDrawer");
    document.getElementById("closeDrawer").addEventListener("click", () => this.toggle(false));
    document.addEventListener("keydown", (e) => { if (e.key === "Escape" && this.open) this.toggle(false); });
    document.getElementById("leaderboardBody").innerHTML =
      '<tr><td colspan="5"><div class="skeleton sk-row" style="height:120px"></div></td></tr>';
  },

  toggle(force) {
    this.open = typeof force === "boolean" ? force : !this.open;
    this.drawer.classList.toggle("open", this.open);
    this.drawer.setAttribute("aria-hidden", String(!this.open));
    clearInterval(this.timer);
    if (this.open) {
      this.refresh();
      this.timer = setInterval(() => this.refresh(), 3000);
    }
  },

  async refresh() {
    try {
      const [stats, lb] = await Promise.all([API.get("/api/analytics"), API.get("/api/leaderboard")]);
      this.renderTiles(stats);
      this.renderCharts(stats);
      this.renderLeaderboard(lb.top);
    } catch (e) {
      UI.error(e, "Analytics failed");
    }
  },

  renderTiles(a) {
    const set = (k, v, dec = 0) => UI.animateNumber(document.querySelector(`[data-stat="${k}"]`), v, dec);
    set("totalRides", a.totalRides);
    set("completed", a.completed);
    set("cancelled", a.cancelled);
    set("avgWaitMin", a.avgWaitMin, 1);
    set("avgFare", Math.round(a.avgFare));
    document.getElementById("topEarner").textContent = a.topEarner && a.topEarner.earnings > 0
      ? `${a.topEarner.name} · Rs ${Math.round(a.topEarner.earnings)}` : "–";
  },

  baseOptions(extra = {}) {
    return Object.assign({
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 450, easing: "easeOutCubic" },
      plugins: {
        legend: { display: false },
        tooltip: { backgroundColor: "#0d1424", borderColor: "rgba(255,255,255,0.16)", borderWidth: 1, titleColor: "#e6ebf5", bodyColor: "#e6ebf5", padding: 10 },
      },
      scales: {
        x: { grid: { display: false }, ticks: { color: INK, font: { size: 11 } }, border: { color: GRID } },
        y: { beginAtZero: true, grid: { color: GRID }, ticks: { color: INK, font: { size: 11 }, precision: 0 }, border: { display: false } },
      },
    }, extra);
  },

  // create once, then update data so Chart.js animates the change
  upsert(key, canvasId, config) {
    const chart = this.charts[key];
    if (!chart) { this.charts[key] = new Chart(document.getElementById(canvasId), config); return; }
    chart.data.labels = config.data.labels;
    config.data.datasets.forEach((ds, i) => Object.assign(chart.data.datasets[i], { data: ds.data, backgroundColor: ds.backgroundColor }));
    chart.update();
  },

  renderCharts(a) {
    if (typeof Chart === "undefined") return;
    const hours = a.ridesPerHour.length ? a.ridesPerHour : [0];
    this.upsert("hourly", "chartHourly", {
      type: "line",
      data: {
        labels: hours.map((_, h) => UI.simClock(h * 60)),
        datasets: [{ label: "Completed rides", data: hours, borderColor: SERIES, backgroundColor: "rgba(16,185,129,0.12)", fill: true, borderWidth: 2, tension: 0.3, pointRadius: 4, pointHoverRadius: 6, pointBackgroundColor: SERIES }],
      },
      options: this.baseOptions({ interaction: { mode: "index", intersect: false } }),
    });

    const types = ["BIKE", "MINI", "AC"];
    this.upsert("types", "chartTypes", {
      type: "doughnut",
      data: {
        labels: ["Bike", "Mini", "AC Car"],
        datasets: [{ data: types.map((t) => a.byType[t] || 0), backgroundColor: types.map((t) => TYPE_COLORS[t]), borderColor: "#0f1626", borderWidth: 2, hoverOffset: 6 }],
      },
      options: {
        responsive: true, maintainAspectRatio: false, cutout: "62%",
        animation: { duration: 450 },
        plugins: {
          legend: { position: "bottom", labels: { color: "#e6ebf5", boxWidth: 10, boxHeight: 10, font: { size: 11 } } },
          tooltip: this.baseOptions().plugins.tooltip,
        },
      },
    });

    const busiest = a.busiest.length ? a.busiest : [{ name: "No rides yet", requests: 0 }];
    this.upsert("busiest", "chartBusiest", {
      type: "bar",
      data: {
        labels: busiest.map((b) => b.name),
        datasets: [{ label: "Requests", data: busiest.map((b) => b.requests), backgroundColor: SERIES, borderRadius: 4, borderSkipped: "start", maxBarThickness: 22 }],
      },
      options: this.baseOptions({
        indexAxis: "y",
        scales: {
          x: { beginAtZero: true, grid: { color: GRID }, ticks: { color: INK, font: { size: 11 }, precision: 0 }, border: { display: false } },
          y: { grid: { display: false }, ticks: { color: "#e6ebf5", font: { size: 11 } }, border: { color: GRID } },
        },
      }),
    });
  },

  renderLeaderboard(top) {
    const body = document.getElementById("leaderboardBody");
    if (!top.length) { body.innerHTML = '<tr><td colspan="5" class="empty">No drivers</td></tr>'; return; }
    body.innerHTML = top.map((d, i) => {
      const before = this.prevRanks.get(d.id);
      const moved = before !== undefined && before !== d.rank;
      return `<tr class="${moved ? "moved" : ""}" style="animation-delay:${i * 25}ms">
        <td>${d.rank}</td><td>${UI.escape(d.name)}</td><td>${d.type}</td>
        <td class="rating">★ ${d.rating.toFixed(2)}</td><td>${d.trips}</td></tr>`;
    }).join("");
    this.prevRanks = new Map(top.map((d) => [d.id, d.rank]));
  },
};
