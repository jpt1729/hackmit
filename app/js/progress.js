import { routineItems, manualRoutineResults } from "./checklist.js";

const STORAGE_KEY = "brain-buddy-reminder-history-v1";
const OUTCOMES = new Set(["prompt_fired", "prompt_acked", "prompt_missed"]);
const tabs = [document.getElementById("clock-tab"), document.getElementById("progress-tab")];
const panels = [document.getElementById("clock-view"), document.getElementById("progress-view")];
const views = document.getElementById("routine-views");
let tabTransition = 0;
const summary = document.getElementById("progress-summary");
const daysRoot = document.getElementById("progress-days");
const scaleRoot = document.getElementById("progress-scale");
let replayEvents = [];
let liveEvents = [];
const dayTooltip = document.createElement("div");
dayTooltip.id = "progress-tooltip";
dayTooltip.className = "routine-tooltip progress-tooltip";
dayTooltip.setAttribute("role", "tooltip");
dayTooltip.hidden = true;
document.body.append(dayTooltip);

function hideDayTooltip() { dayTooltip.hidden = true; }

function showDayTooltip(bar, date, groups, hasData) {
  dayTooltip.replaceChildren();
  const heading = document.createElement("strong");
  heading.textContent = date.toLocaleDateString([], { weekday: "long", month: "short", day: "numeric" });
  dayTooltip.append(heading);
  for (const [label, names] of groups) {
    const section = document.createElement("div");
    const title = document.createElement("strong");
    title.textContent = `${label} (${names.length})`;
    const details = document.createElement("p");
    details.textContent = names.length ? names.join(", ") : "None recorded";
    section.append(title, details);
    dayTooltip.append(section);
  }
  if (!hasData) {
    const empty = document.createElement("p");
    empty.textContent = "No reminder data for this day.";
    dayTooltip.append(empty);
  }
  dayTooltip.hidden = false;
  const bounds = bar.getBoundingClientRect();
  const center = bounds.left + bounds.width / 2;
  const left = Math.max(8, Math.min(center - dayTooltip.offsetWidth / 2, innerWidth - dayTooltip.offsetWidth - 8));
  dayTooltip.style.left = `${left}px`;
  dayTooltip.style.top = `${Math.max(8, bounds.top - dayTooltip.offsetHeight - 10)}px`;
  dayTooltip.style.setProperty("--tail-left", `${Math.max(14, Math.min(center - left, dayTooltip.offsetWidth - 14))}px`);
}

window.addEventListener("scroll", hideDayTooltip, true);
window.addEventListener("resize", hideDayTooltip);
document.addEventListener("keydown", (event) => { if (event.key === "Escape") hideDayTooltip(); });

try {
  const parsed = JSON.parse(localStorage.getItem(STORAGE_KEY) || "[]");
  if (Array.isArray(parsed)) liveEvents = parsed.filter((event) => OUTCOMES.has(event.type) && Number.isFinite(event.ts)).slice(-800);
} catch { /* The current session still works when storage is unavailable. */ }

async function showTab(index, moveFocus = false) {
  hideDayTooltip();
  const version = ++tabTransition;
  const previous = panels.find((panel) => !panel.hidden);
  const oldHeight = views.getBoundingClientRect().height;
  [views, ...panels].forEach((element) => element.getAnimations().forEach((animation) => animation.cancel()));
  tabs.forEach((tab, position) => {
    tab.setAttribute("aria-selected", String(position === index));
    tab.tabIndex = position === index ? 0 : -1;
  });
  if (moveFocus) tabs[index].focus();
  if (previous === panels[index]) return;
  const animate = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (animate && previous) {
    await previous.animate([{ opacity: 1 }, { opacity: 0, transform: "translateY(-4px)" }], { duration: 110, easing: "ease-in", fill: "forwards" }).finished.catch(() => {});
    if (version !== tabTransition) return;
  }
  panels.forEach((panel, position) => {
    panel.hidden = position !== index;
    panel.getAnimations().forEach((animation) => animation.cancel());
  });
  if (animate) {
    const newHeight = views.getBoundingClientRect().height;
    views.animate([{ height: `${oldHeight}px` }, { height: `${newHeight}px` }], { duration: 220, easing: "ease-out" });
    panels[index].animate([{ opacity: 0, transform: "translateY(6px)" }, { opacity: 1, transform: "translateY(0)" }], { duration: 220, easing: "ease-out" });
  }
}

function setupProgress() {
  tabs.forEach((tab, index) => {
    tab.addEventListener("click", () => showTab(index));
    tab.addEventListener("keydown", (event) => {
      let next = index;
      if (event.key === "ArrowRight") next = (index + 1) % tabs.length;
      else if (event.key === "ArrowLeft") next = (index + tabs.length - 1) % tabs.length;
      else if (event.key === "Home") next = 0;
      else if (event.key === "End") next = tabs.length - 1;
      else return;
      event.preventDefault();
      showTab(next, true);
    });
  });
}

function recordProgressEvent(event, mode) {
  if (!OUTCOMES.has(event.type) || !Number.isFinite(Number(event.ts))) return;
  const entry = { id: Number(event.id) || 0, ts: Number(event.ts), type: event.type, detail: String(event.detail || "") };
  const destination = mode === "replay" ? replayEvents : liveEvents;
  if (destination.some((item) => item.id === entry.id && item.ts === entry.ts && item.type === entry.type && item.detail === entry.detail)) return;
  destination.push(entry);
  if (mode === "replay") return;
  liveEvents = destination.filter((item) => item.ts >= Date.now() / 1000 - 60 * 60 * 24 * 45).slice(-800);
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify(liveEvents)); } catch { /* Show the current session without persistence. */ }
}

