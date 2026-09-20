const STORAGE_KEY = "brain-buddy-reminder-history-v1";
const OUTCOMES = new Set(["prompt_fired", "prompt_acked", "prompt_missed"]);
const tabs = [document.getElementById("clock-tab"), document.getElementById("progress-tab")];
const panels = [document.getElementById("clock-view"), document.getElementById("progress-view")];
const summary = document.getElementById("progress-summary");
const daysRoot = document.getElementById("progress-days");
const scaleRoot = document.getElementById("progress-scale");
let replayEvents = [];
let liveEvents = [];

try {
  const parsed = JSON.parse(localStorage.getItem(STORAGE_KEY) || "[]");
  if (Array.isArray(parsed)) liveEvents = parsed.filter((event) => OUTCOMES.has(event.type) && Number.isFinite(event.ts)).slice(-800);
} catch { /* The current session still works when storage is unavailable. */ }

function showTab(index, moveFocus = false) {
  tabs.forEach((tab, position) => {
    tab.setAttribute("aria-selected", String(position === index));
    tab.tabIndex = position === index ? 0 : -1;
    panels[position].hidden = position !== index;
  });
  if (moveFocus) tabs[index].focus();
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
  const reference = Number.isFinite(Number(ts)) && Number(ts) > 0 ? new Date(Number(ts) * 1000) : new Date();
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
    const values = [...records.values()];
    const received = values.filter((item) => item.received).length;
    const missed = values.filter((item) => item.missed && !item.received).length;
    const unconfirmed = values.filter((item) => item.sent && !item.received && !item.missed).length;
    const hasData = values.length > 0;
    if (hasData) daysWithData++;
    receivedTotal += received;
    missedTotal += missed;
    days.push({ date, received, missed, unconfirmed, hasData });
  }
  // All seven bars use the same count scale, rounded up to three integer intervals.
  const largestDay = Math.max(1, ...days.map((day) => day.received + day.missed + day.unconfirmed));
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
  for (const { date, received, missed, unconfirmed, hasData } of days) {
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
    result.textContent = !hasData ? "No data" : `${received} received · ${missed} missed · ${unconfirmed} unconfirmed`;
    const total = document.createElement("strong");
    total.className = "progress-total";
    total.textContent = hasData ? String(received + missed + unconfirmed) : "—";
    total.setAttribute("aria-hidden", "true");
    const bar = document.createElement("div");
    bar.className = "progress-bar";
    bar.setAttribute("aria-hidden", "true");
    for (const [count, kind] of [[received, "done"], [missed, "missed"], [unconfirmed, "elapsed"]]) {
      if (!count) continue;
      const segment = document.createElement("span");
      segment.className = "progress-segment " + kind;
      segment.style.height = `${count / maximum * 100}%`;
      segment.textContent = String(count);
      bar.append(segment);
    }
    row.title = `${date.toLocaleDateString([], { weekday: "long", month: "short", day: "numeric" })}: ${result.textContent}`;
    row.append(total, bar, day, result);
    daysRoot.append(row);
  }
  summary.textContent = daysWithData
    ? `${receivedTotal} received · ${missedTotal} missed · ${daysWithData} ${daysWithData === 1 ? "day" : "days"} with data`
    : "No reminder results recorded in this seven-day view.";
}

function resetReplayProgress() { replayEvents = []; }

export { setupProgress, recordProgressEvent, renderProgress, resetReplayProgress };
