function setupCompanion() {
  const button = document.getElementById("read-aloud");
  const label = button.querySelector("span");
  const status = document.getElementById("speech-status");
  const mascot = document.getElementById("brain-mascot");
  let currentUtterance = null;

  function stopSpeaking() {
    const wasSpeaking = currentUtterance !== null;
    currentUtterance = null;
    if (wasSpeaking) window.speechSynthesis.cancel();
    label.textContent = "Read aloud";
    mascot.classList.remove("is-speaking");
    status.textContent = "";
  }

  if (!("speechSynthesis" in window) || !("SpeechSynthesisUtterance" in window)) {
    button.hidden = true;
    status.textContent = "You can read Pip’s message above. Read aloud is unavailable in this browser.";
    return;
  }
  document.addEventListener("routine-message-change", stopSpeaking);
  document.addEventListener("visibilitychange", () => { if (document.hidden) stopSpeaking(); });
  // Ask for the voice list early; some browsers load it asynchronously.
  window.speechSynthesis.getVoices();
  button.addEventListener("click", () => {
    if (currentUtterance) { stopSpeaking(); return; }
    // Keep speech on the device, consistent with the app's local-only design.
    const voice = window.speechSynthesis.getVoices().find((item) => item.localService && /^en(?:-|$)/i.test(item.lang));
    if (!voice) {
      status.textContent = "No English voice is available on this device yet. You can read Pip’s message above.";
      return;
    }
    const message = ["focus-title", "focus-description", "next-event"]
      .map((id) => document.getElementById(id).textContent).filter(Boolean).join(" ");
    const utterance = new SpeechSynthesisUtterance(message);
    utterance.voice = voice;
    utterance.lang = voice.lang;
    utterance.rate = 0.85;
    currentUtterance = utterance;
    status.textContent = "";
    label.textContent = "Stop reading";
    mascot.classList.add("is-speaking");
    utterance.onend = () => { if (currentUtterance === utterance) stopSpeaking(); };
    utterance.onerror = () => {
      if (currentUtterance !== utterance) return;
      stopSpeaking();
      status.textContent = "Read aloud could not start. You can read Pip’s message above.";
    };
    try { window.speechSynthesis.speak(utterance); }
    catch { utterance.onerror(); }
  });
}

export { setupCompanion };
