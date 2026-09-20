import { routineLabel } from "./checklist.js";

// Fall notices come first because they are the only ones that can end with an
// ambulance; the rest are ordered by how quickly a caregiver needs to act.
const NOTICE_TYPES = ["fall_ems", "fall_alert", "fall_detected", "fall_cancelled",
                      "wander", "wear_off", "prompt_missed"];
const SERIOUS = ["fall_ems", "fall_alert", "fall_detected"];

function renderAlerts(container, events = []) {
  if (!container) return;
  const list = document.createElement("ul");
  list.className = "alert-list";
  const notices = events.filter((event) => NOTICE_TYPES.includes(event.type));
  for (const event of notices.slice(0, 8)) {
    const item = document.createElement("li");
    item.className = "alert-item" + (SERIOUS.includes(event.type) ? " alert-serious" : "");
    const title = document.createElement("strong");
    const description = document.createElement("p");
    const copy = {
      wander: ["Movement during the night", "The wristband detected movement during sleeping hours."],
      wear_off: ["Wristband removed", "The wristband recorded a loss of skin contact."],
      prompt_missed: ["Reminder not acknowledged", routineLabel(event.detail) + ": no shake was recorded in the response window."],
      fall_detected: ["Possible fall", "The wristband saw a fall and asked if they were OK."],
      fall_alert: ["Fall — no answer", "Nobody cancelled the alert on the wristband."],
      fall_ems: ["Fall — emergency services called", "The alert went unanswered and was escalated."],
      fall_cancelled: ["Fall alert cleared", "They confirmed they were OK, so nobody was called."]
    };
    [title.textContent, description.textContent] = copy[event.type];
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
  if (!notices.length) {
    const item = document.createElement("li");
    item.className = "alert-item ok";
    item.textContent = "No notices have been received.";
    list.append(item);
  }
  container.replaceChildren(list);
}
export { renderAlerts };
