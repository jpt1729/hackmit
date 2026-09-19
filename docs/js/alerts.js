function renderAlerts(container, events = []) {
  if (!container) return;

  const items = events.length
    ? events.slice(0, 8).map((event) => {
        const type = event.type || "info";
        const danger = ["prompt_missed", "wear_off", "wander", "geofence_exit"];
        const colorClass = danger.includes(type) ? "danger" : "warn";
        return `
          <li class="alert-item ${colorClass}">
            <strong>${type.replace(/_/g, " ")}</strong>
            <div>${event.detail || "—"}</div>
          </li>
        `;
      }).join("")
    : '<li class="alert-item ok"><strong>No alerts</strong><div>System stable.</div></li>';

  container.innerHTML = `<ul class="alert-list">${items}</ul>`;
}

export { renderAlerts };
