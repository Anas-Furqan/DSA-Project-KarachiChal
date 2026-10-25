/*
 * panels.js — left panel: Trie-backed autocomplete, booking form, live ride
 * card (status timeline, ETA, cancel, rating) and rider history.
 */

/* ------------------------------------------------------------ Autocomplete */
// One instance per search input. Calls /api/search (Trie) on every keystroke;
// stale responses are dropped with a sequence number.
class Autocomplete {
  constructor(input, onPick) {
    this.input = input;
    this.list = input.parentElement.querySelector(".suggestions");
    this.onPick = onPick;
    this.items = [];
    this.active = -1;
    this.seq = 0;
    this.timer = null;

    input.addEventListener("input", () => {
      input.classList.remove("invalid");
      clearTimeout(this.timer);
      this.timer = setTimeout(() => this.search(), 60);
    });
    input.addEventListener("focus", () => { if (input.value.trim()) this.search(); });
    input.addEventListener("keydown", (e) => this.onKey(e));
    input.addEventListener("blur", () => setTimeout(() => this.close(), 120));
    this.list.addEventListener("mousedown", (e) => {
      const li = e.target.closest("li[data-i]");
      if (li) { e.preventDefault(); this.pick(Number(li.dataset.i)); }
    });
  }

  async search() {
    const q = this.input.value;
    const mySeq = ++this.seq;
    if (typeof Inspector !== "undefined") Inspector.setTriePrefix(q);
    if (!q.trim()) { this.items = []; this.close(); return; }
    try {
      const data = await API.get(`/api/search?q=${encodeURIComponent(q)}`);
      if (mySeq !== this.seq) return;   // a newer keystroke already answered
      this.items = data.suggestions;
      this.active = this.items.length ? 0 : -1;
      this.render(q.trim().toLowerCase());
    } catch (e) {
      if (mySeq === this.seq) this.close();
    }
  }

  render(q) {
    if (!this.items.length) {
      this.list.innerHTML = '<li class="empty">No matching area</li>';
    } else {
      this.list.innerHTML = this.items.map((s, i) => {
        const name = s.name;
        const at = this.matchStart(name.toLowerCase(), q);
        const label = at < 0 ? UI.escape(name)
          : UI.escape(name.slice(0, at)) + "<mark>" + UI.escape(name.slice(at, at + q.length)) + "</mark>" + UI.escape(name.slice(at + q.length));
        return `<li data-i="${i}" role="option" class="${i === this.active ? "active" : ""}"><span>${label}</span><span class="sg-id">#${s.id}</span></li>`;
      }).join("");
    }
    this.list.classList.add("open");
    this.input.setAttribute("aria-expanded", "true");
  }

  // prefix of the whole name, or of a later word (aliases like "iqbal")
  matchStart(name, q) {
    if (name.startsWith(q)) return 0;
    for (let i = 1; i < name.length; i++) {
      if ((name[i - 1] === " " || name[i - 1] === "-") && name.startsWith(q, i)) return i;
    }
    return -1;
  }

  onKey(e) {
    const open = this.list.classList.contains("open") && this.items.length;
    if (e.key === "ArrowDown" && open) { e.preventDefault(); this.move(1); }
    else if (e.key === "ArrowUp" && open) { e.preventDefault(); this.move(-1); }
    else if (e.key === "Enter" && open && this.active >= 0) { e.preventDefault(); this.pick(this.active); }
    else if (e.key === "Escape") this.close();
  }

  move(d) {
    this.active = (this.active + d + this.items.length) % this.items.length;
    this.list.querySelectorAll("li").forEach((li, i) => li.classList.toggle("active", i === this.active));
  }

  pick(i) {
    const s = this.items[i];
    if (!s) return;
    this.input.value = s.name;
    this.close();
    this.onPick(s.id);
  }

  close() {
    this.list.classList.remove("open");
    this.input.setAttribute("aria-expanded", "false");
  }
}

/* ----------------------------------------------------------------- Booking */
const STATUS_ORDER = ["WAITING", "ASSIGNED", "IN_PROGRESS", "COMPLETED"];
const TYPE_LABEL = { BIKE: "Bike", MINI: "Mini", AC: "AC Car" };

