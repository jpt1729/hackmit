import { routineLabel } from "./checklist.js";

function renderAlerts(container, events = []) {
  if (!container) return;
  const list = document.createElement("ul");
  list.className = "alert-list";
  const notices = events.filter((event) => ["wander", "wear_off", "prompt_missed"].includes(event.type));
  for (const event of notices.slice(0, 8)) {
    const item = document.createElement("li");
    item.className = "alert-item";
    const title = document.createElement("strong");
    const description = document.createElement("p");
    const copy = {
      wander: ["Movement during the night", "The wristband detected movement during sleeping hours."],
      wear_off: ["Wristband removed", "The wristband recorded a loss of skin contact."],
      prompt_missed: ["Reminder not acknowledged", routineLabel(event.detail) + ": no shake was recorded in the response window."]
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
    item.textContent = "No updates have been received.";
    list.append(item);
  }
  container.replaceChildren(list);
}
export { renderAlerts };
