/*
 * map.js — Leaflet only DRAWS. Every route shown here comes from the C++
 * Dijkstra over our adjacency-list Graph; no routing API is used.
 *
 * Layers: roads (+ wide invisible hit lines), area markers, surge circles,
 * cars, active-ride routes, route preview, dry-run highlights.
 */
const MapView = {
  map: null,
  areas: [],
  roads: [],
  roadLayers: new Map(),      // road id -> { line, hit, x }
  areaMarkers: new Map(),     // area id -> marker
  onAreaClick: null,          // set by Booking
  layers: {},

  init(mapData) {
    this.areas = mapData.areas;
    this.roads = mapData.roads;

    this.map = L.map("map", { zoomControl: true, preferCanvas: false, zoomSnap: 0.25 })
      .setView([24.885, 67.075], 12);
    // CARTO dark_all now answers "API KEY REQUIRED" without a key, so we use
    // standard OSM tiles darkened with a CSS filter (.tiles-dark). To use
    // CARTO with a key, swap the URL for its dark_all URL and drop className.
    L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
      className: "tiles-dark",
      maxZoom: 19,
    }).addTo(this.map);

    const bounds = L.latLngBounds(this.areas.map((a) => [a.lat, a.lng]));
    this.map.fitBounds(bounds.pad(0.06));

    ["roads", "surge", "routes", "areas", "cars", "dryrun"].forEach((name) => {
      this.layers[name] = L.layerGroup().addTo(this.map);
    });

    this.drawRoads();
    this.drawAreas();
    this.map.on("zoomend", () => this.updateLabels());
    this.updateLabels();
  },

  latLng(areaId) {
    const a = this.areas[areaId];
    return a ? [a.lat, a.lng] : null;
  },

  // ------------------------------------------------------------ roads
  drawRoads() {
    for (const r of this.roads) {
      const pts = [this.latLng(r.from), this.latLng(r.to)];
      const line = L.polyline(pts, { className: "road", interactive: false }).addTo(this.layers.roads);
      const hit = L.polyline(pts, { className: "road-hit", weight: 14, opacity: 0 }).addTo(this.layers.roads);
      hit.on("click", (e) => this.onRoadClick(r.id, e.latlng));
      hit.on("mouseover", () => document.body.classList.contains("admin-mode") && line.getElement().classList.add("hover"));
      hit.on("mouseout", () => line.getElement().classList.remove("hover"));
      this.roadLayers.set(r.id, { line, hit, x: null });
      this.styleRoad(r);
    }
  },

  styleRoad(r) {
    const layer = this.roadLayers.get(r.id);
    if (!layer) return;
    const el = layer.line.getElement();
    el.classList.toggle("traffic-2", !r.blocked && r.traffic >= 1.5 && r.traffic < 2.5);
    el.classList.toggle("traffic-3", !r.blocked && r.traffic >= 2.5);
    el.classList.toggle("blocked", r.blocked);
    if (r.blocked && !layer.x) {
      const mid = this.midpoint(r);
      layer.x = L.marker(mid, {
        icon: L.divIcon({ className: "", html: '<div class="block-x">✕</div>', iconSize: [18, 18], iconAnchor: [9, 9] }),
        interactive: false,
      }).addTo(this.layers.roads);
    } else if (!r.blocked && layer.x) {
      layer.x.remove();
      layer.x = null;
    }
  },

  midpoint(r) {
    const a = this.latLng(r.from), b = this.latLng(r.to);
    return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
  },

  // state.roads lists only non-default roads; everything else is normal
  syncRoads(changed) {
    const byId = new Map(changed.map((c) => [c.id, c]));
    for (const r of this.roads) {
      const c = byId.get(r.id);
      const traffic = c ? c.traffic : 1;
      const blocked = c ? c.blocked : false;
      if (r.traffic !== traffic || r.blocked !== blocked) {
        r.traffic = traffic;
        r.blocked = blocked;
        this.styleRoad(r);
      }
    }
  },

  resetRoads() {
    this.syncRoads([]);
  },

  // Admin popover: set traffic 1x/2x/3x or block/unblock. The backend then
  // re-routes every active ride whose remaining path uses this road.
  onRoadClick(roadId, latlng) {
    if (!document.body.classList.contains("admin-mode")) return;
    const r = this.roads.find((x) => x.id === roadId);
    if (!r) return;
    const name = (id) => UI.escape(this.areas[id].name);
    const t = r.traffic;
    const html = `<div class="road-pop">
        <h4>${name(r.from)} ↔ ${name(r.to)}</h4>
        <div class="rp-sub">${r.km.toFixed(1)} km base · effective ${(r.km * t).toFixed(1)} km · road #${r.id}</div>
        <div class="rp-row">
          <button data-f="1" class="${t < 1.5 ? "active" : ""}">1x</button>
          <button data-f="2" class="t2 ${t >= 1.5 && t < 2.5 ? "active" : ""}">2x</button>
          <button data-f="3" class="t3 ${t >= 2.5 ? "active" : ""}">3x</button>
        </div>
        <button class="rp-block ${r.blocked ? "unblock" : ""}">${r.blocked ? "Unblock road" : "Block road"}</button>
      </div>`;
    const popup = L.popup({ closeButton: true, autoPanPadding: [40, 40] }).setLatLng(latlng).setContent(html).openOn(this.map);
    const root = popup.getElement();
    root.querySelectorAll("button[data-f]").forEach((b) =>
      b.addEventListener("click", () => this.setTraffic(r, Number(b.dataset.f), popup)));
    root.querySelector(".rp-block").addEventListener("click", () => this.setBlocked(r, !r.blocked, popup));
  },

  async setTraffic(r, factor, popup) {
    try {
      await API.post("/api/road/traffic", { from: r.from, to: r.to, factor });
      r.traffic = factor;
      this.styleRoad(r);
      popup.close();
      UI.toast(`Traffic ${factor}x`, `${this.areas[r.from].name} ↔ ${this.areas[r.to].name}. Rides using it were re-routed.`, factor > 1 ? "warn" : "info");
      App.schedulePoll(0);
    } catch (e) {
      UI.error(e, "Traffic update failed");
    }
  },

  async setBlocked(r, blocked, popup) {
    try {
      const res = await API.post("/api/road/block", { from: r.from, to: r.to, blocked });
      r.blocked = blocked;
      this.styleRoad(r);
      popup.close();
      UI.toast(res.message, `${this.areas[r.from].name} ↔ ${this.areas[r.to].name}`, blocked ? "warn" : "info");
      App.schedulePoll(0);
    } catch (e) {
      UI.error(e, "Road update failed");
    }
  },

  // ------------------------------------------------------------ areas
  drawAreas() {
    for (const a of this.areas) {
      const icon = L.divIcon({
        className: "area-marker",
        html: `<div class="area-dot"></div><div class="area-label">${UI.escape(a.name)}</div>`,
        iconSize: [12, 12],
        iconAnchor: [6, 6],
      });
      const m = L.marker([a.lat, a.lng], { icon, title: a.name, zIndexOffset: 500 }).addTo(this.layers.areas);
      m.on("click", () => this.onAreaClick && this.onAreaClick(a.id));
      this.areaMarkers.set(a.id, m);
    }
  },

  updateLabels() {
    this.map.getContainer().classList.toggle("show-labels", this.map.getZoom() >= 12.75);
  },

  // highlight the selected pickup / drop markers
  setSelection(pickupId, dropId) {
    for (const [id, m] of this.areaMarkers) {
      const el = m.getElement();
      if (!el) continue;
      el.classList.toggle("pickup", id === pickupId);
      el.classList.toggle("drop", id === dropId);
    }
  },

  // ------------------------------------------------------------ routes
  nodesToLatLngs(nodes) {
    return (nodes || []).map((n) => this.latLng(n)).filter(Boolean);
  },

  // dotted preview of the Dijkstra route while booking
  showPreview(nodes) {
    if (this.previewLine) { this.previewLine.remove(); this.previewLine = null; }
    if (!nodes || nodes.length < 2) return;
    this.previewLine = L.polyline(this.nodesToLatLngs(nodes), { className: "route-line preview", interactive: false })
      .addTo(this.layers.routes);
    this.map.fitBounds(this.previewLine.getBounds().pad(0.35), { maxZoom: 13.5, animate: true, duration: 0.6 });
  },

  // driver -> pickup in blue, pickup -> drop in accent
  showRide(ride, driver) {
    this.myRide = ride;
    this.myDriverId = ride ? ride.driverId : null;
    const key = ride ? JSON.stringify([ride.status, ride.tripNodes, driver && driver.legs.map((l) => l.to)]) : "";
    if (key === this.rideKey) return;   // nothing changed, keep the animated lines
    this.rideKey = key;
    if (this.rideLines) this.rideLines.forEach((l) => l.remove());
    this.rideLines = [];
    if (!ride) return;
    if (ride.status !== "WAITING") this.showPreview(null);

    const legNodes = driver && driver.legs.length ? [driver.legs[0].from, ...driver.legs.map((l) => l.to)] : null;
    if (ride.status === "ASSIGNED" && legNodes) {
      this.rideLines.push(L.polyline(this.nodesToLatLngs(legNodes), { className: "route-line to-pickup", interactive: false }).addTo(this.layers.routes));
    }
    if (ride.status === "ASSIGNED" || ride.status === "WAITING") {
      this.rideLines.push(L.polyline(this.nodesToLatLngs(ride.tripNodes), { className: "route-line trip", interactive: false }).addTo(this.layers.routes));
    }
    if (ride.status === "IN_PROGRESS" && legNodes) {
      this.rideLines.push(L.polyline(this.nodesToLatLngs(legNodes), { className: "route-line trip", interactive: false }).addTo(this.layers.routes));
    }
  },

  // ------------------------------------------------------------ cars
  // Each poll gives every driver's remaining legs with sim start/end times.
  // frame() places the car by the extrapolated sim clock, so it glides at
  // 60 fps between polls; a short exponential ease hides re-route snaps.
  cars: new Map(),           // driver id -> { marker, el, data, lat, lng, heading }
  lastFrame: 0,

  carSvg(type) {
    if (type === "BIKE") {
      return `<svg viewBox="0 0 26 26"><rect class="body" x="10" y="4" width="6" height="18" rx="3"/><rect x="11" y="7" width="4" height="3" rx="1" fill="#0b1220"/><circle cx="13" cy="15" r="2" fill="#0b1220"/></svg>`;
    }
    const long = type === "AC";
    const h = long ? 20 : 17, y = (26 - h) / 2;
    return `<svg viewBox="0 0 26 26"><rect class="body" x="7.5" y="${y}" width="11" height="${h}" rx="3.5"/>
      <rect x="9" y="${y + 3}" width="8" height="4" rx="1.2" fill="#0b1220" opacity="0.85"/>
      <rect x="9" y="${y + h - 5}" width="8" height="2.6" rx="1" fill="#0b1220" opacity="0.6"/>
      ${long ? `<rect x="12" y="${y + 8}" width="2" height="${h - 14}" fill="#0b1220" opacity="0.35"/>` : ""}</svg>`;
  },

  syncCars(drivers) {
    const seen = new Set();
    for (const d of drivers) {
      seen.add(d.id);
      let car = this.cars.get(d.id);
      if (!car) {
        const icon = L.divIcon({ className: "car-marker", html: `<div class="car"><span class="halo"></span>${this.carSvg(d.type)}</div>`, iconSize: [0, 0] });
        const start = this.parkedSpot(d);
        const marker = L.marker(start, { icon, zIndexOffset: 1000, keyboard: false }).addTo(this.layers.cars);
        marker.bindTooltip("", { direction: "top", offset: [0, -12], className: "car-tip" });
        car = { marker, el: marker.getElement().querySelector(".car"), lat: start[0], lng: start[1], heading: 0 };
        this.cars.set(d.id, car);
      }
      car.data = d;
      car.el.className = `car status-${d.status}${d.id === this.myDriverId ? " mine" : ""}`;
      car.marker.setTooltipContent(`<b>${UI.escape(d.name)}</b> · ${d.type} · ★${d.rating.toFixed(2)}<br>${d.status.replace("_", " ")}`);
      car.marker.setZIndexOffset(d.id === this.myDriverId ? 3000 : 1000);
    }
    for (const [id, car] of this.cars) {
      if (!seen.has(id)) { car.marker.remove(); this.cars.delete(id); }
    }
  },

  // parked cars fan out around the area dot so they do not overlap
  parkedSpot(d) {
    const c = this.latLng(d.areaId) || [24.88, 67.07];
    const angle = (d.id * 137.5) * Math.PI / 180;
    const r = 0.0016 + (d.id % 3) * 0.0006;
    return [c[0] + Math.sin(angle) * r, c[1] + Math.cos(angle) * r * 1.1];
  },

  bearing(a, b) {
    const dy = b[0] - a[0];
    const dx = (b[1] - a[1]) * Math.cos(a[0] * Math.PI / 180);
    return Math.atan2(dx, dy) * 180 / Math.PI;   // 0 = north, clockwise
  },

  targetOf(d, now) {
    if (!d.legs || !d.legs.length) return { pos: this.parkedSpot(d), heading: null };
    let leg = d.legs[d.legs.length - 1];
    for (const l of d.legs) { if (now < l.end) { leg = l; break; } }
    const span = leg.end - leg.start;
    const p = span > 0 ? Math.min(1, Math.max(0, (now - leg.start) / span)) : 1;
    const a = this.latLng(leg.from), b = this.latLng(leg.to);
    return { pos: [a[0] + (b[0] - a[0]) * p, a[1] + (b[1] - a[1]) * p], heading: this.bearing(a, b) };
  },

  frame(now) {
    const t = performance.now();
    const dt = this.lastFrame ? Math.min(0.1, (t - this.lastFrame) / 1000) : 0.016;
    this.lastFrame = t;
    const k = 1 - Math.exp(-dt / 0.09);
    for (const car of this.cars.values()) {
      if (!car.data) continue;
      const target = this.targetOf(car.data, now);
      const dLat = target.pos[0] - car.lat, dLng = target.pos[1] - car.lng;
      if (Math.abs(dLat) + Math.abs(dLng) < 1e-7) continue;
      const jump = Math.abs(dLat) + Math.abs(dLng) > 0.02;   // reset / teleport: no ease
      car.lat = jump ? target.pos[0] : car.lat + dLat * k;
      car.lng = jump ? target.pos[1] : car.lng + dLng * k;
      if (target.heading !== null) {
        const diff = ((target.heading - car.heading + 540) % 360) - 180;   // shortest turn
        car.heading += diff * Math.min(1, k * 1.6);
      }
      car.marker.setLatLng([car.lat, car.lng]);
      car.el.style.transform = `rotate(${car.heading.toFixed(1)}deg)`;
    }
  },

  // ------------------------------------------------------------ surge
  // Areas whose BFS supply/demand ratio gives a multiplier > 1 pulse amber
  // (1.25x), orange (1.5x) or red (2x).
  surgeZones: new Map(),     // area id -> { circle, tag, mult }

  syncSurge(list) {
    const live = new Map(list.filter((x) => x.multiplier > 1).map((x) => [x.area, x]));
    for (const [area, z] of this.surgeZones) {
      if (!live.has(area)) { z.circle.remove(); z.tag.remove(); this.surgeZones.delete(area); }
    }
    for (const [area, info] of live) {
      const cls = info.multiplier >= 2 ? "s200" : info.multiplier >= 1.5 ? "s150" : "s125";
      const tip = `${this.areas[area].name}: ${info.demand} waiting / ${info.supply} free drivers within 2 hops → ${info.multiplier}x`;
      let z = this.surgeZones.get(area);
      if (z && z.mult === info.multiplier) { z.tag.setTooltipContent(tip); continue; }
      if (z) { z.circle.remove(); z.tag.remove(); }
      const circle = L.circle(this.latLng(area), {
        radius: 700 + info.multiplier * 350, className: `surge-circle ${cls}`, weight: 1.5, interactive: false,
      }).addTo(this.layers.surge);
      const tag = L.marker(this.latLng(area), {
        icon: L.divIcon({ className: "", html: `<span class="surge-tag ${info.multiplier >= 2 ? "hot" : ""}">⚡${info.multiplier}x</span>`, iconSize: [0, 0] }),
        zIndexOffset: 2000,
      }).addTo(this.layers.surge);
      tag.bindTooltip(tip, { direction: "top", className: "car-tip" });
      this.surgeZones.set(area, { circle, tag, mult: info.multiplier });
    }
  },

  // ------------------------------------------------------------ hooks
  onState(s) {
    this.syncRoads(s.roads || []);
    this.syncCars(s.drivers || []);
    this.syncSurge(s.surge || []);
  },
};
