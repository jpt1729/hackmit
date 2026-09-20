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

async function startReplayMode() {
  if (replayStarted) return;
  replayStarted = true;
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
  } catch (error) {
    failures += 1;
    console.warn("Device poll failed:", error);
    if (failures >= 3) startReplayMode();
    else setMode("connecting", deviceHost);
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

export { bootApi, registerStateListener, registerEventListener, toggleReplay };
