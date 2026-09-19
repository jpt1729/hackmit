import { bootApi, registerStateListener, registerEventListener } from "./api.js";
import { renderChecklist, applyChecklistEvent } from "./checklist.js";
import { renderTimeline } from "./timeline.js";
import { renderAlerts } from "./alerts.js";

const checklistRoot = document.getElementById("checklist");
const timelineRoot = document.getElementById("timeline");
const alertsRoot = document.getElementById("alerts");
const modeBanner = document.getElementById("mode-banner");

const eventLog = [];

function setModeBanner(mode, deviceHost = "") {
  if (mode === "live") {
    modeBanner.textContent = `Live mode • device ${deviceHost}`;
  } else {
    modeBanner.textContent = "Replay mode • recorded session";
  }
}

document.addEventListener("mode-change", (event) => {
  const { mode, deviceHost } = event.detail;
  setModeBanner(mode, deviceHost || "");
});

registerStateListener((state) => {
  console.log("State update:", state);
});

registerEventListener((event) => {
  eventLog.unshift(event);
  applyChecklistEvent(event);
  renderAlerts(alertsRoot, eventLog);
  renderChecklist(checklistRoot);
});

renderChecklist(checklistRoot);
renderTimeline(timelineRoot);
renderAlerts(alertsRoot, eventLog);

bootApi();
