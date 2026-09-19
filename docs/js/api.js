const stateListeners = [];
const eventListeners = [];

let currentMode = "replay";
let lastEventId = 0;
let replayIndex = 0;
let replayTimer = null;

function registerStateListener(listener) {
  stateListeners.push(listener);
}

function registerEventListener(listener) {
  eventListeners.push(listener);
}

function notifyState(state) {
  stateListeners.forEach((listener) => listener(state));
}

function notifyEvent(event) {
  eventListeners.forEach((listener) => listener(event));
}

async function fetchJson(url) {
  const response = await fetch(url, { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Request failed: ${response.status}`);
  }
  return response.json();
}

function startReplayMode() {
  currentMode = "replay";
  document.dispatchEvent(new CustomEvent("mode-change", { detail: { mode: "replay" } }));

  fetchJson("data/demo.json")
    .then((payload) => {
      const events = Array.isArray(payload.events) ? payload.events : [];
      const state = payload.state || {};
      if (state && Object.keys(state).length) {
        notifyState(state);
      }

      const playback = () => {
        if (replayIndex >= events.length) {
          return;
        }
        const event = events[replayIndex];
        replayIndex += 1;
        notifyEvent(event);
        if (event.id) {
          lastEventId = Math.max(lastEventId, Number(event.id));
        }
        replayTimer = window.setTimeout(playback, 1800);
      };

      replayTimer = window.setTimeout(playback, 1000);
    })
    .catch((error) => {
      console.warn("Replay mode failed to load demo data:", error);
    });
}

async function pollLiveMode(deviceHost) {
  currentMode = "live";
  document.dispatchEvent(new CustomEvent("mode-change", { detail: { mode: "live", deviceHost } }));

  try {
    const state = await fetchJson(`http://${deviceHost}/state`);
    notifyState(state);

    const eventsPayload = await fetchJson(`http://${deviceHost}/events?since=${lastEventId}`);
    const events = Array.isArray(eventsPayload.events) ? eventsPayload.events : [];
    events.forEach((event) => {
      notifyEvent(event);
      if (event.id) {
        lastEventId = Math.max(lastEventId, Number(event.id));
      }
    });
  } catch (error) {
    console.warn("Live mode failed, falling back to replay:", error);
    startReplayMode();
  }
}

function bootApi() {
  const params = new URLSearchParams(window.location.search);
  const deviceHost = params.get("device");

  if (deviceHost) {
    pollLiveMode(deviceHost);
    window.setInterval(() => pollLiveMode(deviceHost), 3000);
  } else {
    startReplayMode();
  }
}

export { bootApi, registerStateListener, registerEventListener };
