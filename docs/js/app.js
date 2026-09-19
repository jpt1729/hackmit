import { bootApi, registerStateListener, registerEventListener, toggleReplay } from "./api.js";
import { renderChecklist, applyChecklistEvent, renderFocus, resetChecklist, routineLabel, selectRoutine, setRoutineTime } from "./checklist.js";
import { renderClock } from "./clock.js";
import { setupCompanion } from "./companion.js";
import { addTimelineEvent, setTimelineState, renderTimeline, resetTimeline } from "./timeline.js";
import { renderAlerts } from "./alerts.js";

const checklistRoot = document.getElementById("checklist");
const timelineRoot = document.getElementById("timeline");
const alertsRoot = document.getElementById("alerts");
const modeBanner = document.getElementById("mode-banner");
const modeDescription = document.getElementById("mode-description");
const sessionDate = document.getElementById("session-date");
const replayToggle = document.getElementById("replay-toggle");
const textSize = document.getElementById("text-size");
const themeToggle = document.getElementById("theme-toggle");
const announcement = document.getElementById("routine-announcement");
const eventLog = [];
let mode = "";
let currentState = null;

function setTheme(dark) {
  document.documentElement.dataset.theme = dark ? "dark" : "light";
  themeToggle.setAttribute("aria-pressed", String(dark));
  document.querySelector('meta[name="theme-color"]').content = dark ? "#171922" : "#f4f3f8";
}
setTheme(document.documentElement.dataset.theme !== "light");
themeToggle.addEventListener("click", () => {
  const dark = themeToggle.getAttribute("aria-pressed") !== "true";
  setTheme(dark);
  try { localStorage.setItem("routine-anchor-theme", dark ? "dark" : "light"); } catch { /* Optional preference storage. */ }
});

function renderRoutine() {
  renderChecklist(checklistRoot);
  renderClock(currentState?.ts, mode === "replay");
  renderFocus();
}

setupCompanion();
document.getElementById("main").addEventListener("click", (event) => {
  const button = event.target.closest("[data-select-routine]");
  if (!button) return;
  selectRoutine(button.dataset.selectRoutine);
  renderRoutine();
  announcement.textContent = document.getElementById("focus-title").textContent + ". " + document.getElementById("focus-description").textContent;
});

function setTextSize(large) {
  document.body.classList.toggle("large-text", large);
  textSize.setAttribute("aria-pressed", String(large));
}
try { setTextSize(localStorage.getItem("routine-anchor-large-text") === "true"); } catch { /* Optional preference storage. */ }
textSize.addEventListener("click", () => {
  const large = textSize.getAttribute("aria-pressed") !== "true";
  setTextSize(large);
  try { localStorage.setItem("routine-anchor-large-text", String(large)); } catch { /* Continue without storage. */ }
});
replayToggle.addEventListener("click", toggleReplay);
document.addEventListener("replay-status", ({ detail }) => {
  replayToggle.disabled = detail.complete;
  replayToggle.textContent = detail.complete ? "Demo finished" : detail.paused ? "Continue demo" : "Pause demo";
  if (detail.complete) modeDescription.textContent = "The recorded example has finished. These are not live reminders.";
  else modeDescription.textContent = detail.paused
    ? "The example is paused. Select Continue demo when you are ready."
    : "This is a recorded example, not your live routine.";
});

function updateDate() {
  const date = mode === "replay" ? (currentState?.ts ? new Date(currentState.ts * 1000) : null) : new Date();
  sessionDate.textContent = date && Number.isFinite(date.getTime())
    ? (mode === "replay" ? "Recorded on " : "") + date.toLocaleDateString([], { weekday: "long", month: "long", day: "numeric", year: "numeric" })
    : "Recorded example";
}

document.addEventListener("mode-change", ({ detail }) => {
  if (detail.mode === "replay" && mode !== "replay") {
    eventLog.length = 0;
    currentState = null;
    resetChecklist();
    resetTimeline();
    renderRoutine();
    renderTimeline(timelineRoot);
    renderAlerts(alertsRoot, eventLog);
  }
  mode = detail.mode;
  const copy = {
    live: ["Your wristband is connected.", "Your reminders update here automatically."],
    connecting: ["Connecting to your wristband…", "Waiting for an update. Your caregiver can check the connection."],
    unavailable: ["We could not load your routine.", "Please ask your caregiver to check the connection, then reload this page."],
    replay: ["You are viewing a demo.", "This is a recorded example, not your live routine."]
  };
  const [title, description] = copy[mode];
  // Do not repeatedly announce successful polls.
  if (modeBanner.textContent !== title) {
    modeBanner.textContent = title;
    modeDescription.textContent = description;
  }
  replayToggle.hidden = mode !== "replay";
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
  renderRoutine();
  updateDate();
});

registerEventListener((event) => {
  if (["wander", "wear_off", "prompt_missed"].includes(event.type)) {
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
    setTimelineState(currentState);
  }
  applyChecklistEvent(event);
  addTimelineEvent(event);
  renderAlerts(alertsRoot, eventLog);
  renderRoutine();
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
renderTimeline(timelineRoot);
renderAlerts(alertsRoot, eventLog);
bootApi();

if ("serviceWorker" in navigator) {
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("./sw.js").catch((error) => {
      console.warn("Offline app support unavailable:", error);
    });
  });
}
