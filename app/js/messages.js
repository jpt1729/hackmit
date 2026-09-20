const status = document.getElementById("messages-status");
const messageThread = document.getElementById("message-thread");
const messageList = document.getElementById("message-list");
const buddyScene = document.getElementById("buddy-scene");
const messagePanel = document.getElementById("buddy-messages");
const messageOpen = document.getElementById("message-open");
const mascotButton = document.getElementById("brain-buddy-open");
const messageNav = document.getElementById("nav-message");
const invitationLabel = document.getElementById("message-invitation-label");
const recordButton = document.getElementById("message-record");
const textButton = document.getElementById("message-text");
const preview = document.getElementById("message-preview");
const previewAudio = document.getElementById("message-preview-audio");
const transcriptInput = document.getElementById("message-transcript");
const transcribeOption = document.getElementById("message-transcribe");
const transcriptStatus = document.getElementById("transcript-status");
const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
const sendButton = document.getElementById("message-send");
const discardButton = document.getElementById("message-discard");
const ALLOWED_AUDIO = new Set(["audio/webm", "audio/mp4", "audio/ogg", "audio/wav", "audio/mpeg", "audio/mp3"]);
const SENDER_ROLE = "caregiver";
let cachedMessages = [];
let latestIncomingMessage = null;
const messageHistory = new Map();
let historyInitialized = false;
let historyHasMore = false;
let loadingOlder = false;
let followNewestMessage = true;
let sending = false;
let recorder = null;
let microphone = null;
let recordingTimer = null;
let pendingAudio = null;
let previewUrl = null;
let polling = false;
let recognition = null;
let finishRecognition = null;
let draftVersion = 0;
let lastReadMessage = 0;
try { lastReadMessage = Number(localStorage.getItem("brain-buddy-caregiver-last-read-message")) || 0; } catch { /* Reading messages still works without storage. */ }

function updateInvitation() {
  invitationLabel.textContent = latestIncomingMessage && latestIncomingMessage.id > lastReadMessage ? "New message" : "Messages";
}

function markMessageRead() {
  if (!latestIncomingMessage || !buddyScene.classList.contains("messages-open")) return;
  const card = messageList.querySelector(`[data-message-id="${Number(latestIncomingMessage.id)}"]`);
  if (!card) return;
  const messageBounds = card.getBoundingClientRect();
  const threadBounds = messageThread.getBoundingClientRect();
  if (!threadBounds.height || messageBounds.bottom <= threadBounds.top || messageBounds.top >= threadBounds.bottom) return;
  lastReadMessage = Math.max(lastReadMessage, Number(latestIncomingMessage.id));
  try { localStorage.setItem("brain-buddy-caregiver-last-read-message", String(lastReadMessage)); } catch { /* Keep read state for this visit. */ }
  updateInvitation();
}

function setConversationOpen(open, moveFocus = false) {
  buddyScene.classList.toggle("messages-open", open);
  [messageOpen, mascotButton, messageNav].forEach((button) => button.setAttribute("aria-expanded", String(open)));
  mascotButton.setAttribute("aria-label", open ? "Close messages" : "Open messages");
  if (open) {
    messageThread.scrollTop = messageThread.scrollHeight;
    followNewestMessage = false;
    markMessageRead();
  }
  else {
    messagePanel.querySelectorAll("audio").forEach((audio) => audio.pause());
    if (recorder?.state === "recording") recorder.stop();
  }
  if (moveFocus) (open ? messagePanel : mascotButton).focus({ preventScroll: true });
}

messageOpen.addEventListener("click", () => setConversationOpen(true, true));
mascotButton.addEventListener("click", () => setConversationOpen(!buddyScene.classList.contains("messages-open"), true));
messageNav.addEventListener("click", () => {
  setConversationOpen(true, true);
  buddyScene.scrollIntoView({ block: "start", behavior: "auto" });
});

function cancelTranscription() {
  const active = recognition;
  recognition = null;
  finishRecognition?.();
  active?.abort();
}

