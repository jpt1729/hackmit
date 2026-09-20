import { deviceCommand, deviceOnline } from "./api.js";

// The fall banner. It is deliberately the loudest thing on the page: if it is
// showing, nothing else on the dashboard matters.
const STAGES = {
  confirming: {
    title: "Possible fall detected",
    description: "The wristband is asking if they are OK. It cancels itself if they shake it.",
    action: "They are OK — clear this"
  },
  caregiver: {
    title: "Fall — no answer from the wristband",
    description: "Nobody cancelled the alert. Check on them now.",
    action: "I have checked — clear this"
  },
  ems: {
    title: "Fall — emergency services called",
    description: "The alert went unanswered long enough to escalate.",
    action: "Clear this alert"
  }
};

let root = null;
let button = null;
let status = null;
let currentStage = "none";

function setStatus(text) {
  if (status) status.textContent = text;
}

async function clearFall() {
  if (!deviceOnline()) return;
  button.disabled = true;
  setStatus("Clearing…");
  try {
    await deviceCommand("/fall/cancel");
    setStatus("Cleared on the wristband.");
  } catch (error) {
    // Say what actually went wrong: a caregiver needs to know the band is
    // still alarming, not just that a button did nothing.
    setStatus(`The alert is still active: ${error.message}`);
    button.disabled = false;
  }
}

function setupFall(container) {
  root = container;
  if (!root) return;
  root.hidden = true;
}

function renderFall(state) {
  if (!root) return;
  const stage = state?.fallStage || "none";
  if (stage === "none") {
    root.hidden = true;
    root.replaceChildren();
    currentStage = stage;
    return;
  }
  // Only rebuild when the stage actually changes, so the button does not lose
  // focus or reset mid-click while the dashboard polls every three seconds.
  if (stage === currentStage && !root.hidden) return;
  currentStage = stage;

  const copy = STAGES[stage] || STAGES.confirming;
  root.hidden = false;
  root.className = `fall-banner fall-${stage}`;

  const title = document.createElement("h2");
  title.textContent = copy.title;
  const description = document.createElement("p");
  description.textContent = copy.description;

  button = document.createElement("button");
  button.className = "button button-primary";
  button.type = "button";
  button.textContent = copy.action;
  button.addEventListener("click", clearFall);
  // Replay has no wristband to tell, so the control is shown but inert.
  button.disabled = !deviceOnline();

  status = document.createElement("p");
  status.className = "fall-status";
  status.setAttribute("role", "status");
  if (!deviceOnline()) setStatus("This is a recorded example — no wristband is connected.");

  root.replaceChildren(title, description, button, status);
}

export { setupFall, renderFall };
