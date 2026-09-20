import { routineItems, routineIcon, routineStatus, routineVisualState, selectedRoutine, setRoutineTime } from "./checklist.js";

function point(angle, radius) {
  const radians = (angle - 90) * Math.PI / 180;
  return { x: 200 + Math.cos(radians) * radius, y: 200 + Math.sin(radians) * radius };
}

let drawnSignature = null;
const tooltip = document.getElementById("routine-tooltip");
let tooltipButton = null;

function hideTooltip() {
  tooltip.hidden = true;
  tooltipButton?.removeAttribute("aria-describedby");
  tooltipButton = null;
}

function showTooltip(button) {
  hideTooltip();
  const item = routineItems().find((item) => item.id === button.dataset.selectRoutine);
  if (!item) return;
  tooltipButton = button;
  tooltip.querySelector("strong").textContent = item.label;
  tooltip.querySelector("span").textContent = item.time;
  tooltip.hidden = false;
  button.setAttribute("aria-describedby", "routine-tooltip");
  const bounds = button.getBoundingClientRect();
  const center = bounds.left + bounds.width / 2;
  const left = Math.max(8, Math.min(center - tooltip.offsetWidth / 2, window.innerWidth - tooltip.offsetWidth - 8));
  tooltip.style.left = `${left}px`;
  tooltip.style.top = `${Math.max(8, bounds.top - tooltip.offsetHeight - 10)}px`;
  tooltip.style.setProperty("--tail-left", `${Math.max(14, Math.min(center - left, tooltip.offsetWidth - 14))}px`);
}

window.addEventListener("scroll", hideTooltip, true);
window.addEventListener("resize", hideTooltip);
document.addEventListener("keydown", (event) => { if (event.key === "Escape") hideTooltip(); });

