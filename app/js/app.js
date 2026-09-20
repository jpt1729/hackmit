import { bootApi, registerStateListener, registerEventListener } from "./api.js";
import { applyChecklistEvent, resetChecklist, routineLabel, routineStatus, selectedRoutine, selectRoutine, setRoutineTime } from "./checklist.js";
import { renderClock } from "./clock.js";
import { setupMessages } from "./messages.js";
import { addTimelineEvent, setTimelineState, renderTimeline, resetTimeline } from "./timeline.js";
import { renderAlerts, NOTICE_TYPES } from "./alerts.js";
import { setupProgress, recordProgressEvent, renderProgress, resetReplayProgress } from "./progress.js";
import { setupRoutineEditor } from "./routine-editor.js";
import { setupLocationMap } from "./location-map.js";
import { setupDevice } from "./device.js";
import { renderRing } from "./ring.js";
import { setupFall, renderFall } from "./fall.js";
import { setupWatchMessage } from "./watch-message.js";

const timelineRoot = document.getElementById("timeline");
const alertsRoot = document.getElementById("alerts");
const modeBanner = document.getElementById("mode-banner");
const modeDescription = document.getElementById("mode-description");
const sessionDate = document.getElementById("session-date");   // absent in the current layout
const connectionNotice = document.getElementById("connection-notice");
const ringRoot = document.getElementById("routine-ring");
const announcement = document.getElementById("routine-announcement");
const eventLog = [];
let mode = "";
let currentState = null;

function renderRoutine() {
  renderClock(currentState?.ts, mode === "replay");
}

// The notices panel carries live wristband controls, so it has to be redrawn
// when the connection changes, not only when an event arrives.
function showAlerts() {
  renderAlerts(alertsRoot, eventLog, currentState, mode === "live");
}

setupMessages();
setupProgress();
setupDevice();
setupFall(document.getElementById("fall-banner"));
setupWatchMessage(document.getElementById("watch-message-form"));
setupLocationMap();
setupRoutineEditor();
document.addEventListener("routine-change", renderRoutine);
document.getElementById("main").addEventListener("click", (event) => {
  const button = event.target.closest("[data-select-routine]");
  if (!button) return;
  selectRoutine(button.dataset.selectRoutine);
  renderRoutine();
  const selected = selectedRoutine();
  if (!selected) return;
  announcement.textContent = selected.label + " at " + selected.time + ". " + routineStatus(selected) + ".";
});

function updateDate() {
  const date = mode === "replay" ? (currentState?.ts ? new Date(currentState.ts * 1000) : null) : new Date();
  if (!sessionDate) return;
  sessionDate.textContent = date && Number.isFinite(date.getTime())
    ? (mode === "replay" ? "Recorded on " : "") + date.toLocaleDateString([], { weekday: "long", month: "long", day: "numeric", year: "numeric" })
    : "Recorded example";
}

document.addEventListener("mode-change", ({ detail }) => {
  if (detail.mode === "replay" && mode !== "replay") {
    eventLog.length = 0;
    currentState = null;
    resetChecklist();
    resetReplayProgress();
    resetTimeline();
    renderRoutine();
    renderTimeline(timelineRoot);
  }
  mode = detail.mode;
  showAlerts();
  renderProgress(currentState?.ts, mode);
  const copy = {
    live: ["Your wristband is connected.", "Your reminders update here automatically."],
    connecting: ["Connecting to your wristband…", "Waiting for an update. Your caregiver can check the connection."],
    unavailable: ["We could not load your routine.", "Please ask your caregiver to check the connection, then reload this page."]
  };
  const [title, description] = copy[mode] || ["", ""];
  connectionNotice.hidden = mode === "replay";
  // Do not repeatedly announce successful polls.
  if (modeBanner.textContent !== title) {
    modeBanner.textContent = title;
    modeDescription.textContent = description;
  }
  updateDate();
  renderClock(currentState?.ts, mode === "replay");
});

registerStateListener((state) => {
  currentState = { ...state };
  setRoutineTime(state.ts);
  setTimelineState(currentState);
  renderTimeline(timelineRoot);
  if (state.pendingPrompt) {
    applyChecklistEvent({ type: "prompt_fired", detail: state.pendingPrompt });
  }
  showAlerts();
  renderRoutine();
  renderRing(ringRoot, currentState);
  renderFall(currentState);
  renderProgress(currentState.ts, mode);
  updateDate();
});

registerEventListener((event) => {
  recordProgressEvent(event, mode);
  if (NOTICE_TYPES.includes(event.type)) {
    eventLog.unshift(event);
    if (eventLog.length > 50) eventLog.pop();
  }
  // Replay has only an initial snapshot, so apply recorded changes to that snapshot.
  if (mode === "replay" && currentState) {
    currentState.ts = event.ts;
    setRoutineTime(event.ts);
    if (event.type === "room_change") currentState.room = event.detail;
    if (event.type === "wear_on") currentState.worn = true;
    if (event.type === "wear_off") currentState.worn = false;
    if (event.type === "fall_detected") currentState.fallStage = "confirming";
    if (event.type === "fall_alert") currentState.fallStage = "caregiver";
    if (event.type === "fall_ems") currentState.fallStage = "ems";
    if (event.type === "fall_cancelled") currentState.fallStage = "none";
    if (event.type === "prompt_acked") {
      currentState.tasksDone = Math.min((Number(currentState.tasksDone) || 0) + 1,
                                        Number(currentState.tasksTotal) || 0);
    }
    setTimelineState(currentState);
  }
  applyChecklistEvent(event);
  addTimelineEvent(event);
  showAlerts();
  renderRoutine();
  renderRing(ringRoot, currentState);
  renderFall(currentState);
  renderProgress(currentState?.ts, mode);
  updateDate();
  renderTimeline(timelineRoot);
  const promptMessages = {
    prompt_fired: "Reminder sent.",
    prompt_acked: "Reminder received by the wristband.",
    prompt_missed: "Reminder not yet acknowledged."
  };
  if (promptMessages[event.type]) announcement.textContent = routineLabel(event.detail) + ". " + promptMessages[event.type];
});

renderRoutine();
renderRing(ringRoot, null);
renderProgress(null, mode);
renderTimeline(timelineRoot);
showAlerts();
bootApi();

if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("./sw.js").catch((error) => {
      console.warn("Offline app support unavailable:", error);
    });
  });
}
