const ROUTINE_ITEMS = [
  { id: "meds_9am", label: "Medication", time: "9:00 AM", status: "pending" },
  { id: "breakfast", label: "Breakfast", time: "8:00 AM", status: "pending" },
  { id: "walk", label: "Morning walk", time: "10:00 AM", status: "pending" }
];

function renderChecklist(container) {
  if (!container) return;

  const rows = ROUTINE_ITEMS.map((item) => {
    const classes = ["checklist-item"];
    if (item.status === "done") classes.push("done");
    if (item.status === "missed") classes.push("missed");

    return `
      <div class="${classes.join(" ")}">
        <span>${item.label} <small>(${item.time})</small></span>
        <strong>${item.status === "done" ? "Done" : item.status === "missed" ? "Missed" : "Pending"}</strong>
      </div>
    `;
  }).join("");

  container.innerHTML = rows;
}

function applyChecklistEvent(event) {
  const item = ROUTINE_ITEMS.find((entry) => entry.id === event.detail);
  if (!item) return;

  if (event.type === "prompt_acked") item.status = "done";
  if (event.type === "prompt_missed") item.status = "missed";
}

export { renderChecklist, applyChecklistEvent };