function drawClock() {
  hideTooltip();
  const face = document.getElementById("clock-face");
  const ticks = Array.from({ length: 60 }, (_, index) => {
    const major = index % 5 === 0;
    const start = point(index * 6, major ? 116 : 121);
    const end = point(index * 6, 127);
    return `<line class="clock-tick ${major ? "major" : ""}" x1="${start.x}" y1="${start.y}" x2="${end.x}" y2="${end.y}"/>`;
  }).join("");
  const numbers = [12, 3, 6, 9].map((hour) => {
    const position = point(hour * 30, 98);
    return `<text class="clock-number" x="${position.x}" y="${position.y}">${hour}</text>`;
  }).join("");
  const items = routineItems();
  // Put overlapping times on outer rings so every bubble remains clickable.
  const positions = [];
  for (const item of items) {
    const duration = item.endMinutes - item.minutes;
    const width = duration >= 15 ? 48 + Math.min(duration, 360) * 0.55 : 48;
    const sweep = duration >= 15 ? Math.min(duration / 2, 359) : 0;
    const angle = item.minutes / 2 + sweep / 2;
    let radius = 162;
    const sampleArc = () => {
      const steps = Math.max(1, Math.ceil(sweep / 4));
      return Array.from({ length: steps + 1 }, (_, index) => point(item.minutes / 2 + sweep * index / steps, radius));
    };
    let samples = sampleArc();
    // Compare the curved shapes so longer tasks do not force unnecessary empty rings.
    while (positions.some((other) => samples.some((sample) => other.samples.some((p) => Math.hypot(sample.x - p.x, sample.y - p.y) < 61)))) {
      radius += 62;
      samples = sampleArc();
    }
    positions.push({ ...point(angle, radius), radius, width, angle, duration, samples });
  }
  const largestRadius = Math.max(162, ...positions.map((position) => position.radius));
  const span = Math.max(200, ...positions.map(({ radius, width }) => Math.hypot(radius, width / 2) + 30));
  const crowded = largestRadius > 162;
  const clock = document.getElementById("routine-clock");
  clock.style.minWidth = crowded ? `${span * 2}px` : "";
  clock.style.maxWidth = crowded ? `${span * 2}px` : "";
  face.setAttribute("viewBox", `${200 - span} ${200 - span} ${span * 2} ${span * 2}`);
  const scroll = clock.parentElement;
  scroll.tabIndex = crowded ? 0 : -1;
  scroll.setAttribute("role", "group");
  scroll.setAttribute("aria-label", "Routine clock");
  const arc = '<circle class="clock-arc" cx="200" cy="200" r="162" pathLength="100" stroke-dasharray="100" stroke-dashoffset="100" transform="rotate(-90 200 200)"/>';
  const outerRings = [...new Set(positions.map((position) => position.radius))].filter((radius) => radius > 162)
    .map((radius) => `<circle class="clock-extra-ring" cx="200" cy="200" r="${radius}"/>`).join("");
  face.innerHTML = `<circle class="clock-ring" cx="200" cy="200" r="162"/>${outerRings}${arc}${ticks}${numbers}`;
  document.getElementById("routine-empty").hidden = items.length !== 0;
  const events = document.getElementById("clock-events");
  const existing = new Map([...events.children].map((button) => [button.dataset.selectRoutine, button]));
  items.forEach((item, index) => {
    const position = positions[index];
    const left = (position.x - 200 + span) / (span * 2) * 100;
    const top = (position.y - 200 + span) / (span * 2) * 100;
    let button = existing.get(item.id);
    if (!button) {
      button = document.createElement("button");
      button.type = "button";
      button.className = "clock-event";
      button.dataset.selectRoutine = item.id;
      button.addEventListener("pointerenter", () => showTooltip(button));
      button.addEventListener("pointerleave", hideTooltip);
      button.addEventListener("pointercancel", hideTooltip);
      button.addEventListener("click", hideTooltip);
      button.addEventListener("blur", hideTooltip);
    }
    let content = `<span class="clock-event-content">${routineIcon(item)}<span class="clock-event-period" aria-hidden="true">${item.minutes < 720 ? "AM" : "PM"}</span></span>`;

    button.style.setProperty("--task-width", `${position.width}px`);
    button.style.setProperty("--task-angle", `${position.angle}deg`);
    button.style.setProperty("--task-counter-angle", `${-position.angle}deg`);
    button.style.left = `${left}%`;
    button.style.top = `${top}%`;
    button.style.removeProperty("width");
    button.style.removeProperty("height");
    if (position.duration >= 15) {
      const startAngle = item.minutes / 2;
      const sweep = Math.min(position.duration / 2, 359);
      const samples = Array.from({ length: Math.ceil(sweep / 3) + 1 }, (_, i) => i);
      const points = samples.map((_, i) => point(startAngle + sweep * i / (samples.length - 1), position.radius));
      const minX = Math.min(...points.map(p => p.x)) - 30;
      const minY = Math.min(...points.map(p => p.y)) - 30;
      const width = Math.max(...points.map(p => p.x)) + 30 - minX;
      const height = Math.max(...points.map(p => p.y)) + 30 - minY;
      const start = points[0], end = points.at(-1);
      const path = `M${start.x} ${start.y} A${position.radius} ${position.radius} 0 ${sweep > 180 ? 1 : 0} 1 ${end.x} ${end.y}`;
      const center = point(startAngle + sweep / 2, position.radius);
      content = `<svg class="task-curve" viewBox="${minX} ${minY} ${width} ${height}" aria-hidden="true"><path class="task-curve-focus" d="${path}"/><path class="task-curve-edge" d="${path}"/><path class="task-curve-fill" d="${path}"/></svg>` + content;
      button.style.width = `${width / (span * 2) * 100}%`;
      button.style.height = `${height / (span * 2) * 100}%`;
      button.style.left = `${(minX + width / 2 - 200 + span) / (span * 2) * 100}%`;
      button.style.top = `${(minY + height / 2 - 200 + span) / (span * 2) * 100}%`;
      button.style.setProperty("--icon-x", `${(center.x - minX) / width * 100}%`);
      button.style.setProperty("--icon-y", `${(center.y - minY) / height * 100}%`);
      const badge = point(startAngle + sweep / 2 + 20 / position.radius * 180 / Math.PI, position.radius);
      button.style.setProperty("--badge-x", `${(badge.x - minX) / width * 100}%`);
      button.style.setProperty("--badge-y", `${(badge.y - minY) / height * 100}%`);
    }
    if (button.innerHTML !== content) button.innerHTML = content;
    if (events.children[index] !== button) events.insertBefore(button, events.children[index] || null);
    existing.delete(item.id);
  });
  existing.forEach((button) => button.remove());
}

function renderClock() {
  const date = new Date();
  setRoutineTime(date.getTime() / 1000);
  const signature = JSON.stringify(routineItems().map(({ id, minutes, endMinutes, icon }) => [id, minutes, endMinutes, icon]));
  if (signature !== drawnSignature) { drawClock(); drawnSignature = signature; }
  const elapsed = (date.getHours() % 12) * 60 + date.getMinutes();
  const arc = document.querySelector(".clock-arc");
  // Each half-day starts at twelve and progresses clockwise with local time.
  const restarted = elapsed < Number(arc.dataset.elapsed || 0);
  arc.style.transition = restarted ? "none" : "";
  arc.style.strokeDashoffset = String(100 * (1 - elapsed / 720));
  arc.style.visibility = elapsed === 0 ? "hidden" : "visible";
  arc.dataset.elapsed = String(elapsed);
  for (const item of routineItems()) {
    const button = document.querySelector('.clock-event[data-select-routine="' + item.id + '"]');
    button.className = "clock-event " + routineVisualState(item) + (item.endMinutes - item.minutes >= 15 ? " duration-task" : "");
    button.setAttribute("aria-pressed", String(selectedRoutine()?.id === item.id));
    const status = routineStatus(item);
    button.setAttribute("aria-label", item.label + ", " + item.time + ". " + status + ".");
  }
  document.getElementById("clock-time-label").textContent = "Current time";
  document.getElementById("clock-time").textContent = date.toLocaleTimeString("en-US", { hour: "numeric", minute: "2-digit", hour12: true }).replace(/\s?[AP]M/, "");
  document.getElementById("clock-period").textContent = date.getHours() >= 12 ? "PM" : "AM";
}

export { renderClock };