function startTranscription() {
  if (!SpeechRecognition || !transcribeOption.checked) {
    transcriptStatus.textContent = "You can type the message's words after recording.";
    return;
  }
  const active = new SpeechRecognition();
  recognition = active;
  active.lang = document.documentElement.lang === "en" ? "en-US" : document.documentElement.lang || "en-US";
  active.continuous = true;
  active.interimResults = true;
  active.onresult = (event) => {
    if (recognition !== active) return;
    transcriptInput.value = Array.from(event.results, (result) => result[0].transcript).join(" ").trim().slice(0, 4000);
    transcriptStatus.textContent = "Turning your voice into words…";
  };
  active.onerror = () => {
    if (recognition !== active) return;
    transcriptStatus.textContent = "Speech to text stopped. You can check or type the words after recording.";
  };
  active.onend = () => {
    if (recognition !== active) return;
    recognition = null;
    finishRecognition?.();
    transcriptStatus.textContent = transcriptInput.value
      ? "Check the words after recording. You can correct anything that was missed."
      : "No words were captured. You can type them after recording.";
  };
  try {
    active.start();
    transcriptStatus.textContent = "Speech to text is listening…";
  } catch {
    recognition = null;
    transcriptStatus.textContent = "Speech to text is unavailable. You can type the words after recording.";
  }
}

function stopTranscription() {
  if (!recognition) return Promise.resolve();
  return new Promise((resolve) => {
    // Give the service time to deliver its final words before allowing edits.
    const timer = setTimeout(() => {
      cancelTranscription();
      transcriptStatus.textContent = "Check the words below and add anything that was missed.";
    }, 2000);
    finishRecognition = () => {
      clearTimeout(timer);
      finishRecognition = null;
      resolve();
    };
    try { recognition.stop(); } catch { cancelTranscription(); }
  });
}

async function api(path, options = {}) {
  const response = await fetch(`api/voice/${path}`, {
    cache: "no-store",
    credentials: "same-origin",
    ...options,
    headers: options.body ? { "Content-Type": "application/json" } : undefined
  });
  const body = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(body.error || `Message service error (${response.status})`);
  return body;
}

function setStatus(message, kind = "hint") {
  status.textContent = message;
  status.dataset.kind = kind;
}

function stopMicrophone() {
  clearTimeout(recordingTimer);
  microphone?.getTracks().forEach((track) => track.stop());
  microphone = null;
  recorder = null;
  recordButton.textContent = "Record a voice message";
  recordButton.dataset.recording = "false";
  textButton.disabled = false;
  transcribeOption.disabled = !SpeechRecognition;
}

function discardAudio() {
  draftVersion++;
  cancelTranscription();
  if (recorder?.state === "recording") recorder.stop();
  stopMicrophone();
  pendingAudio = null;
  previewAudio.removeAttribute("src");
  previewAudio.load();
  if (previewUrl) URL.revokeObjectURL(previewUrl);
  previewUrl = null;
  preview.hidden = true;
  textButton.setAttribute("aria-expanded", "false");
  previewAudio.hidden = true;
  transcriptInput.value = "";
  transcriptStatus.textContent = "";
}

function showPreview(blob) {
  if (!blob.size || blob.size > 4_000_000) {
    setStatus("Please record a shorter message.", "error");
    return;
  }
  const mimeType = blob.type.split(";")[0].toLowerCase();
  if (!ALLOWED_AUDIO.has(mimeType)) {
    setStatus("Recording is unavailable in this browser. You can text your message.", "error");
    return;
  }
  if (previewUrl) URL.revokeObjectURL(previewUrl);
  pendingAudio = blob;
  previewUrl = URL.createObjectURL(blob);
  previewAudio.src = previewUrl;
  previewAudio.hidden = false;
  preview.hidden = false;
  textButton.setAttribute("aria-expanded", "true");
  setStatus("Listen to your message and check its words, then send when you are ready.");
}

function showTextPreview(moveFocus = true) {
  preview.hidden = false;
  previewAudio.hidden = !pendingAudio;
  textButton.setAttribute("aria-expanded", "true");
  if (moveFocus) transcriptInput.focus();
}

textButton.addEventListener("click", () => showTextPreview());

