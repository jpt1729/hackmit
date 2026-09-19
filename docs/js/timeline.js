function renderTimeline(container) {
  if (!container) return;

  const segments = [
    { label: "Sleep", mood: "sleep" },
    { label: "Rest", mood: "rest" },
    { label: "Move", mood: "move" },
    { label: "Kitchen", mood: "room" },
    { label: "Bed", mood: "room" },
    { label: "Living", mood: "room" }
  ];

  container.innerHTML = `
    <div class="timeline-strip">
      ${segments.map((segment) => `
        <div class="timeline-segment ${segment.mood}">${segment.label}</div>
      `).join("")}
    </div>
  `;
}

export { renderTimeline };
