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

  onRoadClick() { /* admin popover added with traffic controls */ },

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

  // ------------------------------------------------------------ hooks
  onState(s) {
    this.syncRoads(s.roads || []);
  },

  frame() { /* car animation added later */ },
};