function createMessageCard(message) {
  const card = document.createElement("article");
  card.dataset.messageId = message.id;
  card.className = "message-card " + (message.sender_role === SENDER_ROLE ? "sent" : "received");
  const heading = document.createElement("strong");
  heading.className = "message-sender";
  heading.textContent = message.sender_role === SENDER_ROLE ? "You" : message.sender_name || "Patient";
  const timestamp = document.createElement("time");
  timestamp.dateTime = new Date(message.created_at * 1000).toISOString();
  timestamp.textContent = new Date(message.created_at * 1000).toLocaleString([], { weekday: "short", hour: "numeric", minute: "2-digit" });
  card.append(heading, timestamp);
  if (message.transcript) {
    const transcript = document.createElement("p");
    transcript.className = "message-transcript";
    transcript.textContent = message.transcript;
    card.append(transcript);
  } else {
    const fallback = document.createElement("p");
    fallback.className = "message-transcript";
    fallback.textContent = "Voice message";
    card.append(fallback);
  }
  if (message.has_audio ?? message.mime_type !== "text/plain") {
    const audio = document.createElement("audio");
    audio.controls = true;
    audio.preload = "none";
    audio.setAttribute("aria-label", `${heading.textContent}, ${timestamp.textContent}`);
    audio.src = `api/voice/messages/${message.id}/audio`;
    card.append(audio);
    const play = document.createElement("button");
    play.type = "button";
    play.className = "message-play";
    play.setAttribute("aria-label", `Play message from ${heading.textContent === "You" ? "you" : heading.textContent}`);
    play.setAttribute("aria-pressed", "false");
    play.innerHTML = '<svg class="play-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 3.5 21 12 6 20.5Z"/></svg><svg class="pause-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M5 4h5v16H5zm9 0h5v16h-5z"/></svg>';
    const syncPlayback = () => {
      play.setAttribute("aria-pressed", String(!audio.paused));
      play.setAttribute("aria-label", `${audio.paused ? "Play" : "Pause"} message from ${heading.textContent === "You" ? "you" : heading.textContent}`);
    };
    audio.addEventListener("play", syncPlayback);
    audio.addEventListener("pause", syncPlayback);
    audio.addEventListener("ended", syncPlayback);
    play.addEventListener("click", async () => {
      if (!audio.paused) { audio.pause(); return; }
      document.querySelectorAll("audio").forEach((other) => { if (other !== audio) other.pause(); });
      try { await audio.play(); }
      catch { setStatus("This recording could not play. Please try again.", "error"); }
    });
    card.append(play);
  }
  if (message.note) {
    const note = document.createElement("p");
    note.textContent = message.note;
    card.append(note);
  }
  return card;
}

function renderHistory({ prepend = false } = {}) {
  const messages = [...messageHistory.values()].sort((a, b) => a.id - b.id);
  const previousHeight = messageThread.scrollHeight;
  const previousTop = messageThread.scrollTop;
  const atBottom = previousHeight - previousTop - messageThread.clientHeight < 24;
  const keepAtBottom = !prepend && (followNewestMessage || atBottom || !buddyScene.classList.contains("messages-open"));
  if (!messages.length) {
    if (!messageList.querySelector(".messages-empty")) {
      const empty = document.createElement("p");
      empty.className = "messages-empty";
      empty.textContent = "No messages yet";
      messageList.replaceChildren(empty);
    }
  } else {
    messageList.querySelector(".messages-empty")?.remove();
    const cards = new Map([...messageList.children].map((card) => [Number(card.dataset.messageId), card]));
    messages.forEach((message, index) => {
      const card = cards.get(message.id) || createMessageCard(message);
      card.classList.toggle("latest-incoming", message.id === latestIncomingMessage?.id);
      // Keep mounted players and their playback state when polling or loading history.
      if (messageList.children[index] !== card) {
        messageList.insertBefore(card, messageList.children[index] || null);
      }
    });
  }
  if (prepend) messageThread.scrollTop = previousTop + messageThread.scrollHeight - previousHeight;
  else if (keepAtBottom) messageThread.scrollTop = messageThread.scrollHeight;
  followNewestMessage = false;
  updateInvitation();
  markMessageRead();
}

async function loadOlderMessages() {
  if (loadingOlder || !historyHasMore || !messageHistory.size || !buddyScene.classList.contains("messages-open")) return;
  loadingOlder = true;
  messageThread.setAttribute("aria-busy", "true");
  try {
    const oldest = Math.min(...messageHistory.keys());
    const result = await api(`messages?before=${oldest}`);
    for (const message of result.messages || []) messageHistory.set(message.id, message);
    historyHasMore = Boolean(result.hasMore);
    renderHistory({ prepend: true });
  } catch {
    setStatus("Older messages are unavailable right now. Please try again.", "error");
  } finally {
    loadingOlder = false;
    messageThread.removeAttribute("aria-busy");
  }
}

messageThread.addEventListener("scroll", () => {
  markMessageRead();
  if (messageThread.scrollTop < 48) loadOlderMessages();
}, { passive: true });

