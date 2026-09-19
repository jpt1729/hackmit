const ROUTINE_ITEMS = [
  { id: "breakfast", label: "Breakfast", time: "8:00 AM", minutes: 480, icon: "sun", status: "pending", description: "A little time for your morning meal." },
  { id: "meds_9am", label: "Medication", time: "9:00 AM", minutes: 540, icon: "pill", status: "pending", description: "Your medication reminder is part of your morning routine." },
  { id: "walk", label: "Morning walk", time: "10:00 AM", minutes: 600, icon: "walk", status: "pending", description: "A little time for your morning walk." }
];
const ICONS = {
  sun: '<circle cx="16" cy="16" r="6"/><path d="M16 3v3m0 20v3M3 16h3m20 0h3M7 7l2 2m14 14 2 2M7 25l2-2M23 9l2-2"/>',
  pill: '<path d="M9 23a6 6 0 0 1 0-8l6-6a6 6 0 0 1 8 8l-6 6a6 6 0 0 1-8 0ZM12 12l8 8"/>',
  walk: '<circle cx="18" cy="5" r="2"/><path d="m15 11 4 5 5 1M9 17l3-5 4-2-2 10-5 9m5-9 6 3 2 6"/>'
};
const STATUS = { pending: "Scheduled", active: "Reminder sent", done: "Received", missed: "Not yet acknowledged" };
let selectedId = null;
let selectionPinned = false;
let routineTime = null;

function routineItems() { return ROUTINE_ITEMS; }
function routineIcon(item) { return '<svg viewBox="0 0 32 32" aria-hidden="true">' + ICONS[item.icon] + '</svg>'; }
function routineStatus(item) { return STATUS[item.status]; }
function nextRoutine() {
  if (!routineTime) return null;
  const minutes = routineTime.getHours() * 60 + routineTime.getMinutes();
  return ROUTINE_ITEMS.find((item) => item.status === "pending" && item.minutes >= minutes) || null;
}
function selectedRoutine() {
  return ROUTINE_ITEMS.find((item) => item.id === selectedId) || nextRoutine() || ROUTINE_ITEMS[0];
}
function selectRoutine(id) {
  if (!ROUTINE_ITEMS.some((item) => item.id === id)) return;
  selectedId = id;
  selectionPinned = true;
}
function setRoutineTime(ts) {
  const date = new Date(Number(ts) * 1000);
  if (!Number.isFinite(date.getTime())) return;
  routineTime = date;
  if (!selectionPinned) selectedId = (ROUTINE_ITEMS.find((item) => item.status === "active") || nextRoutine() || selectedRoutine()).id;
}

function renderChecklist(container) {
  if (!container) return;
  // Keep the buttons mounted so polling never takes away keyboard focus.
  if (!container.querySelector(".routine-list")) {
    container.innerHTML = '<ol class="routine-list">' + ROUTINE_ITEMS.map((item) => `
      <li class="checklist-item" data-routine-id="${item.id}">
        <button type="button" class="routine-select" data-select-routine="${item.id}">
          <span class="routine-icon" aria-hidden="true">${routineIcon(item)}</span>
          <span><span class="routine-label">${item.label}</span><span class="routine-time">${item.time}</span></span>
          <span class="routine-status"></span><span class="routine-arrow" aria-hidden="true">›</span>
        </button>
      </li>`).join("") + "</ol>";
  }
  for (const item of ROUTINE_ITEMS) {
    const row = container.querySelector('[data-routine-id="' + item.id + '"]');
    row.className = "checklist-item " + item.status;
    const button = row.querySelector("button");
    button.setAttribute("aria-pressed", String(selectedRoutine().id === item.id));
    button.setAttribute("aria-label", item.label + ", " + item.time + ". " + STATUS[item.status] + ". Show reminder.");
    row.querySelector(".routine-status").textContent = (item.status === "done" ? "✓ " : "") + STATUS[item.status];
  }
}

function applyChecklistEvent(event) {
  const item = ROUTINE_ITEMS.find((entry) => entry.id === event.detail);
  if (!item) return;
  const statuses = { prompt_fired: "active", prompt_acked: "done", prompt_missed: "missed" };
  if (!statuses[event.type]) return;
  item.status = statuses[event.type];
  if (!selectionPinned && event.type === "prompt_fired") selectedId = item.id;
}

function renderFocus() {
  const title = document.getElementById("focus-title");
  const description = document.getElementById("focus-description");
  const label = document.getElementById("focus-label");
  const nextText = document.getElementById("next-event");
  if (!title) return;
  const item = selectedRoutine();
  const next = nextRoutine();
  const labels = { active: "A gentle reminder", done: "Reminder received", missed: "Your reminder is still here", pending: next?.id === item.id ? "Coming up next" : "In your routine" };
  const descriptions = {
    active: "Your wristband has sent this reminder. Ask your caregiver if you would like help.",
    done: "Your wristband registered a shake. I’ll keep your routine here for you.",
    missed: "No shake was recorded for this reminder. Your caregiver can help if you need it.",
    pending: item.description
  };
  const newTitle = item.label + " at " + item.time;
  const newDescription = descriptions[item.status];
  const newNext = next && next.id !== item.id ? "Next scheduled: " + next.label + " at " + next.time + "." :
    !routineTime ? "I’m waiting for the time from your routine." :
    !next ? "There are no later reminders scheduled today." : "";
  if (title.textContent !== newTitle || description.textContent !== newDescription || nextText.textContent !== newNext) {
    document.dispatchEvent(new CustomEvent("routine-message-change"));
    label.textContent = labels[item.status];
    title.textContent = newTitle;
    description.textContent = newDescription;
    nextText.textContent = newNext;
  } else label.textContent = labels[item.status];
}

function resetChecklist() {
  ROUTINE_ITEMS.forEach((item) => { item.status = "pending"; });
  selectedId = null;
  selectionPinned = false;
  routineTime = null;
}

function routineLabel(id) { return ROUTINE_ITEMS.find((item) => item.id === id)?.label || "Routine"; }

export { renderChecklist, applyChecklistEvent, renderFocus, resetChecklist, routineLabel,
  routineItems, routineIcon, routineStatus, selectedRoutine, selectRoutine, setRoutineTime };
