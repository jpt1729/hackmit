const status = document.getElementById("messages-status");
const roleButtons = [...document.querySelectorAll("[data-demo-role]")];
const roleLabel = document.getElementById("message-role-label");
const composeTitle = document.getElementById("message-compose-title");
const caregiverPanel = document.querySelector(".caregiver-panel");
const messageList = document.getElementById("message-list");
const brainBuddy = document.getElementById("brain-buddy");
const latestMessage = document.getElementById("caregiver-latest-message");
const caregiverHistory = document.getElementById("caregiver-message-history");
const recordButton = document.getElementById("message-record");
const preview = document.getElementById("message-preview");
const previewAudio = document.getElementById("message-preview-audio");
const transcriptInput = document.getElementById("message-transcript");
const transcribeOption = document.getElementById("message-transcribe");
const transcriptStatus = document.getElementById("transcript-status");
const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
const sendButton = document.getElementById("message-send");
const discardButton = document.getElementById("message-discard");
const ALLOWED_AUDIO = new Set(["audio/webm", "audio/mp4", "audio/ogg", "audio/wav", "audio/mpeg", "audio/mp3"]);
let currentRole = "patient";
let cachedMessages = [];
let latestCaregiverMessage = null;
let sending = false;
const drafts = { patient: null, caregiver: null };
let recorder = null;
let microphone = null;
let recordingTimer = null;
let pendingAudio = null;
let previewUrl = null;
let polling = false;
let recognition = null;
let finishRecognition = null;
let draftVersion = 0;

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

function setStatus(message) { status.textContent = message; }

function stopMicrophone() {
  clearTimeout(recordingTimer);
  microphone?.getTracks().forEach((track) => track.stop());
  microphone = null;
  recorder = null;
  recordButton.textContent = "Start recording";
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
  transcriptInput.value = "";
  transcriptStatus.textContent = "";
}

function showPreview(blob) {
  if (!blob.size || blob.size > 4_000_000) {
    setStatus("The recording is empty or too large. Please record a shorter message.");
    return;
  }
  const mimeType = blob.type.split(";")[0].toLowerCase();
  if (!ALLOWED_AUDIO.has(mimeType)) {
    setStatus("This browser could not create a supported recording. Please try another browser.");
    return;
  }
  if (previewUrl) URL.revokeObjectURL(previewUrl);
  pendingAudio = blob;
  previewUrl = URL.createObjectURL(blob);
  previewAudio.src = previewUrl;
  preview.hidden = false;
  setStatus("Listen to your message and check its words, then send when you are ready.");
}

function setRole(role, initial = false) {
  if (!["patient", "caregiver"].includes(role) || sending || (!initial && role === currentRole)) return;
  const wasRecording = Boolean(microphone) || recordButton.disabled;
  if (!initial && pendingAudio) {
    drafts[currentRole] = { audio: pendingAudio, transcript: transcriptInput.value };
  }
  discardAudio();
  currentRole = role;
  try { sessionStorage.setItem("granny-nanny-demo-role", role); } catch { /* View selection still works without storage. */ }
  roleButtons.forEach((button) => button.setAttribute("aria-pressed", String(button.dataset.demoRole === role)));
  const isPatient = role === "patient";
  const recipient = isPatient ? "your caregiver" : "the patient";
  roleLabel.textContent = `${isPatient ? "Patient" : "Caregiver"} view · Send to ${recipient}`;
  composeTitle.textContent = `Message ${recipient}`;
  brainBuddy.hidden = !isPatient;
  caregiverHistory.hidden = isPatient;
  caregiverPanel.hidden = isPatient;
  caregiverPanel.open = !isPatient;
  messageList.querySelectorAll("audio").forEach((audio) => audio.pause());
  latestMessage.querySelectorAll("audio").forEach((audio) => audio.pause());
  messageList.replaceChildren();
  renderMessages(cachedMessages);
  renderLatestMessage();
  const draft = drafts[role];
  if (draft) {
    showPreview(draft.audio);
    transcriptInput.value = draft.transcript;
    setStatus("Your unsent message is ready to review.");
  } else {
    setStatus(wasRecording ? "View switched. The unfinished recording was stopped." : `You can listen to messages or send one to ${recipient}.`);
  }
  refreshMessages();
}

roleButtons.forEach((button) => button.addEventListener("click", () => setRole(button.dataset.demoRole)));
document.addEventListener("demo-role-request", (event) => setRole(event.detail?.role));

function renderMessages(messages) {
  if (currentRole === "patient") return;
  if (!messages.length) {
    if (messageList.querySelector(".messages-empty")) return;
    messageList.replaceChildren();
    const empty = document.createElement("p");
    empty.className = "messages-empty";
    empty.textContent = "No voice messages yet. You can leave the first one below.";
    messageList.append(empty);
    return;
  }
  messageList.querySelector(".messages-empty")?.remove();
  const ids = new Set(messages.map((message) => String(message.id)));
  for (const card of [...messageList.children]) {
    if (!ids.has(card.dataset.messageId)) card.remove();
  }
  // Keep existing audio players mounted so polling cannot interrupt playback.
  for (const message of messages) {
    if (messageList.querySelector(`[data-message-id="${Number(message.id)}"]`)) continue;
    messageList.prepend(createMessageCard(message));
  }
}

