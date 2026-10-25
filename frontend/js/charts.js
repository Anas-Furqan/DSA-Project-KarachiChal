/*
 * charts.js — bottom analytics drawer: stat tiles, Chart.js charts and the
 * driver leaderboard (AVL in-order top 10). Refreshes every 3 s while open.
 */
const Analytics = {
  open: false,
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
      const lb = await API.get("/api/leaderboard");
      this.renderLeaderboard(lb.top);
    } catch (e) {
      UI.error(e, "Leaderboard failed");
    }
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