async function refreshMessages() {
  if (polling || document.hidden) return;
  polling = true;
  try {
    const result = await api("messages");
    cachedMessages = result.messages || [];
    latestIncomingMessage = result.latestPatientMessage !== undefined
      ? result.latestPatientMessage
      : [...cachedMessages].reverse().find((message) => message.sender_role === "patient") || null;
    for (const message of cachedMessages) messageHistory.set(message.id, message);
    if (!historyInitialized) {
      historyHasMore = Boolean(result.hasMore);
      historyInitialized = true;
    }
    renderHistory();
  } catch (error) {
    setStatus("Messages are unavailable right now. Please try again shortly.", "error");
    if (!latestIncomingMessage) {
      const empty = messageList.querySelector(".messages-empty");
      if (empty) empty.textContent = "Messages unavailable";
    }
  } finally { polling = false; }
}

recordButton.addEventListener("click", async () => {
  if (recorder?.state === "recording") { recorder.stop(); return; }
  if (!navigator.mediaDevices?.getUserMedia || !window.MediaRecorder) {
    setStatus("Recording is unavailable here. You can text your message.", "error");
    return;
  }
  discardAudio();
  const version = draftVersion;
  recordButton.disabled = true;
  textButton.disabled = true;
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    if (version !== draftVersion || !buddyScene.classList.contains("messages-open")) {
      stream.getTracks().forEach((track) => track.stop());
      return;
    }
    microphone = stream;
    const mimeType = ["audio/webm;codecs=opus", "audio/mp4", "audio/webm", "audio/ogg"].find((type) => MediaRecorder.isTypeSupported(type));
    const chunks = [];
    recorder = new MediaRecorder(microphone, mimeType ? { mimeType } : undefined);
    recorder.ondataavailable = (event) => { if (event.data.size) chunks.push(event.data); };
    const activeRecorder = recorder;
    recorder.onstop = async () => {
      const type = activeRecorder.mimeType;
      if (recorder !== activeRecorder) return;
      stopMicrophone();
      recordButton.disabled = true;
      textButton.disabled = true;
      await stopTranscription();
      recordButton.disabled = false;
      textButton.disabled = false;
      if (version !== draftVersion) return;
      showPreview(new Blob(chunks, { type }));
    };
    recorder.start();
    transcribeOption.disabled = true;
    startTranscription();
    recordButton.textContent = "Stop recording";
    recordButton.dataset.recording = "true";
    setStatus("Recording…", "status");
    recordingTimer = setTimeout(() => { if (recorder?.state === "recording") recorder.stop(); }, 30_000);
  } catch {
    cancelTranscription();
    stopMicrophone();
    setStatus("Microphone access is unavailable. You can text your message.", "error");
  } finally {
    recordButton.disabled = false;
    if (!recorder) textButton.disabled = false;
  }
});

discardButton.addEventListener("click", () => { discardAudio(); setStatus("Message discarded.", "status"); });
sendButton.addEventListener("click", async () => {
  if (sending) return;
  if (!pendingAudio && !transcriptInput.value.trim()) {
    setStatus("Enter a message first.", "error");
    transcriptInput.focus();
    return;
  }
  sending = true;
  const senderRole = SENDER_ROLE;
  const controls = [sendButton, recordButton, textButton, discardButton, transcriptInput];
  controls.forEach((control) => { control.disabled = true; });
  setStatus("Sending…", "status");
  try {
    let audio = null;
    if (pendingAudio) {
      const bytes = new Uint8Array(await pendingAudio.arrayBuffer());
      let binary = "";
      for (const byte of bytes) binary += String.fromCharCode(byte);
      audio = { data: btoa(binary), mimeType: pendingAudio.type };
    }
    await api("messages", { method: "POST", body: JSON.stringify({ senderRole, audio, transcript: transcriptInput.value.trim() }) });
    discardAudio();
    setStatus("Message sent", "status");
    followNewestMessage = true;
    refreshMessages();
  } catch (error) { setStatus(error.message, "error"); }
  finally { sending = false; controls.forEach((control) => { control.disabled = false; }); }
});

async function setupMessages() {
  if (!SpeechRecognition) {
    transcribeOption.checked = false;
    transcribeOption.disabled = true;
    document.getElementById("transcription-help").textContent = "Speech to text is unavailable in this browser. You can record a message and type its words before sending.";
  }
  if (!navigator.mediaDevices?.getUserMedia || !window.MediaRecorder) {
    recordButton.hidden = true;
    document.getElementById("message-recording-help").textContent = "Recording needs a supported browser on HTTPS or localhost. You can still listen to received messages here.";
  }
  setConversationOpen(false);
  renderHistory();
  refreshMessages();
  setInterval(refreshMessages, 6000);
}

export { setupMessages };
