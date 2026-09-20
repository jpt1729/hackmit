const stateListeners = [];
const eventListeners = [];
let lastEventId = 0;
let failures = 0;
let polling = false;
let replayStarted = false;
let replayTimer = null;
let replayPaused = false;
let replayEvents = [];
let replayIndex = 0;
let liveHost = "";
let lastClockSyncMs = 0;
let clockSyncInFlight = false;

function replayStatus(complete = false) {
  document.dispatchEvent(new CustomEvent("replay-status", { detail: { paused: replayPaused, complete } }));
}

function playNextEvent() {
  if (replayPaused || replayIndex >= replayEvents.length) return;
  notifyEvent(replayEvents[replayIndex++]);
  if (replayIndex < replayEvents.length) replayTimer = window.setTimeout(playNextEvent, 7000);
  else replayStatus(true);
}

function toggleReplay() {
  if (!replayStarted || replayIndex >= replayEvents.length) return;
  replayPaused = !replayPaused;
  window.clearTimeout(replayTimer);
  if (!replayPaused) replayTimer = window.setTimeout(playNextEvent, 7000);
  replayStatus();
}

function registerStateListener(listener) { stateListeners.push(listener); }
function registerEventListener(listener) { eventListeners.push(listener); }
function notifyState(state) { stateListeners.forEach((listener) => listener(state)); }
function notifyEvent(event) { eventListeners.forEach((listener) => listener(event)); }
function setMode(mode, deviceHost = "") {
  document.dispatchEvent(new CustomEvent("mode-change", { detail: { mode, deviceHost } }));
}

async function fetchJson(url) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) throw new Error(`Request failed: ${response.status}`);
  return response.json();
}

// The write half of the device contract. Everything above this line reads the
// wristband; these are the four things the dashboard can ask it to do.
function deviceOnline() { return Boolean(liveHost); }

async function deviceCommand(path, { params, body } = {}) {
  if (!liveHost) throw new Error("The wristband is not connected.");
  const query = params ? `?${new URLSearchParams(params)}` : "";
  const response = await fetch(`http://${liveHost}${path}${query}`, {
    method: "POST",
    cache: "no-store",
    ...(body === undefined ? {} : { headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) })
  });
  // The firmware answers with JSON on every path, including its errors.
  const payload = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(payload.error || `The wristband could not do that (${response.status}).`);
  return payload;
}

async function deviceSchedule() {
  if (!liveHost) throw new Error("The wristband is not connected.");
  return fetchJson(`http://${liveHost}/schedule`);
}

// The ESP32 has no battery-backed clock. Until something hands it the time its
// timeValid() gate stays shut and not one scheduled reminder fires - the band
// looks alive and quietly does nothing. The dashboard is the only thing on the
// network that reliably knows what time it is, so it is the one that tells it.
async function syncClock(state) {
  const now = Math.floor(Date.now() / 1000);
  const deviceTs = Number(state?.ts);
  const clockLooksRight = deviceTs > 1700000000 && Math.abs(deviceTs - now) < 120;
  if (clockLooksRight || clockSyncInFlight) return;
  // A device that keeps refusing should not be asked every three seconds.
  if (lastClockSyncMs && Date.now() - lastClockSyncMs < 60000) return;
  clockSyncInFlight = true;
  lastClockSyncMs = Date.now();
  try {
    await deviceCommand("/time", { params: { epoch: Math.floor(Date.now() / 1000) } });
    document.dispatchEvent(new CustomEvent("device-clock-set", { detail: { drift: deviceTs - now } }));
  } catch (error) {
    console.warn("Could not set the wristband clock:", error);
  } finally {
    clockSyncInFlight = false;
  }
}

async function startReplayMode() {
  if (replayStarted) return;
  replayStarted = true;
  liveHost = "";              // recorded data takes no commands
  setMode("replay");
  try {
    const payload = await fetchJson("data/demo.json");
    notifyState(payload.state || {});
    replayEvents = Array.isArray(payload.events) ? payload.events : [];
    replayStatus(replayEvents.length === 0);
    if (!replayPaused) replayTimer = window.setTimeout(playNextEvent, 7000);
  } catch (error) {
    console.warn("Replay data unavailable:", error);
    setMode("unavailable");
  }
}

async function pollLiveMode(deviceHost) {
  if (polling || replayStarted) return;
  polling = true;
  try {
    const state = await fetchJson(`http://${deviceHost}/state`);
    liveHost = deviceHost;    // reachable: commands may be sent from here on
    const payload = await fetchJson(`http://${deviceHost}/events?since=${lastEventId}`);
    const events = Array.isArray(payload.events) ? payload.events : [];
    notifyState(state);
    events.forEach((event) => {
      if (Number(event.id) > lastEventId) {
        lastEventId = Number(event.id);
        notifyEvent(event);
      }
    });
    failures = 0;
    setMode("live", deviceHost);
    await syncClock(state);
  } catch (error) {
    failures += 1;
    console.warn("Device poll failed:", error);
    if (failures >= 3) {
      liveHost = "";
      startReplayMode();
    } else setMode("connecting", deviceHost);
  } finally {
    polling = false;
  }
}

function bootApi() {
  const deviceHost = new URLSearchParams(window.location.search).get("device");
  if (!deviceHost) return startReplayMode();
  if (!/^[a-z\d.-]+(?::\d{1,5})?$/i.test(deviceHost)) {
    setMode("unavailable");
    return;
  }
  setMode("connecting", deviceHost);
  pollLiveMode(deviceHost);
  window.setInterval(() => pollLiveMode(deviceHost), 3000);
}

export { bootApi, registerStateListener, registerEventListener, toggleReplay,
  deviceCommand, deviceSchedule, deviceOnline };
