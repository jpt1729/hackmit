const DEFAULT_ROUTINE_ITEMS = [
  { id: "breakfast", label: "Breakfast", time: "8:00 AM", minutes: 480, icon: "sun", status: "pending", description: "A little time for your morning meal." },
  { id: "meds_9am", label: "Medication", time: "9:00 AM", minutes: 540, icon: "pill", status: "pending", description: "Time for your medication." },
  { id: "walk", label: "Morning walk", time: "10:00 AM", minutes: 600, icon: "walk", status: "pending", description: "A little time for your morning walk." },
  { id: "water_break", label: "Water break", time: "11:00 AM", minutes: 660, icon: "water", status: "pending", description: "Take a moment for a drink." },
  { id: "lunch_checkin", label: "Lunch", time: "12:00 PM", minutes: 720, icon: "meal", status: "pending", description: "A little time for your midday meal." },
  { id: "rest", label: "Rest time", time: "1:00 PM", minutes: 780, icon: "rest", status: "pending", description: "Settle into a comfortable chair for a break." },
  { id: "puzzle", label: "Puzzle time", time: "2:00 PM", minutes: 840, icon: "puzzle", status: "pending", description: "Enjoy a familiar puzzle or favorite activity." },
  { id: "snack", label: "Afternoon snack", time: "3:00 PM", minutes: 900, icon: "snack", status: "pending", description: "Take a break for a snack." },
  { id: "caregiver_checkin", label: "Caregiver check-in", time: "4:00 PM", minutes: 960, icon: "chat", status: "pending", description: "Say hello or leave your caregiver a voice message." },
  { id: "music", label: "Music time", time: "5:00 PM", minutes: 1020, icon: "music", status: "pending", description: "Listen to some favorite songs." },
  { id: "dinner", label: "Dinner", time: "6:00 PM", minutes: 1080, icon: "meal", status: "pending", description: "A little time for your evening meal." },
  { id: "quiet_time", label: "Quiet time", time: "7:00 PM", minutes: 1140, icon: "moon", status: "pending", description: "Enjoy a calm moment as the day winds down." }
];
const ICONS = {
  sun: '<circle cx="16" cy="16" r="6"/><path d="M16 3v3m0 20v3M3 16h3m20 0h3M7 7l2 2m14 14 2 2M7 25l2-2M23 9l2-2"/>',
  pill: '<path d="M9 23a6 6 0 0 1 0-8l6-6a6 6 0 0 1 8 8l-6 6a6 6 0 0 1-8 0ZM12 12l8 8"/>',
  walk: '<circle cx="18" cy="5" r="2"/><path d="m15 11 4 5 5 1M9 17l3-5 4-2-2 10-5 9m5-9 6 3 2 6"/>',
  water: '<path d="M16 3S6 15 6 21a10 10 0 0 0 20 0C26 15 16 3 16 3Z"/><path d="M11 21a5 5 0 0 0 5 5"/>',
  meal: '<circle cx="17" cy="16" r="8"/><path d="M3 4v8m4-8v8M3 9h4m-2 3v16M29 4v24"/>',
  rest: '<path d="M8 16V9a5 5 0 0 1 5-5h6a5 5 0 0 1 5 5v7M8 16H4v9h24v-9h-4M8 16v5h16v-5M7 25v4m18-4v4"/>',
  puzzle: '<path d="M5 5h8a4 4 0 1 1 6 0h8v8a4 4 0 1 0 0 6v8h-8a4 4 0 1 0-6 0H5v-8a4 4 0 1 1 0-6Z"/>',
  snack: '<path d="M16 10c-10-8-16 5-10 14 4 6 7 4 10 3 3 1 6 3 10-3 6-9 0-22-10-14ZM16 10V5m0 0c2-3 5-3 7-3-1 4-4 5-7 3Z"/>',
  chat: '<path d="M27 16a11 11 0 0 1-11 11h-5l-7 3 2-8A11 11 0 1 1 27 16Z"/><path d="M10 13h12m-12 5h8"/>',
  music: '<path d="M12 23V7l14-3v16M12 11l14-3"/><ellipse cx="8" cy="25" rx="4" ry="3"/><ellipse cx="22" cy="22" rx="4" ry="3"/>',
  moon: '<path d="M25 21A12 12 0 0 1 11 4a12 12 0 1 0 14 17Z"/><path d="M23 3v6m-3-3h6"/>'
};
const ROUTINE_STORAGE_KEY = "granny-nanny-routine-v1";
let items = DEFAULT_ROUTINE_ITEMS.map((item) => ({ ...item }));
const MANUAL_STATUS_KEY = "brain-buddy-manual-status-v1";
let manualResults = [];
try {
  const saved = JSON.parse(localStorage.getItem(MANUAL_STATUS_KEY) || "[]");
  if (Array.isArray(saved)) manualResults = saved.filter((entry) => entry && typeof entry.id === "string" && /^\d{4}-\d{2}-\d{2}$/.test(entry.date) && ["done", "missed", "unknown", "pending"].includes(entry.status)).slice(-2160).map((entry) => ({ ...entry, status: entry.status === "unknown" ? "pending" : entry.status }));
} catch { /* Manual changes still work for this visit. */ }
function todayKey(date = new Date()) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, "0")}-${String(date.getDate()).padStart(2, "0")}`;
}
function manualRoutineResults() { return manualResults; }
function manualStatus(item) { return manualResults.find((entry) => entry.date === todayKey() && entry.id === item.id)?.status; }

function formatTime(minutes) {
  minutes %= 1440;
  const hours = Math.floor(minutes / 60);
  return `${hours % 12 || 12}:${String(minutes % 60).padStart(2, "0")} ${hours < 12 ? "AM" : "PM"}`;
}

function timeRange(start, end) { return `${formatTime(start)} – ${formatTime(end)}`; }

function parseSchedule(value) {
  if (!Array.isArray(value) || value.length > 48) return null;
  const ids = new Set();
  const parsed = [];
  for (const item of value) {
    if (!item || typeof item.id !== "string" || !/^[a-zA-Z0-9_-]{1,80}$/.test(item.id) || ids.has(item.id) ||
        typeof item.label !== "string" || !item.label.trim() || item.label.length > 60 ||
        !Number.isInteger(item.minutes) || item.minutes < 0 || item.minutes >= 1440 || !Object.hasOwn(ICONS, item.icon) ||
        (item.notes !== undefined && (typeof item.notes !== "string" || item.notes.length > 1000))) return null;
    const endMinutes = item.endMinutes ?? Math.min(item.minutes + 30, 1440);
    if (!Number.isInteger(endMinutes) || endMinutes <= item.minutes || endMinutes > 1440) return null;
    ids.add(item.id);
    parsed.push({ id: item.id, label: item.label.trim(), minutes: item.minutes, endMinutes, icon: item.icon, notes: (item.notes || "").trim(), time: timeRange(item.minutes, endMinutes), status: "pending" });
  }
  return parsed.sort((a, b) => a.minutes - b.minutes);
}

items = parseSchedule(items);

try {
  const saved = localStorage.getItem(ROUTINE_STORAGE_KEY);
  if (saved !== null) items = parseSchedule(JSON.parse(saved)) || items;
} catch { /* The schedule can still be edited for this visit. */ }

function saveSchedule() {
  let saved = true;
  try {
    localStorage.setItem(ROUTINE_STORAGE_KEY, JSON.stringify(items.map(({ id, label, minutes, endMinutes, icon, notes = "" }) => ({ id, label, minutes, endMinutes, icon, notes }))));
  } catch { saved = false; }
  document.dispatchEvent(new CustomEvent("routine-change", { detail: { saved } }));
  return saved;
}

function routineFields(label, time, icon, notes = "", endTime) {
  const name = label.trim();
  if (!name || name.length > 60) throw new Error("Enter an activity name of up to 60 characters.");
  if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(time)) throw new Error("Choose a time for the activity.");
  if (!Object.hasOwn(ICONS, icon)) throw new Error("Choose an activity icon.");
  if (typeof notes !== "string" || notes.length > 1000) throw new Error("Keep notes to 1,000 characters or fewer.");
  const [hours, minute] = time.split(":").map(Number);
  const minutes = hours * 60 + minute;
  let endMinutes = Math.min(minutes + 30, 1440);
  if (endTime !== undefined) {
    if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(endTime)) throw new Error("Choose an end time for the activity.");
    const [endHour, endMinute] = endTime.split(":").map(Number);
    endMinutes = endHour * 60 + endMinute || 1440;
    if (endMinutes <= minutes) throw new Error("End time must be after start time. Use 12:00 AM for the end of the day.");
  }
  return { label: name, minutes, endMinutes, time: timeRange(minutes, endMinutes), icon, notes: notes.trim() };
}

function addRoutine(label, time, icon, notes = "", status = "pending", endTime) {
  const fields = routineFields(label, time, icon, notes, endTime);
  if (!["done", "missed", "pending"].includes(status)) throw new Error("Choose Scheduled, Completed, or Missed.");
  if (items.length >= 48) throw new Error("Your routine has 48 activities. Remove an activity before adding another.");
  const item = { id: `custom_${crypto.randomUUID()}`, ...fields, status: "pending" };
  items.push(item);
  return updateRoutine(item.id, label, time, icon, notes, status, endTime);
}

function updateRoutine(id, label, time, icon, notes = "", status, endTime) {
  const item = items.find((entry) => entry.id === id);
  if (!item) throw new Error("This activity has been removed. Close the editor and choose another activity.");
  if (status !== undefined && !["done", "missed", "pending"].includes(status)) throw new Error("Choose Scheduled, Completed, or Missed.");
  Object.assign(item, routineFields(label, time, icon, notes, endTime));
  let statusSaved = true;
  if (status !== undefined) {
    const date = todayKey();
    manualResults = manualResults.filter((entry) => entry.date !== date || entry.id !== id);
    manualResults.push({ date, id, status, label: item.label });
    manualResults = manualResults.slice(-2160);
    try { localStorage.setItem(MANUAL_STATUS_KEY, JSON.stringify(manualResults)); } catch { statusSaved = false; }
  }
  items.sort((a, b) => a.minutes - b.minutes);
  return { item, saved: saveSchedule() && statusSaved };
}

function removeRoutine(id) {
  const item = items.find((entry) => entry.id === id);
  if (!item) return null;
  items = items.filter((entry) => entry.id !== id);
  if (selectedId === id) { selectedId = null; selectionPinned = false; }
  return { item, saved: saveSchedule() };
}

window.addEventListener("storage", (event) => {
  if (event.key !== ROUTINE_STORAGE_KEY) return;
  try {
    const incoming = event.newValue === null ? parseSchedule(DEFAULT_ROUTINE_ITEMS) : parseSchedule(JSON.parse(event.newValue));
    if (!incoming) return;
    const statuses = new Map(items.map((item) => [item.id, item.status]));
    items = incoming.map((item) => ({ ...item, status: statuses.get(item.id) || "pending" }));
    if (!items.some((item) => item.id === selectedId)) { selectedId = null; selectionPinned = false; }
    document.dispatchEvent(new CustomEvent("routine-change", { detail: { saved: true } }));
  } catch { /* Ignore invalid saved schedules. */ }
});

let selectedId = null;
let selectionPinned = false;
let routineTime = null;

function routineItems() { return items; }
function routineIcon(item) { return '<svg viewBox="0 0 32 32" aria-hidden="true">' + ICONS[item.icon] + '</svg>'; }
function routineVisualState(item) {
  const date = new Date();
  const manual = manualResults.find((entry) => entry.date === todayKey(date) && entry.id === item.id);
  if (manual && !manual.automatic && manual.status !== "pending") return manual.status;
  if (item.statusDate === todayKey(date) && ["done", "missed"].includes(item.status)) return item.status;
  return date.getHours() * 60 + date.getMinutes() >= item.endMinutes ? "missed" : "pending";
}

function syncRoutineDeadlines() {
  const date = todayKey();
  const previous = JSON.stringify(manualResults);
  for (const item of items) {
    const entry = manualResults.find((record) => record.date === date && record.id === item.id);
    if (entry && !entry.automatic && entry.status !== "pending") continue;
    const state = routineVisualState(item);
    if (state === "missed") {
      if (entry?.automatic) continue;
      manualResults = manualResults.filter((record) => record.date !== date || record.id !== item.id);
      manualResults.push({ date, id: item.id, label: item.label, status: "missed", automatic: true });
    } else if (entry?.automatic) {
      manualResults = manualResults.filter((record) => record !== entry);
    }
  }
  manualResults = manualResults.slice(-2160);
  if (previous !== JSON.stringify(manualResults)) {
    try { localStorage.setItem(MANUAL_STATUS_KEY, JSON.stringify(manualResults)); } catch { /* Keep the current result in memory. */ }
  }
}
function routineStatus(item) {
  return { pending: "Scheduled", done: "Completed", missed: "Missed" }[routineVisualState(item)];
}
function latestRoutine() {
  const minutes = routineTime ? routineTime.getHours() * 60 + routineTime.getMinutes() : -1;
  return [...items].reverse().find((item) => item.minutes <= minutes) || items[0];
}
function nextRoutine() {
  if (!routineTime) return null;
  const minutes = routineTime.getHours() * 60 + routineTime.getMinutes();
  return items.find((item) => item.status === "pending" && item.minutes >= minutes) || null;
}
function selectedRoutine() {
  return items.find((item) => item.id === selectedId) || nextRoutine() || latestRoutine() || null;
}
function selectRoutine(id) {
  if (!items.some((item) => item.id === id)) return;
  selectedId = id;
  selectionPinned = true;
}
function setRoutineTime(ts) {
  const date = new Date(Number(ts) * 1000);
  if (!Number.isFinite(date.getTime())) return;
  routineTime = date;
  syncRoutineDeadlines();
  if (!selectionPinned) selectedId = (items.find((item) => item.status === "active") || nextRoutine() || latestRoutine())?.id || null;
}

function applyChecklistEvent(event) {
  const item = items.find((entry) => entry.id === event.detail);
  if (!item) return;
  const statuses = { prompt_fired: "active", prompt_acked: "done", prompt_missed: "missed" };
  if (!statuses[event.type]) return;
  item.status = statuses[event.type];
  item.statusDate = todayKey(new Date(Number(event.ts) * 1000));
  if (!selectionPinned && event.type === "prompt_fired") selectedId = item.id;
}

function resetChecklist() {
  items.forEach((item) => { item.status = "pending"; });
  selectedId = null;
  selectionPinned = false;
  routineTime = null;
}

function routineLabel(id) { return items.find((item) => item.id === id)?.label || "Routine"; }
export { addRoutine, updateRoutine, removeRoutine, manualRoutineResults, applyChecklistEvent, resetChecklist, routineLabel,
  routineItems, routineIcon, routineStatus, routineVisualState, selectedRoutine, selectRoutine, setRoutineTime };
