import { deviceHost, postDevice } from "./api.js";

// Sends a short note to the OLED on the wrist. The dashboard already records
// voice messages with a transcript; this puts that text where the wearer will
// actually see it.
const MAX_LEN = 96;

function setupWatchMessage(form) {
  if (!form) return;
  const input = form.querySelector("#watch-message-text");
  const button = form.querySelector("#watch-message-send");
  const counter = form.querySelector("#watch-message-count");
  // The status line lives outside the form so screen readers announce it
  // without it being part of the submitted controls.
  const status = document.getElementById("watch-message-status");
  if (!input || !button || !counter || !status) return;

  const connected = Boolean(deviceHost());
  input.disabled = button.disabled = !connected;
  if (!connected) {
    status.textContent = "Connect a wristband to send a note to its screen.";
  }

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
      await postDevice("/message", { text });
      input.value = "";
      updateCount();
      status.textContent = "Sent. It stays on the screen until they shake it away.";
    } catch (error) {
      console.warn("Could not send the message:", error);
      status.textContent = "Could not reach the wristband. Try again.";
    } finally {
      button.disabled = !deviceHost();
    }
  });
}

export { setupWatchMessage, MAX_LEN };