function localDateKey(ts) {
  const date = new Date(ts * 1000);
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
}

function renderProgress(ts, mode) {
  hideDayTooltip();
  let reference = Number.isFinite(Number(ts)) && Number(ts) > 0 ? new Date(Number(ts) * 1000) : new Date();
  const latestManualDate = manualRoutineResults().map((entry) => entry.date).sort().at(-1);
  if (latestManualDate && latestManualDate > localDateKey(reference.getTime() / 1000)) reference = new Date(`${latestManualDate}T12:00:00`);
  const events = mode === "replay" ? replayEvents : liveEvents;
  let receivedTotal = 0;
  let missedTotal = 0;
  let daysWithData = 0;
  const days = [];
  for (let offset = 6; offset >= 0; offset--) {
    const date = new Date(reference);
    date.setHours(12, 0, 0, 0);
    date.setDate(date.getDate() - offset);
    const key = localDateKey(date.getTime() / 1000);
    const records = new Map();
    for (const event of events) {
      if (localDateKey(event.ts) !== key) continue;
      const item = records.get(event.detail) || { sent: false, received: false, missed: false };
      if (event.type === "prompt_fired") item.sent = true;
      if (event.type === "prompt_acked") item.received = true;
      if (event.type === "prompt_missed") item.missed = true;
      records.set(event.detail, item);
    }
    for (const entry of manualRoutineResults()) {
      if (entry.date !== key) continue;
      records.set(entry.id, { sent: true, received: entry.status === "done", missed: entry.status === "missed", label: entry.label });
    }
    const values = [...records.values()];
    const received = values.filter((item) => item.received).length;
    const missed = values.filter((item) => item.missed && !item.received).length;
    const scheduled = values.filter((item) => item.sent && !item.received && !item.missed).length;
    const hasData = values.length > 0;
    if (hasData) daysWithData++;
    receivedTotal += received;
    missedTotal += missed;
    const groups = [["Completed", []], ["Missed", []], ["Scheduled", []]];
    for (const [id, record] of records) {
      const name = routineItems().find((item) => item.id === id)?.label || record.label || String(id || "Unknown activity").replaceAll("_", " ");
      const group = record.received ? 0 : record.missed ? 1 : 2;
      groups[group][1].push(name);
    }
    days.push({ date, received, missed, scheduled, hasData, groups });
  }
  // All seven bars use the same count scale, rounded up to three integer intervals.
  const largestDay = Math.max(1, ...days.map((day) => day.received + day.missed + day.scheduled));
  const step = Math.ceil(largestDay / 3);
  const maximum = step * 3;
  scaleRoot.replaceChildren();
  for (let tick = 0; tick <= 3; tick++) {
    const label = document.createElement("span");
    label.textContent = String(tick * step);
    label.style.bottom = `${tick / 3 * 100}%`;
    scaleRoot.append(label);
  }
  daysRoot.replaceChildren();
  for (const { date, received, missed, scheduled, hasData, groups } of days) {
    const row = document.createElement("div");
    row.className = "progress-day" + (hasData ? "" : " no-data");
    row.setAttribute("role", "listitem");
    const day = document.createElement("strong");
    day.className = "progress-day-label";
    const weekday = document.createElement("span");
    weekday.textContent = date.toLocaleDateString([], { weekday: "short" });
    const dayDate = document.createElement("span");
    dayDate.textContent = date.toLocaleDateString([], { month: "numeric", day: "numeric" });
    day.append(weekday, dayDate);
    const result = document.createElement("p");
    result.className = "progress-day-result" + (hasData ? " sr-only" : "");
    result.textContent = !hasData ? "No data" : `${received} completed · ${missed} missed · ${scheduled} scheduled`;
    const total = document.createElement("strong");
    total.className = "progress-total";
    total.textContent = hasData ? String(received + missed + scheduled) : "—";
    total.setAttribute("aria-hidden", "true");
    const bar = document.createElement("div");
    bar.className = "progress-bar";
    bar.setAttribute("role", "img");
    bar.setAttribute("aria-label", `${date.toLocaleDateString()}. ${hasData ? groups.map(([label, names]) => `${label}: ${names.join(", ") || "none recorded"}`).join(". ") : "No reminder data"}`);
    bar.addEventListener("pointerenter", () => showDayTooltip(bar, date, groups, hasData));
    bar.addEventListener("pointerleave", hideDayTooltip);
    bar.addEventListener("pointercancel", hideDayTooltip);
    for (const [count, kind] of [[received, "done"], [missed, "missed"], [scheduled, "pending"]]) {
      if (!count) continue;
      const segment = document.createElement("span");
      segment.className = "progress-segment " + kind;
      segment.style.height = `${count / maximum * 100}%`;
      segment.textContent = String(count);
      bar.append(segment);
    }
    row.append(total, bar, day, result);
    daysRoot.append(row);
  }
  summary.textContent = daysWithData
    ? `${receivedTotal} completed · ${missedTotal} missed · ${daysWithData} ${daysWithData === 1 ? "day" : "days"} with data`
    : "No reminder results recorded in this seven-day view.";
}

function resetReplayProgress() { replayEvents = []; }

export { setupProgress, recordProgressEvent, renderProgress, resetReplayProgress };
