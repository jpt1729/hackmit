import { deviceCommand, deviceOnline } from "./api.js";
import { routineItems } from "./checklist.js";

// The routine editor used to be a list that talked to nobody: activities were
// saved in this browser while the wristband buzzed for whatever was compiled
// into its firmware. This module closes that loop. Everything a caregiver
// changes here is pushed to the band, and the two safety controls that only
// ever existed on the device - acknowledge a reminder, stop the away-from-home
// chime - can now be reached from the dashboard.

// Kept in step with firmware/src/schedule.h.
const DEVICE_MAX_ITEMS = 24;
const DEVICE_LABEL_MAX = 47;

let syncTimer = null;
let pushedFingerprint = "";
let pushing = false;

const status = () => document.getElementById("routine-sync-status");
const pushButton = () => document.getElementById("routine-sync-now");

function setStatus(message, kind = "") {
  const node = status();
  if (!node) return;
  node.textContent = message;
  node.dataset.kind = kind;
}

// The band stores hour and minute; icons and descriptions stay on the
// dashboard, because a 128x64 OLED has room for a label and nothing else.
function schedulePayload() {
  const items = [...routineItems()].sort((a, b) => a.minutes - b.minutes);
  const sent = items.slice(0, DEVICE_MAX_ITEMS).map((item) => ({
    id: item.id,
    label: item.label.slice(0, DEVICE_LABEL_MAX),
    hour: Math.floor(item.minutes / 60),
    minute: item.minutes % 60
  }));
  return { sent, dropped: items.slice(DEVICE_MAX_ITEMS) };
}

function fingerprint(items) {
  return JSON.stringify(items);
}

async function pushRoutine({ manual = false } = {}) {
  if (!deviceOnline()) {
    if (manual) setStatus("The wristband is not connected, so the routine stays on this device for now.", "warn");
    return false;
  }
  const { sent, dropped } = schedulePayload();
  const stamp = fingerprint(sent);
  if (!manual && stamp === pushedFingerprint) return true;
  if (pushing) return false;

  pushing = true;
  if (pushButton()) pushButton().disabled = true;
  setStatus("Sending the routine to the wristband…");
  try {
    const result = await deviceCommand("/schedule", { body: { items: sent } });
    pushedFingerprint = stamp;
    const count = Number(result.count ?? sent.length);
    // Saying nothing about the ones that did not fit would leave a caregiver
    // believing the band will remind about an activity it has never heard of.
    setStatus(dropped.length
      ? `${count} reminder${count === 1 ? "" : "s"} are on the wristband. It holds ${DEVICE_MAX_ITEMS}, `
        + `so these are not on it: ${dropped.map((item) => item.label).join(", ")}.`
      : `${count} reminder${count === 1 ? "" : "s"} are on the wristband.`,
      dropped.length ? "warn" : "ok");
    document.dispatchEvent(new CustomEvent("device-routine-pushed", { detail: { count } }));
    return true;
  } catch (error) {
    setStatus(`The wristband kept its previous routine: ${error.message}`, "warn");
    return false;
  } finally {
    pushing = false;
    if (pushButton()) pushButton().disabled = !deviceOnline();
  }
}

// Tick a reminder off from the dashboard. The band normally waits for a shake;
// a caregiver standing in the room can see it was done and say so here.
async function ackPending() {
  await deviceCommand("/ack");
}

// Stop the away-from-home chime on the wrist. The alert deliberately stays on
// the dashboard: silencing the buzzer is not the same as the person being home.
async function silenceAway() {
  await deviceCommand("/silence");
}

function setupDevice() {
  pushButton()?.addEventListener("click", () => pushRoutine({ manual: true }));

  // A caregiver dragging times around should not fire a write per keystroke.
  document.addEventListener("routine-change", () => {
    window.clearTimeout(syncTimer);
    syncTimer = window.setTimeout(() => pushRoutine(), 800);
  });

  document.addEventListener("mode-change", ({ detail }) => {
    const online = detail.mode === "live";
    if (pushButton()) pushButton().disabled = !online;
    if (!online) {
      // A different band, or the same one rebooted, needs telling again.
      pushedFingerprint = "";
      if (detail.mode === "replay") setStatus("This is a recorded demo, so nothing is sent to a wristband.");
      else setStatus("Waiting for the wristband before sending the routine.");
      return;
    }
    pushRoutine();
  });
}

export { setupDevice, pushRoutine, ackPending, silenceAway };
