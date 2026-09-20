import { addRoutine, updateRoutine, removeRoutine, routineItems, routineIcon, routineVisualState } from "./checklist.js";

const form = document.getElementById("routine-form");
const list = document.getElementById("routine-editor-list");
const status = document.getElementById("routine-editor-status");
const nameInput = document.getElementById("routine-name");
const timeInput = document.getElementById("routine-time");
const endTimeInput = document.getElementById("routine-end-time");
const iconInput = document.getElementById("routine-icon");
const notesInput = document.getElementById("routine-notes");
const completionInput = document.getElementById("routine-completion");
const editDialog = document.getElementById("routine-edit-dialog");
const editForm = document.getElementById("routine-edit-form");
const editName = document.getElementById("routine-edit-name");
const editTime = document.getElementById("routine-edit-time");
const editEndTime = document.getElementById("routine-edit-end-time");
const editIcon = document.getElementById("routine-edit-icon");
const editNotes = document.getElementById("routine-edit-notes");
const editStatus = document.getElementById("routine-edit-status");
const editCompletion = document.getElementById("routine-edit-completion");
let completionChanged = false;
editCompletion.addEventListener("change", () => { completionChanged = true; });
let editingId = null;
let editOrigin = null;

function openRoutineEditor(id) {
  const item = routineItems().find((entry) => entry.id === id);
  if (!item) return;
  editingId = id;
  editOrigin = document.activeElement;
  editName.value = item.label;
  editTime.value = `${String(Math.floor(item.minutes / 60)).padStart(2, "0")}:${String(item.minutes % 60).padStart(2, "0")}`;
  const end = item.endMinutes % 1440;
  editEndTime.value = `${String(Math.floor(end / 60)).padStart(2, "0")}:${String(end % 60).padStart(2, "0")}`;
  editIcon.value = item.icon;
  editNotes.value = item.notes || "";
  const state = routineVisualState(item);
  editCompletion.value = ["done", "missed", "pending"].includes(state) ? state : "pending";
  completionChanged = false;
  editStatus.textContent = "";
  editDialog.showModal();
  editName.focus();
}

function renderEditor() {
  const previous = new Map([...list.children].map((row) => [row.dataset.routineId, row.getBoundingClientRect()]));
  list.replaceChildren();
  for (const item of routineItems()) {
    const row = document.createElement("li");
    row.dataset.routineId = item.id;
    const icon = document.createElement("span");
    icon.className = "routine-editor-icon";
    icon.innerHTML = routineIcon(item);
    const details = document.createElement("div");
    const label = document.createElement("strong");
    label.textContent = item.label;
    const time = document.createElement("span");
    time.textContent = item.time;
    details.append(label, time);
    if (item.notes) {
      const notes = document.createElement("span");
      notes.className = "routine-item-notes";
      notes.textContent = item.notes;
      details.append(notes);
    }
    const actions = document.createElement("span");
    actions.className = "routine-row-actions";
    const edit = document.createElement("button");
    edit.type = "button";
    edit.className = "button routine-edit";
    edit.textContent = "Edit";
    edit.dataset.editRoutine = item.id;
    edit.setAttribute("aria-label", `Edit ${item.label} at ${item.time}`);
    const remove = document.createElement("button");
    remove.type = "button";
    remove.className = "button routine-remove";
    remove.textContent = "Remove";
    remove.dataset.removeRoutine = item.id;
    remove.setAttribute("aria-label", `Remove ${item.label} at ${item.time}`);
    actions.append(edit, remove);
    row.append(icon, details, actions);
    list.append(row);
  }
  if (previous.size && !window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
    for (const row of list.children) {
      const bounds = row.getBoundingClientRect();
      if (!bounds.height) continue;
      const old = previous.get(row.dataset.routineId);
      row.animate([
        { opacity: 0.4, transform: `translateY(${old ? old.top - bounds.top : 8}px)` },
        { opacity: 1, transform: "translateY(0)" }
      ], { duration: 260, easing: "ease-out" });
    }
  }
}

function setupRoutineEditor() {
  editIcon.innerHTML = iconInput.innerHTML;
  document.getElementById("routine-edit-cancel").addEventListener("click", () => editDialog.close());
  editDialog.addEventListener("close", () => {
    const sourceAttribute = editOrigin?.hasAttribute("data-select-routine") ? "data-select-routine" : "data-edit-routine";
    const target = editOrigin?.isConnected ? editOrigin : document.querySelector(`[${sourceAttribute}="${editingId}"]`);
    (target || nameInput).focus({ preventScroll: true });
    editingId = null;
  });
  editForm.addEventListener("submit", (event) => {
    event.preventDefault();
    try {
      const { item, saved } = updateRoutine(editingId, editName.value, editTime.value, editIcon.value, editNotes.value, completionChanged ? editCompletion.value : undefined, editEndTime.value);
      status.textContent = `${item.label} updated to ${item.time}.${saved ? " Saved in this browser." : " Updated for this visit; browser storage is unavailable."}`;
      editDialog.close();
    } catch (error) { editStatus.textContent = error.message; }
  });
  renderEditor();
  document.addEventListener("routine-change", renderEditor);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    try {
      const { item, saved } = addRoutine(nameInput.value, timeInput.value, iconInput.value, notesInput.value, completionInput.value, endTimeInput.value);
      status.textContent = `${item.label} added at ${item.time}.${saved ? " Saved in this browser." : " Added for this visit; browser storage is unavailable."}`;
      form.reset();
      nameInput.focus();
    } catch (error) { status.textContent = error.message; }
  });
  list.addEventListener("click", (event) => {
    const edit = event.target.closest("[data-edit-routine]");
    if (edit) { openRoutineEditor(edit.dataset.editRoutine); return; }
    const button = event.target.closest("[data-remove-routine]");
    if (!button) return;
    const buttons = [...list.querySelectorAll("button")];
    const index = buttons.indexOf(button);
    const result = removeRoutine(button.dataset.removeRoutine);
    if (!result) return;
    status.textContent = `${result.item.label} removed.${result.saved ? "" : " Removed for this visit; browser storage is unavailable."}`;
    const remaining = list.querySelectorAll("button");
    (remaining[Math.min(index, remaining.length - 1)] || nameInput).focus();
  });
}

export { setupRoutineEditor, openRoutineEditor };
