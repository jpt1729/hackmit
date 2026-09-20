import { addRoutine, removeRoutine, routineItems, routineIcon } from "./checklist.js";

const form = document.getElementById("routine-form");
const list = document.getElementById("routine-editor-list");
const status = document.getElementById("routine-editor-status");
const nameInput = document.getElementById("routine-name");
const timeInput = document.getElementById("routine-time");
const iconInput = document.getElementById("routine-icon");

function renderEditor() {
  list.replaceChildren();
  for (const item of routineItems()) {
    const row = document.createElement("li");
    const icon = document.createElement("span");
    icon.className = "routine-editor-icon";
    icon.innerHTML = routineIcon(item);
    const details = document.createElement("div");
    const label = document.createElement("strong");
    label.textContent = item.label;
    const time = document.createElement("span");
    time.textContent = item.time;
    details.append(label, time);
    const remove = document.createElement("button");
    remove.type = "button";
    remove.className = "button routine-remove";
    remove.textContent = "Remove";
    remove.dataset.removeRoutine = item.id;
    remove.setAttribute("aria-label", `Remove ${item.label} at ${item.time}`);
    row.append(icon, details, remove);
    list.append(row);
  }
}

function setupRoutineEditor() {
  renderEditor();
  document.addEventListener("routine-change", renderEditor);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    try {
      const { item, saved } = addRoutine(nameInput.value, timeInput.value, iconInput.value);
      status.textContent = `${item.label} added at ${item.time}.${saved ? " Saved in this browser." : " Added for this visit; browser storage is unavailable."}`;
      form.reset();
      nameInput.focus();
    } catch (error) { status.textContent = error.message; }
  });
  list.addEventListener("click", (event) => {
    const button = event.target.closest("[data-remove-routine]");
    if (!button) return;
    const buttons = [...list.querySelectorAll("button")];
    const index = buttons.indexOf(button);
    const result = removeRoutine(button.dataset.removeRoutine);
    if (!result) return;
    status.textContent = `${result.item.label} removed.${result.saved ? " Saved in this browser." : " Removed for this visit; browser storage is unavailable."}`;
    const remaining = list.querySelectorAll("button");
    (remaining[Math.min(index, remaining.length - 1)] || nameInput).focus();
  });
}

export { setupRoutineEditor };
