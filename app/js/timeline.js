const entries = [];
let latestState = null;

function addTimelineEvent(event) {
  if (["room_change", "wear_on", "wear_off", "wander"].includes(event.type)) {
    entries.unshift(event);
    if (entries.length > 12) entries.pop();
  }
}

function setTimelineState(state) { latestState = state; }
function resetTimeline() { entries.length = 0; latestState = null; }

function renderTimeline(container) {
  if (!container) return;
  const state = latestState || {};
  const room = state.room || "unknown";
  const activity = state.activity || "unknown";
  const worn = state.worn === false ? "Off wrist" : state.worn === true ? "On wrist" : "Unknown";
  const summary = document.createElement("div");
  summary.className = "timeline-summary";
  const activityLabels = { moving: "Moving", resting: "Resting", sleeping: "Sleeping" };
  summary.textContent = `Last reported activity: ${activityLabels[activity] || "Not available"}. Room estimate: ${room.replace(/_/g, " ")}. Wristband: ${worn.toLowerCase()}.`;
  const list = document.createElement("ol");
  list.className = "timeline-list";
  for (const event of entries) {
    const item = document.createElement("li");
    const time = Number.isFinite(Number(event.ts)) ? new Date(Number(event.ts) * 1000).toLocaleTimeString([], { hour: "numeric", minute: "2-digit" }) : "";
    const label = event.type === "room_change" ? `Moved to ${event.detail || "unknown room"}` :
      event.type === "wear_off" ? "Wristband removed" : event.type === "wear_on" ? "Wristband put on" : "Movement during the night";
    item.textContent = `${time}  ${label}`;
    list.append(item);
  }
  if (!entries.length) {
    const item = document.createElement("li");
    item.textContent = "Waiting for activity";
    list.append(item);
  }
  container.replaceChildren(summary, list);
}

export { addTimelineEvent, setTimelineState, renderTimeline, resetTimeline };
