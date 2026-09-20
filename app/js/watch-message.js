import { deviceCommand, deviceOnline } from "./api.js";

// Sends a short note to the OLED on the wrist. The dashboard already records
// voice messages with a transcript; this puts that text where the wearer will
// actually see it.
const MAX_LEN = 96;

function setupWatchMessage(form) {
  if (!form) return;
  const input = form.querySelector("#watch-message-text");
  const button = form.querySelector("#watch-message-send");
  // The count and status lines sit outside the form, so they are looked up on
  // the document rather than within it.
  const counter = document.getElementById("watch-message-count");
  const status = document.getElementById("watch-message-status");
  if (!input || !button || !counter || !status) return;

  // setup runs before the first poll, so connectivity is decided here and then
  // kept in step with the connection - otherwise the box stays disabled for a
  // wristband that is right there.
  const setConnected = (online) => {
    input.disabled = button.disabled = !online;
    if (!online) status.textContent = "Connect a wristband to send a note to its screen.";
    else if (status.textContent.startsWith("Connect a wristband")) status.textContent = "";
  };
  setConnected(deviceOnline());
  document.addEventListener("mode-change", ({ detail }) => setConnected(detail.mode === "live"));

  const updateCount = () => {
    counter.textContent = `${input.value.length}/${MAX_LEN}`;
  };
  input.addEventListener("input", updateCount);
  updateCount();

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const text = input.value.trim();
    if (!text) {
      status.textContent = "Type a short note first.";
      return;
    }
    button.disabled = true;
    status.textContent = "Sending…";
    try {
      await deviceCommand("/message", { params: { text } });
      input.value = "";
      updateCount();
      status.textContent = "Sent. It stays on the screen until they shake it away.";
    } catch (error) {
      status.textContent = `The note was not sent: ${error.message}`;
    } finally {
      button.disabled = !deviceOnline();
    }
  });
}

export { setupWatchMessage, MAX_LEN };