const Booking = {
  areas: [],
  pickup: null,
  drop: null,
  type: "MINI",
  rideId: null,
  lastStatus: null,
  finalShown: false,
  routeSeq: 0,

  init(mapData) {
    this.areas = mapData.areas;
    this.$ = (id) => document.getElementById(id);

    const savedName = UI.store.get("kc.rider");
    if (savedName) this.$("riderName").value = savedName;
    this.$("riderName").addEventListener("input", (e) => {
      e.target.classList.remove("invalid");
      UI.store.set("kc.rider", e.target.value);
    });

    this.pickupAC = new Autocomplete(this.$("pickupInput"), (id) => this.setArea("pickup", id));
    this.dropAC = new Autocomplete(this.$("dropInput"), (id) => this.setArea("drop", id));
    // typing after a pick invalidates the selection
    this.$("pickupInput").addEventListener("input", () => { if (this.pickup !== null) this.setArea("pickup", null, true); });
    this.$("dropInput").addEventListener("input", () => { if (this.drop !== null) this.setArea("drop", null, true); });

    document.querySelectorAll(".ride-type").forEach((card) =>
      card.addEventListener("click", () => {
        this.type = card.dataset.type;
        document.querySelectorAll(".ride-type").forEach((c) => c.classList.toggle("active", c === card));
      }));
    document.querySelectorAll(".rt-fare").forEach((el) => (el.textContent = "–"));

    this.$("findDriverBtn").addEventListener("click", () => this.book());
    this.$("cancelRideBtn").addEventListener("click", () => this.cancel());
    this.$("newRideBtn").addEventListener("click", () => this.clearRide());
    this.$("rcStars").addEventListener("click", (e) => {
      const b = e.target.closest("button[data-star]");
      if (b) this.rate(Number(b.dataset.star));
    });

    document.querySelectorAll(".panel-left .tab").forEach((tab) =>
      tab.addEventListener("click", () => {
        document.querySelectorAll(".panel-left .tab").forEach((t) => t.classList.toggle("active", t === tab));
        document.querySelectorAll(".panel-left .tab-body").forEach((b) =>
          b.classList.toggle("active", b.dataset.tabBody === tab.dataset.tab));
        if (tab.dataset.tab === "history" && typeof RideHistory !== "undefined") RideHistory.onShow();
      }));

    // clicking an area on the map fills pickup first, then drop
    MapView.onAreaClick = (id) => {
      if (this.rideId && !this.finalShown) return;
      if (this.pickup === null || (this.pickup !== null && this.drop !== null)) {
        this.setArea("pickup", id);
        if (this.drop === id) this.setArea("drop", null);
      } else if (id !== this.pickup) {
        this.setArea("drop", id);
      }
    };

    const saved = Number(UI.store.get("kc.ride"));
    if (saved) this.track(saved);
  },

  areaName(id) { return this.areas[id] ? this.areas[id].name : "?"; },

  setArea(which, id, keepText = false) {
    this[which] = id;
    const input = this.$(which === "pickup" ? "pickupInput" : "dropInput");
    if (!keepText) input.value = id === null ? "" : this.areaName(id);
    input.classList.remove("invalid");
    MapView.setSelection(this.pickup, this.drop);
    this.refreshRoute();
  },

  // GET /api/route -> distance, ETA and fare per type (with surge)
  async refreshRoute() {
    const seq = ++this.routeSeq;
    const summary = this.$("routeSummary");
    if (this.pickup === null || this.drop === null || this.pickup === this.drop) {
      summary.hidden = true;
      document.querySelectorAll(".rt-fare").forEach((el) => (el.textContent = "–"));
      MapView.showPreview(null);
      return;
    }
    try {
      const r = await API.get(`/api/route?from=${this.pickup}&to=${this.drop}`);
      if (seq !== this.routeSeq) return;
      summary.hidden = false;
      this.$("routeKm").textContent = `${r.distanceKm.toFixed(1)} km`;
      this.$("routeEta").textContent = `~${Math.round(r.etaMin)} min trip`;
      this.setSurgeBadge(this.$("routeSurge"), r.surge);
      for (const t of Object.keys(r.fares)) {
        const el = document.querySelector(`.rt-fare[data-fare="${t}"]`);
        if (el) el.textContent = `Rs ${r.fares[t]}`;
      }
      MapView.showPreview(r.nodes);
    } catch (e) {
      if (seq !== this.routeSeq) return;
      summary.hidden = true;
      MapView.showPreview(null);
      UI.error(e, "No route");
    }
  },

  setSurgeBadge(el, surge) {
    el.hidden = !(surge > 1);
    el.classList.toggle("hot", surge >= 2);
    el.textContent = `⚡ ${surge}x surge`;
  },

  // try to resolve typed-but-not-picked text to an exact area name
  resolveTyped(which) {
    if (this[which] !== null) return;
    const text = this.$(which === "pickup" ? "pickupInput" : "dropInput").value.trim().toLowerCase();
    const hit = this.areas.find((a) => a.name.toLowerCase() === text);
    if (hit) this.setArea(which, hit.id);
  },

  validate() {
    const nameEl = this.$("riderName");
    this.resolveTyped("pickup");
    this.resolveTyped("drop");
    if (!nameEl.value.trim()) { nameEl.classList.add("invalid"); nameEl.focus(); return "Enter the rider's name"; }
    if (this.pickup === null) { this.$("pickupInput").classList.add("invalid"); return "Choose a pickup area from the suggestions"; }
    if (this.drop === null) { this.$("dropInput").classList.add("invalid"); return "Choose a drop area from the suggestions"; }
    if (this.pickup === this.drop) { this.$("dropInput").classList.add("invalid"); return "Pickup and drop must be different"; }
    return null;
  },

  async book() {
    const problem = this.validate();
    if (problem) { UI.toast("Check the form", problem, "warn"); return; }
    const btn = this.$("findDriverBtn");
    UI.setBusy(btn, true);
    try {
      const ride = await API.post("/api/ride/request", {
        riderName: this.$("riderName").value.trim(),
        pickupAreaId: this.pickup,
        dropAreaId: this.drop,
        rideType: this.type,
      });
      this.track(ride.id);
      this.render(ride, null);
      if (ride.status === "WAITING") UI.toast("Request queued", "No free driver of this type is reachable yet — you're in the waiting queue", "warn");
      else UI.toast("Driver found", `${ride.driver.name} is on the way`, "success");
      App.schedulePoll(0);
    } catch (e) {
      UI.error(e, "Booking failed");
    } finally {
      UI.setBusy(btn, false);
    }
  },

  track(rideId) {
    this.rideId = rideId;
    this.lastStatus = null;
    this.finalShown = false;
    UI.store.set("kc.ride", rideId);
    this.setFormLocked(true);
    if (!document.getElementById("rideCard").hidden) return;
    API.get(`/api/ride?id=${rideId}`).then((r) => this.render(r, null)).catch(() => this.clearRide());
  },

  setFormLocked(locked) {
    ["riderName", "pickupInput", "dropInput", "findDriverBtn"].forEach((id) => (this.$(id).disabled = locked));
    document.querySelectorAll(".ride-type").forEach((c) => (c.disabled = locked));
  },

  clearRide() {
    this.rideId = null;
    this.finalShown = false;
    UI.store.del("kc.ride");
    this.$("rideCard").hidden = true;
    this.setFormLocked(false);
    MapView.showRide(null, null);
  },

  // called on every /api/state poll
  onState(s) {
    if (!this.rideId || this.finalShown) return;
    const ride = s.rides.find((r) => r.id === this.rideId);
    if (ride) {
      const driver = s.drivers.find((d) => d.id === ride.driverId) || null;
      this.render(ride, driver);
      return;
    }
    // no longer active: fetch the final record once
    this.finalShown = true;
    API.get(`/api/ride?id=${this.rideId}`).then((r) => {
      this.render(r, null);
      if (r.status === "COMPLETED") UI.toast("Ride completed", `Fare Rs ${r.fare}. Please rate your driver.`, "success");
      else if (r.status === "CANCELLED" && r.cancelReason !== "Cancelled by rider") UI.toast("Ride cancelled", r.cancelReason, "error", 6000);
    }).catch(() => this.clearRide());
  },

  render(r, driver) {
    const card = this.$("rideCard");
    card.hidden = false;
    this.$("rcId").textContent = `Ride #${r.id} · ${TYPE_LABEL[r.rideType] || r.rideType}`;
    this.$("rcRoute").textContent = `${r.pickupName} → ${r.dropName}`;
    const pill = this.$("rcStatus");
    pill.textContent = r.status.replace("_", " ");
    pill.dataset.status = r.status;

    if (r.driver) {
      this.$("rcDriver").textContent = r.driver.name;
      this.$("rcDriverSub").textContent = `★ ${r.driver.rating.toFixed(2)} · ${TYPE_LABEL[r.driver.type]}`;
      this.$("rcAvatar").textContent = r.driver.name.split(" ").map((p) => p[0]).join("").slice(0, 2);
    } else {
      this.$("rcDriver").textContent = r.status === "CANCELLED" ? "No driver" : "Looking for a driver…";
      this.$("rcDriverSub").textContent = r.status === "WAITING" ? "In the waiting queue (FIFO)" : "";
      this.$("rcAvatar").textContent = "?";
    }

    const now = App.simNow();
    let eta = null, label = "ETA";
    if (r.status === "ASSIGNED" && r.etaPickup >= 0) { eta = r.etaPickup - now; label = "to pickup"; }
    else if (r.status === "IN_PROGRESS" && r.etaArrive) { eta = r.etaArrive - now; label = "to drop"; }
    this.$("rcEta").textContent = eta === null ? "–" : `${Math.max(0, Math.ceil(eta))} min`;
    this.$("rcEtaLabel").textContent = label;

    this.$("rcFare").textContent = `Rs ${r.fare}`;
    this.setSurgeBadge(this.$("rcSurge"), r.surge);

    const idx = STATUS_ORDER.indexOf(r.status);
    this.$("rcTimeline").querySelectorAll("li").forEach((li, i) => {
      const done = r.status === "COMPLETED" ? true : i < idx;
      li.classList.toggle("done", done);
      li.classList.toggle("current", i === idx && r.status !== "COMPLETED");
    });

    const reason = this.$("rcReason");
    reason.hidden = r.status !== "CANCELLED";
    reason.textContent = r.cancelReason || "";

    const rate = this.$("rcRate");
    rate.hidden = r.status !== "COMPLETED";
    const stars = this.$("rcStars");
    stars.classList.toggle("locked", r.rating > 0);
    stars.querySelectorAll("button").forEach((b) => b.classList.toggle("on", Number(b.dataset.star) <= r.rating));

    const finished = r.status === "COMPLETED" || r.status === "CANCELLED";
    this.$("cancelRideBtn").hidden = finished;
    this.$("cancelRideBtn").disabled = r.status === "IN_PROGRESS";
    this.$("cancelRideBtn").title = r.status === "IN_PROGRESS" ? "Rides in progress cannot be cancelled" : "";
    this.$("newRideBtn").hidden = !finished;
    if (finished) {
      this.setFormLocked(false);
      UI.store.del("kc.ride");
    }
    MapView.showRide(finished ? null : r, driver);
    this.lastStatus = r.status;
  },

  async cancel() {
    if (!this.rideId) return;
    const btn = this.$("cancelRideBtn");
    btn.disabled = true;
    try {
      const r = await API.post("/api/ride/cancel", { rideId: this.rideId });
      this.finalShown = true;
      this.render(r, null);
      UI.toast("Ride cancelled", `Ride #${r.id} was cancelled`, "info");
      App.schedulePoll(0);
    } catch (e) {
      UI.error(e, "Cannot cancel");
      btn.disabled = false;
    }
  },

  async rate(stars) {
    if (!this.rideId) return;
    try {
      const r = await API.post("/api/ride/rate", { rideId: this.rideId, rating: stars });
      this.render(r, null);
      UI.toast("Thanks!", `You rated ${r.driver ? r.driver.name : "the driver"} ${stars}★ — leaderboard updated`, "success");
    } catch (e) {
      UI.error(e, "Rating failed");
    }
  },
};

