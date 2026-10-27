// app.js: page startup, the /api/state poll (every 500 ms) and the top bar
const POLL_MS = 500;

const App = {
  mapData: null,
  state: null,
  stateAt: 0,
  pollTimer: null,
  serverUp: true,
  started: false,

  async init() {
    API.onServerState = (up) => this.setServerUp(up);
    this.wireTopBar();
    if (typeof Inspector !== "undefined") Inspector.init();
    await this.loadMap();
  },

  async loadMap() {
    try {
      this.mapData = await API.get("/api/map");
    } catch (e) {
      UI.error(e, "Could not load the map");
      return;
    }
    if (!this.started) {
      this.started = true;
      if (typeof MapView !== "undefined") MapView.init(this.mapData);
      if (typeof Booking !== "undefined") Booking.init(this.mapData);
      if (typeof RideHistory !== "undefined") RideHistory.init();
      if (typeof Visualizer !== "undefined") Visualizer.init(this.mapData);
      if (typeof Analytics !== "undefined") Analytics.init();
      requestAnimationFrame((t) => this.frame(t));
    }
    this.schedulePoll(0);
  },

  schedulePoll(delay = POLL_MS) {
    clearTimeout(this.pollTimer);
    this.pollTimer = setTimeout(() => this.poll(), delay);
  },

  async poll() {
    try {
      const s = await API.get("/api/state");
      this.applyState(s);
      this.schedulePoll();
    } catch (e) {
      // banner is shown by setServerUp; keep retrying slowly
      this.schedulePoll(this.serverUp ? POLL_MS : 2000);
    }
  },

  applyState(s) {
    this.state = s;
    this.stateAt = performance.now();
    UI.animateNumber(document.getElementById("cntActive"), s.counters.active);
    UI.animateNumber(document.getElementById("cntWaiting"), s.counters.waiting);
    UI.animateNumber(document.getElementById("cntFree"), s.counters.freeDrivers);
    this.syncControls(s);
    if (typeof MapView !== "undefined") MapView.onState(s);
    if (typeof Booking !== "undefined") Booking.onState(s);
    if (typeof Inspector !== "undefined") Inspector.onState(s);
  },

  // sim minutes "now", extrapolated between polls
  simNow() {
    const s = this.state;
    if (!s) return 0;
    if (s.paused) return s.time;
    const dt = (performance.now() - this.stateAt) / 1000;
    return s.time + Math.min(dt, 2) * s.speed;   // never run far ahead of the server
  },

  frame() {
    const now = this.simNow();
    if (this.state) document.getElementById("simClock").textContent = UI.simClock(now);
    if (typeof MapView !== "undefined") MapView.frame(now);
    requestAnimationFrame(() => this.frame());
  },

  setServerUp(up) {
    if (up === this.serverUp) return;
    this.serverUp = up;
    document.getElementById("serverBanner").hidden = up;
    if (up) UI.toast("Reconnected", "Backend is reachable again", "info", 2500);
  },

  wireTopBar() {
    document.getElementById("retryBtn").addEventListener("click", () => {
      if (!this.mapData) this.loadMap();
      else this.schedulePoll(0);
    });

    document.getElementById("playPauseBtn").addEventListener("click", () => {
      const paused = this.state && this.state.paused;
      this.control({ action: paused ? "resume" : "pause" });
    });

    document.querySelectorAll("#speedControl button").forEach((b) =>
      b.addEventListener("click", () => this.control({ action: "speed", value: Number(b.dataset.speed) })));

    document.getElementById("rushToggle").addEventListener("change", (e) => {
      this.control({ action: "rushHour", value: e.target.checked }).then((ok) => {
        if (ok) UI.toast(e.target.checked ? "Rush hour on" : "Rush hour off",
          e.target.checked ? "Random riders will request rides every 2 sim minutes" : "", e.target.checked ? "warn" : "info");
      });
    });

    document.getElementById("resetBtn").addEventListener("click", async () => {
      if (!confirm("Reset the simulation? All rides, traffic and road blocks are cleared.")) return;
      if (await this.control({ action: "reset" })) {
        UI.toast("Simulation reset", "Map reloaded and 40 drivers re-seeded", "info");
        if (typeof Booking !== "undefined") Booking.clearRide();
        if (typeof MapView !== "undefined") MapView.resetRoads();
      }
    });

    document.getElementById("adminToggle").addEventListener("change", (e) => {
      document.body.classList.toggle("admin-mode", e.target.checked);
      document.getElementById("adminHint").hidden = !e.target.checked;
    });

    document.getElementById("dryRunBtn").addEventListener("click", () => typeof Visualizer !== "undefined" && Visualizer.open());
    document.getElementById("analyticsBtn").addEventListener("click", () => typeof Analytics !== "undefined" && Analytics.toggle());
  },

  async control(body) {
    const buttons = document.querySelectorAll(".sim-controls button, .sim-controls input, #resetBtn");
    buttons.forEach((b) => (b.disabled = true));
    try {
      await API.post("/api/sim/control", body);
      this.schedulePoll(0);
      return true;
    } catch (e) {
      UI.error(e, "Simulation control failed");
      if (this.state) this.syncControls(this.state);
      return false;
    } finally {
      buttons.forEach((b) => (b.disabled = false));
    }
  },

  syncControls(s) {
    document.getElementById("playPauseBtn").classList.toggle("paused", s.paused);
    document.querySelectorAll("#speedControl button").forEach((b) =>
      b.classList.toggle("active", Number(b.dataset.speed) === s.speed));
    const rush = document.getElementById("rushToggle");
    if (rush.checked !== s.rushHour) rush.checked = s.rushHour;
  },
};

document.addEventListener("DOMContentLoaded", () => App.init());
