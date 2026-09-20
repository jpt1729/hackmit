import { routineItems, routineIcon, routineStatus, routineVisualState, selectedRoutine } from "./checklist.js";

function point(angle, radius) {
  const radians = (angle - 90) * Math.PI / 180;
  return { x: 200 + Math.cos(radians) * radius, y: 200 + Math.sin(radians) * radius };
}

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
  const start = point(items[0].minutes / 2, 162);
  const end = point(items[items.length - 1].minutes / 2, 162);
  const largeArc = items[items.length - 1].minutes - items[0].minutes > 360 ? 1 : 0;
  face.innerHTML = `<circle class="clock-ring" cx="200" cy="200" r="162"/><path class="clock-arc" d="M${start.x} ${start.y} A162 162 0 ${largeArc} 1 ${end.x} ${end.y}"/>${ticks}${numbers}`;
  document.getElementById("clock-activity-count").textContent = `${items.length} activities`;
  document.getElementById("clock-events").innerHTML = items.map((item) => {
    const position = point(item.minutes / 2, 162);
    return `<button type="button" class="clock-event" data-select-routine="${item.id}" style="left:${position.x / 4}%;top:${position.y / 4}%">${routineIcon(item)}<span class="clock-event-period" aria-hidden="true">${item.minutes < 720 ? "AM" : "PM"}</span></button>`;
  }).join("");
}

function renderClock(ts, replay = false) {
  if (!document.querySelector(".clock-event")) drawClock();
  const date = ts == null ? null : new Date(Number(ts) * 1000);
  for (const item of routineItems()) {
    const button = document.querySelector('.clock-event[data-select-routine="' + item.id + '"]');
    button.className = "clock-event " + routineVisualState(item);
    button.setAttribute("aria-pressed", String(selectedRoutine().id === item.id));
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