/* ------------------------------------------------------------- RideHistory */
const RideHistory = {
  init() {
    this.input = document.getElementById("historyName");
    this.list = document.getElementById("historyList");
    document.getElementById("historyBtn").addEventListener("click", () => this.load());
    this.input.addEventListener("keydown", (e) => { if (e.key === "Enter") this.load(); });
  },

  onShow() {
    const rider = document.getElementById("riderName").value.trim();
    if (!this.input.value.trim() && rider) this.input.value = rider;
    if (this.input.value.trim()) this.load();
  },

  async load() {
    const name = this.input.value.trim();
    if (!name) { this.input.classList.add("invalid"); return; }
    this.input.classList.remove("invalid");
    this.list.innerHTML = '<div class="skeleton sk-row"></div><div class="skeleton sk-row"></div>';
    try {
      const data = await API.get(`/api/rider/history?name=${encodeURIComponent(name)}`);
      this.render(data);
    } catch (e) {
      this.list.innerHTML = '<p class="empty">Could not load history.</p>';
      UI.error(e, "History failed");
    }
  },

  render(data) {
    if (!data.rides.length) {
      this.list.innerHTML = `<p class="empty">No finished rides for “${UI.escape(data.name)}” yet.${
        data.activeRideId > 0 ? ` Active ride #${data.activeRideId} will appear here when it ends.` : ""}</p>`;
      return;
    }
    this.list.innerHTML = data.rides.map((r, i) => `
      <div class="history-item" style="animation-delay:${i * 40}ms">
        <span class="hi-route">#${r.id} ${UI.escape(r.pickupName)} → ${UI.escape(r.dropName)}</span>
        <span class="hi-fare">${r.status === "COMPLETED" ? "Rs " + r.fare : "–"}</span>
        <span class="hi-meta">${r.status.replace("_", " ")} · ${TYPE_LABEL[r.rideType]} · ${UI.simClock(r.endTime >= 0 ? r.endTime : r.requestTime)}${
          r.rating ? " · " + "★".repeat(r.rating) : ""}${r.cancelReason && r.status === "CANCELLED" ? " · " + UI.escape(r.cancelReason) : ""}</span>
        <span class="hi-meta">${r.driver ? UI.escape(r.driver.name) : ""}</span>
      </div>`).join("");
  },
};