function createMessageCard(message) {
  const card = document.createElement("article");
  card.dataset.messageId = message.id;
  card.className = "message-card " + (message.sender_role === currentRole ? "sent" : "received");
  const heading = document.createElement("strong");
  heading.textContent = message.sender_role === currentRole ? "You sent a message" : `From ${message.sender_name}`;
  const timestamp = document.createElement("time");
  timestamp.dateTime = new Date(message.created_at * 1000).toISOString();
  timestamp.textContent = new Date(message.created_at * 1000).toLocaleString([], { weekday: "short", hour: "numeric", minute: "2-digit" });
  const audio = document.createElement("audio");
  audio.controls = true;
  audio.preload = "none";
  audio.setAttribute("aria-label", `${heading.textContent}, ${timestamp.textContent}`);
  audio.src = `api/voice/messages/${message.id}/audio`;
  card.append(heading, timestamp, audio);
  if (message.transcript) {
    const label = document.createElement("strong");
    label.textContent = "Message in words";
    const transcript = document.createElement("p");
    transcript.className = "message-transcript";
    transcript.textContent = message.transcript;
    card.append(label, transcript);
  } else {
    const fallback = document.createElement("p");
    fallback.textContent = "No transcription was included. You can listen to the recording above.";
    card.append(fallback);
  }
  if (message.note) {
    const note = document.createElement("p");
    note.textContent = message.note;
    card.append(note);
  }
  return card;
}

function renderLatestMessage() {
  if (currentRole !== "patient") return;
  const message = latestCaregiverMessage;
  if (!message) {
    if (latestMessage.dataset.state === "empty") return;
    latestMessage.dataset.state = "empty";
    latestMessage.replaceChildren();
    const empty = document.createElement("p");
    empty.className = "messages-empty";
    empty.textContent = "No message from your caregiver yet. You can send them one below.";
    latestMessage.append(empty);
    return;
  }
  if (latestMessage.firstElementChild?.dataset.messageId === String(message.id)) return;
  const playing = latestMessage.querySelector("audio");
  // Finish the message being heard before displaying a newer arrival.
  if (playing && !playing.paused && !playing.ended) return;
  latestMessage.dataset.state = "message";
  const card = createMessageCard(message);
  const audio = card.querySelector("audio");
  audio.addEventListener("ended", renderLatestMessage);
  audio.addEventListener("pause", renderLatestMessage);
  latestMessage.replaceChildren(card);
}

async function refreshMessages() {
  if (polling || document.hidden) return;
  polling = true;
  try {
    const result = await api("messages");
    cachedMessages = result.messages || [];
    latestCaregiverMessage = result.latestCaregiverMessage !== undefined
      ? result.latestCaregiverMessage
      : [...cachedMessages].reverse().find((message) => message.sender_role === "caregiver") || null;
    renderMessages(cachedMessages);
    renderLatestMessage();
  } catch (error) {
    setStatus(error.message);
    if (!latestCaregiverMessage) {
      latestMessage.dataset.state = "unavailable";
      latestMessage.querySelector(".messages-empty").textContent = "Caregiver messages are unavailable right now. We’ll try again shortly.";
    }
  } finally { polling = false; }
}

recordButton.addEventListener("click", async () => {
  if (recorder?.state === "recording") { recorder.stop(); return; }
  if (!navigator.mediaDevices?.getUserMedia || !window.MediaRecorder) {
    setStatus("Recording needs a supported browser on HTTPS or localhost.");
    return;
  }
  discardAudio();
  const version = draftVersion;
  recordButton.disabled = true;
  try {
    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    if (version !== draftVersion) {
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
      await stopTranscription();
      recordButton.disabled = false;
      if (version !== draftVersion) return;
      showPreview(new Blob(chunks, { type }));
    };
    recorder.start();
    transcribeOption.disabled = true;
    startTranscription();
    recordButton.textContent = "Stop recording";
    setStatus("Recording… Tap Stop recording when you are done.");
    recordingTimer = setTimeout(() => { if (recorder?.state === "recording") recorder.stop(); }, 30_000);
  } catch {
    cancelTranscription();
    stopMicrophone();
    setStatus("Microphone access was unavailable. Allow microphone access in your browser, then try again.");
  } finally { recordButton.disabled = false; }
});

discardButton.addEventListener("click", () => { drafts[currentRole] = null; discardAudio(); setStatus("Recording discarded."); });
sendButton.addEventListener("click", async () => {
  if (!pendingAudio || sending) return;
  sending = true;
  const senderRole = currentRole;
  const controls = [sendButton, recordButton, discardButton, ...roleButtons, transcriptInput];
  controls.forEach((control) => { control.disabled = true; });
  setStatus("Sending your message…");
  try {
    const bytes = new Uint8Array(await pendingAudio.arrayBuffer());
    let binary = "";
    for (const byte of bytes) binary += String.fromCharCode(byte);
    await api("messages", { method: "POST", body: JSON.stringify({ senderRole, audio: { data: btoa(binary), mimeType: pendingAudio.type }, transcript: transcriptInput.value.trim() }) });
    discardAudio();
    drafts[senderRole] = null;
    setStatus(`Message sent to ${senderRole === "patient" ? "your caregiver" : "the patient"}. Switch views to hear it.`);
    refreshMessages();
  } catch (error) { setStatus(error.message); }
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
  try {
    if (sessionStorage.getItem("granny-nanny-demo-role") === "caregiver") currentRole = "caregiver";
  } catch { /* Start in patient view if storage is unavailable. */ }
  setRole(currentRole, true);
  setInterval(refreshMessages, 6000);
}

export { setupMessages };
