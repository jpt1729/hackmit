import { registerStateListener, registerEventListener } from "./api.js";

const ROOMS = { bedroom: "Bedroom", kitchen: "Kitchen", living: "Living room", living_room: "Living room" };
const status = document.getElementById("location-status");
const mapRoot = document.getElementById("bracelet-map");
const mapError = document.getElementById("location-map-error");
const recenter = document.getElementById("location-recenter");
const openMap = document.getElementById("location-open-map");
const wear = document.getElementById("location-wear");
const room = document.getElementById("location-room");
const confidence = document.getElementById("location-confidence");
const updated = document.getElementById("location-updated");
let mode = "connecting";
let state = null;
let fix = null;
let latestReportTs = null;
let receivedAt = null;
let map = null;
let marker = null;
let accuracyCircle = null;
let following = true;
let displayedPosition = null;

function timestamp(value) {
  return typeof value === "number" && Number.isFinite(value) && value > 0 && Number.isFinite(new Date(value * 1000).getTime()) ? value : null;
}

function gpsFix(report) {
  const gps = report.gps;
  if (!gps || gps.valid === false || gps.demo === true) return null;
  const { latitude, longitude, accuracy } = gps;
  if (typeof latitude !== "number" || !Number.isFinite(latitude) || Math.abs(latitude) > 90 ||
      typeof longitude !== "number" || !Number.isFinite(longitude) || Math.abs(longitude) > 180) return null;
  return {
    latitude, longitude,
    accuracy: typeof accuracy === "number" && Number.isFinite(accuracy) && accuracy >= 0 ? accuracy : null,
    ts: timestamp(gps.ts ?? report.ts)
  };
}

function setText(element, text) {
  if (element.textContent !== text) element.textContent = text;
}

function staleFix() {
  return (mode !== "live" || receivedAt === null || Date.now() - receivedAt >= 30_000 ||
    fix?.ts == null || Math.abs(Date.now() - fix.ts * 1000) >= 60_000);
}

function renderMap() {
  // Wait until the map has a layout before initializing its viewport.
  if (!mapRoot.getClientRects().length) return;
  if (!window.L) {
    mapError.hidden = false;
    setText(mapError, "The map could not load. You can still read the location details below.");
    return;
  }
  if (!map) {
    map = L.map(mapRoot, { scrollWheelZoom: false }).setView([20, 0], 2);
    L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
      maxZoom: 19,
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
    }).on("tileerror", () => {
      mapError.hidden = false;
      setText(mapError, "Some streets could not load. Check your internet connection. Location details are still shown below.");
    }).on("loading", () => { mapError.hidden = true; }).addTo(map);
    map.on("dragstart", () => { following = false; });
  }
  map.invalidateSize({ pan: false });
  if (!fix) {
    marker?.remove();
    accuracyCircle?.remove();
    marker = accuracyCircle = null;
    if (displayedPosition) map.setView([20, 0], 2);
    displayedPosition = null;
    return;
  }
  const position = [fix.latitude, fix.longitude];
  const color = staleFix() ? "#94610b" : "#155f64";
  const label = staleFix() ? "Last reported bracelet location" : "Bracelet";
  if (!marker) {
    marker = L.circleMarker(position, { radius: 11, weight: 4, fillOpacity: 1, color: "#ffffff", fillColor: color })
      .addTo(map).bindTooltip(label, { permanent: true, direction: "top", offset: [0, -14] });
  } else {
    marker.setLatLng(position).setStyle({ fillColor: color }).setTooltipContent(label);
  }
  if (fix.accuracy !== null) {
    if (!accuracyCircle) accuracyCircle = L.circle(position, { weight: 2, fillOpacity: 0.12, interactive: false }).addTo(map);
    accuracyCircle.setLatLng(position).setRadius(fix.accuracy).setStyle({ color, fillColor: color });
    marker.bringToFront();
  } else {
    accuracyCircle?.remove();
    accuracyCircle = null;
  }
  if (!displayedPosition) {
    map.setView(position, 16);
    following = true;
  } else if (following && position.some((value, index) => value !== displayedPosition[index])) {
    map.panTo(position);
  }
  displayedPosition = position;
}

function renderLocation() {
  const stale = fix && staleFix();
  setText(status, fix
    ? `${stale ? "Last reported" : "Bracelet"} coordinates: ${fix.latitude.toFixed(5)}, ${fix.longitude.toFixed(5)}.`
    : "Waiting for GPS. The bracelet’s location will appear when it sends coordinates.");
  const prefix = stale || mode !== "live" ? "Last wrist status: " : "";
  setText(wear, state?.worn === false ? `${prefix}Off wrist — this locates the bracelet, not the wearer.` : state?.worn === true ? `${prefix}Bracelet on wrist.` : "Wrist status unavailable.");
  const roomKey = typeof state?.room === "string" ? state.room.trim().toLowerCase().replace(/[ -]+/g, "_") : "";
  const roomLabel = Object.hasOwn(ROOMS, roomKey) ? ROOMS[roomKey] : null;
  setText(room, roomLabel ? `Reported indoor room: ${roomLabel}.` : "Indoor room unavailable.");
  confidence.hidden = fix?.accuracy == null;
  setText(confidence, fix?.accuracy != null ? `Reported GPS accuracy: ±${Math.round(fix.accuracy)} m. The circle shows the reported accuracy radius.` : "");
  const when = fix?.ts ? new Date(fix.ts * 1000).toLocaleString() : null;
  setText(updated, fix ? `${when ? `GPS fix reported: ${when}.` : "GPS fix time unavailable."}${stale ? " Waiting for a fresh GPS update." : ""}` : "Waiting for a GPS fix from the bracelet.");
  recenter.disabled = !fix;
  recenter.textContent = "Center on bracelet";
  openMap.hidden = !fix;
  if (fix) openMap.href = `https://www.google.com/maps/search/?api=1&query=${encodeURIComponent(`${fix.latitude},${fix.longitude}`)}`;
  else openMap.removeAttribute("href");
  setText(openMap, "Open in Google Maps ↗");
  renderMap();
}

function updateState(report) {
  if (mode === "replay") return;
  state = { room: report.room, worn: report.worn };
  fix = gpsFix(report);
  latestReportTs = timestamp(report.ts);
  receivedAt = Date.now();
  renderLocation();
}

function updateEvent(event) {
  if (mode === "replay") return;
  if (!["room_change", "wear_on", "wear_off"].includes(event.type)) return;
  const ts = timestamp(event.ts);
  if (ts === null || (latestReportTs !== null && ts < latestReportTs)) return;
  state ||= {};
  if (event.type === "room_change") state.room = event.detail;
  else state.worn = event.type === "wear_on";
  latestReportTs = ts;
  renderLocation();
}

function setupLocationMap() {
  registerStateListener(updateState);
  registerEventListener(updateEvent);
  document.addEventListener("mode-change", ({ detail }) => {
    if ((detail.mode === "replay" && mode !== "replay") || detail.mode === "unavailable") {
      state = fix = null;
      latestReportTs = receivedAt = null;
    }
    mode = detail.mode;
    renderLocation();
  });
  recenter.addEventListener("click", () => {
    if (!fix || !map) return;
    following = true;
    map.setView([fix.latitude, fix.longitude], 16);
  });
  new ResizeObserver(renderMap).observe(mapRoot);
  renderLocation();
  setInterval(renderLocation, 5000);
}

export { setupLocationMap };
