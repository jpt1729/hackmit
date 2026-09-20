// The dashboard half of the LED ring. The wristband lights one pixel per
// routine item; this draws the same twelve segments so the caregiver can see
// what the wearer is looking at without being in the room.
const SEGMENTS = 12;

// Must match ledsProgressPixels() in firmware/src/leds.cpp: never round a
// started day down to empty, never round an unfinished one up to full.
function progressSegments(done, total) {
  if (!total) return 0;
  if (done >= total) return SEGMENTS;
  let px = Math.round((done * SEGMENTS) / total);
  if (done > 0 && px === 0) px = 1;
  if (px >= SEGMENTS) px = SEGMENTS - 1;
  return px;
}

function renderRing(container, state) {
  if (!container) return;
  const total = Number(state?.tasksTotal) || 0;
  const done = Math.min(Number(state?.tasksDone) || 0, total);
  const lit = progressSegments(done, total);
  const complete = total > 0 && done >= total;

  const figure = document.createElement("div");
  figure.className = "ring" + (complete ? " ring-complete" : "");

  const dial = document.createElement("div");
  dial.className = "ring-dial";
  for (let i = 0; i < SEGMENTS; i++) {
    const pixel = document.createElement("i");
    pixel.className = "ring-pixel" + (i < lit ? " lit" : "");
    // Twelve pixels around the circle, first one at the top.
    pixel.style.transform = `rotate(${(i * 360) / SEGMENTS}deg) translateY(-42px)`;
    dial.append(pixel);
  }

  const centre = document.createElement("div");
  centre.className = "ring-centre";
  const count = document.createElement("strong");
  count.textContent = total ? `${done}/${total}` : "—";
  const label = document.createElement("span");
  label.textContent = total ? "done" : "no routine";
  centre.append(count, label);
  dial.append(centre);

  const caption = document.createElement("p");
  caption.className = "ring-caption";
  caption.textContent = !total
    ? "The wristband has no routine loaded."
    : complete
      ? "Everything on the routine is done. The ring is fully green."
      : `${done} of ${total} reminders acknowledged. ${lit} of ${SEGMENTS} pixels are lit.`;

  figure.append(dial);
  container.replaceChildren(figure, caption);
}

export { renderRing, progressSegments, SEGMENTS };
