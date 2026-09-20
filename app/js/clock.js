import { routineItems, routineIcon, routineStatus, routineVisualState, selectedRoutine } from "./checklist.js";

function point(angle, radius) {
  const radians = (angle - 90) * Math.PI / 180;
  return { x: 200 + Math.cos(radians) * radius, y: 200 + Math.sin(radians) * radius };
}

let drawnSignature = null;

function drawClock() {
  const face = document.getElementById("clock-face");
  const ticks = Array.from({ length: 60 }, (_, index) => {
    const major = index % 5 === 0;
    const start = point(index * 6, major ? 128 : 133);
    const end = point(index * 6, 139);
    return `<line class="clock-tick ${major ? "major" : ""}" x1="${start.x}" y1="${start.y}" x2="${end.x}" y2="${end.y}"/>`;
  }).join("");
  const numbers = [12, 3, 6, 9].map((hour) => {
    const position = point(hour * 30, 110);
    return `<text class="clock-number" x="${position.x}" y="${position.y}">${hour}</text>`;
  }).join("");
  const items = routineItems();
  // Put overlapping times on outer rings so every bubble remains clickable.
  const positions = [];
  for (const item of items) {
    let radius = 162;
    let position = point(item.minutes / 2, radius);
    while (positions.some((other) => Math.hypot(position.x - other.x, position.y - other.y) < 66)) {
      radius += 70;
      position = point(item.minutes / 2, radius);
    }
    positions.push({ ...position, radius });
  }
  const largestRadius = Math.max(162, ...positions.map((position) => position.radius));
  const span = Math.max(200, largestRadius + 35);
  const crowded = largestRadius > 162;
  const clock = document.getElementById("routine-clock");
  clock.style.minWidth = crowded ? `${span * 2}px` : "";
  clock.style.maxWidth = crowded ? `${span * 2}px` : "";
  face.setAttribute("viewBox", `${200 - span} ${200 - span} ${span * 2} ${span * 2}`);
  const helper = document.getElementById("clock-scroll-help");
  helper.hidden = !crowded;
  const scroll = clock.parentElement;
  scroll.tabIndex = crowded ? 0 : -1;
  scroll.setAttribute("role", "group");
  scroll.setAttribute("aria-label", "Routine clock");
  if (crowded) scroll.setAttribute("aria-describedby", "clock-scroll-help");
  else scroll.removeAttribute("aria-describedby");
  let arc = "";
  if (items.length > 1 && items.at(-1).minutes - items[0].minutes < 720) {
    const start = point(items[0].minutes / 2, 162);
    const end = point(items.at(-1).minutes / 2, 162);
    const largeArc = items.at(-1).minutes - items[0].minutes > 360 ? 1 : 0;
    arc = `<path class="clock-arc" d="M${start.x} ${start.y} A162 162 0 ${largeArc} 1 ${end.x} ${end.y}"/>`;
  }
  const outerRings = [...new Set(positions.map((position) => position.radius))].filter((radius) => radius > 162)
    .map((radius) => `<circle class="clock-extra-ring" cx="200" cy="200" r="${radius}"/>`).join("");
  face.innerHTML = `<circle class="clock-ring" cx="200" cy="200" r="162"/>${outerRings}${arc}${ticks}${numbers}`;
  document.getElementById("clock-activity-count").textContent = `${items.length} ${items.length === 1 ? "activity" : "activities"}`;
  document.getElementById("routine-empty").hidden = items.length !== 0;
  document.getElementById("clock-events").innerHTML = items.map((item, index) => {
    const position = positions[index];
    const left = (position.x - 200 + span) / (span * 2) * 100;
    const top = (position.y - 200 + span) / (span * 2) * 100;
    return `<button type="button" class="clock-event" data-select-routine="${item.id}" style="left:${left}%;top:${top}%">${routineIcon(item)}<span class="clock-event-period" aria-hidden="true">${item.minutes < 720 ? "AM" : "PM"}</span></button>`;
  }).join("");
}

function renderClock(ts, replay = false) {
  const signature = JSON.stringify(routineItems().map(({ id, minutes, icon }) => [id, minutes, icon]));
  if (signature !== drawnSignature) { drawClock(); drawnSignature = signature; }
  const date = ts == null ? null : new Date(Number(ts) * 1000);
  for (const item of routineItems()) {
    const button = document.querySelector('.clock-event[data-select-routine="' + item.id + '"]');
    button.className = "clock-event " + routineVisualState(item);
    button.setAttribute("aria-pressed", String(selectedRoutine()?.id === item.id));
    const status = routineStatus(item);
    button.setAttribute("aria-label", item.label + ", " + item.time + ". " + status + ".");
    button.title = item.label + " · " + item.time + " · " + status;
  }
  document.getElementById("clock-time-label").textContent = replay ? "Recorded time" : "Device time";
  document.getElementById("clock-time").textContent = date && Number.isFinite(date.getTime())
    ? date.toLocaleTimeString("en-US", { hour: "numeric", minute: "2-digit", hour12: true }).replace(/\s?[AP]M/, "") : "—";
  document.getElementById("clock-period").textContent = date && Number.isFinite(date.getTime()) ? (date.getHours() >= 12 ? "PM" : "AM") : "Waiting for update";
}

export { renderClock };
