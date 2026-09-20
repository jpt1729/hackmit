import { routineLabel } from "./checklist.js";
import { ackPending, silenceAway } from "./device.js";

const NOTICE_TYPES = ["fall_detected", "fall_alert", "fall_ems", "fall_cancelled",
                      "wander", "wear_off", "prompt_missed", "geofence_exit"];
// Notices that can end with an ambulance get the heavier treatment in the list.
const SERIOUS = ["fall_detected", "fall_alert", "fall_ems"];

function noticeCopy(event) {
  return {
    wander: ["Movement during the night", "The wristband detected movement during sleeping hours."],
    wear_off: ["Wristband removed", "The wristband recorded a loss of skin contact."],
    prompt_missed: [
      "Reminder not acknowledged",
      routineLabel(event.detail) + ": no shake was recorded in the response window."
    ],
    // The band chimes and points the way home on its own; this is the half of
    // that the caregiver needs to see, and it used to go nowhere.
    // The live banner covers a fall that is happening now; these are the
    // record of one, which is what a caregiver reads afterwards.
    fall_detected: ["Possible fall", "The wristband saw a fall and asked if they were OK."],
    fall_alert: ["Fall — no answer", "Nobody cancelled the alert on the wristband."],
    fall_ems: ["Fall — emergency services called", "The alert went unanswered and was escalated."],
    fall_cancelled: ["Fall alert cleared", "They confirmed they were OK, so nobody was called."],
    geofence_exit: [
      "Left the safe area",
      event.detail
        ? `The wristband was ${event.detail} from home and chimed a reminder of the way back.`
        : "The wristband left the safe area around home and chimed a reminder of the way back."
    ]
  }[event.type];
}

// A button that reaches the wristband, with the answer shown where it was
// pressed: a caregiver needs to know whether the band actually heard them.
function commandButton(label, busyLabel, run, onDone) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "button button-small alert-action";
  button.textContent = label;
  button.addEventListener("click", async () => {
    const original = button.textContent;
    button.disabled = true;
    button.textContent = busyLabel;
    try {
      await run();
      onDone?.(null);
    } catch (error) {
      onDone?.(error);
      button.disabled = false;
      button.textContent = original;
    }
  });
  return button;
}

function actionCard(title, description, button) {
  const item = document.createElement("li");
  item.className = "alert-item alert-action-card";
  const heading = document.createElement("strong");
  heading.textContent = title;
  const body = document.createElement("p");
  body.textContent = description;
  const result = document.createElement("p");
  result.className = "alert-action-result";
  result.setAttribute("role", "status");
  item.append(heading, body, button, result);
  return { item, result };
}

function renderAlerts(container, events = [], state = null, online = false) {
  if (!container) return;
  const list = document.createElement("ul");
  list.className = "alert-list";

  // Live controls first: these are things to do, not things that happened.
  if (online && state?.awayFromHome) {
    const distance = Number(state.gps?.distanceHomeM);
    const { item, result } = actionCard(
      "Away from home right now",
      Number.isFinite(distance)
        ? `The wristband is about ${Math.round(distance)} m from home and is chiming.`
        : "The wristband is outside the safe area and is chiming.",
      commandButton("Silence the chime", "Silencing…", silenceAway, (error) => {
        result.textContent = error
          ? `The chime is still sounding: ${error.message}`
          : "The chime is off. This notice stays until the wristband is home again.";
      }));
    list.append(item);
  }

  if (online && state?.pendingPrompt) {
    const { item, result } = actionCard(
      "Waiting to be acknowledged",
      `${routineLabel(state.pendingPrompt)}: the wristband is waiting for a shake.`,
      commandButton("Mark as done", "Sending…", ackPending, (error) => {
        result.textContent = error
          ? `The reminder is still waiting: ${error.message}`
          : "Marked as done on the wristband.";
      }));
    list.append(item);
  }

  const notices = events.filter((event) => NOTICE_TYPES.includes(event.type));
  for (const event of notices.slice(0, 8)) {
    const item = document.createElement("li");
    item.className = "alert-item" + (SERIOUS.includes(event.type) ? " alert-serious" : "");
    const title = document.createElement("strong");
    const description = document.createElement("p");
    [title.textContent, description.textContent] = noticeCopy(event);
    item.append(title, description);
    const date = new Date(Number(event.ts) * 1000);
    if (Number.isFinite(date.getTime())) {
      const time = document.createElement("time");
      time.dateTime = date.toISOString();
      time.textContent = date.toLocaleString([], { month: "short", day: "numeric", hour: "numeric", minute: "2-digit" });
      item.append(time);
    }
    list.append(item);
  }

  if (!list.children.length) {
    const item = document.createElement("li");
    item.className = "alert-item ok";
    item.textContent = "No updates have been received.";
    list.append(item);
  }
  container.replaceChildren(list);
}

export { renderAlerts, NOTICE_TYPES };
