// Static-site demo storage. Audio stays as a Blob in this browser's IndexedDB.
const databaseName = `brain-buddy-messages:${new URL(".", location.href).pathname}`;
let databasePromise;

function database() {
  if (!databasePromise) {
    databasePromise = new Promise((resolve, reject) => {
      const request = indexedDB.open(databaseName, 1);
      request.onupgradeneeded = () => {
        request.result.createObjectStore("messages", { keyPath: "id", autoIncrement: true });
      };
      request.onerror = () => reject(request.error);
      request.onsuccess = () => {
        const db = request.result;
        db.onversionchange = () => { db.close(); databasePromise = null; };
        resolve(db);
      };
    }).catch((error) => { databasePromise = null; throw error; });
  }
  return databasePromise;
}

async function saveLocalMessage(transcript, audio) {
  const db = await database();
  return new Promise((resolve, reject) => {
    const transaction = db.transaction("messages", "readwrite");
    const request = transaction.objectStore("messages").add({
      sender_role: "caregiver",
      sender_name: "You",
      created_at: Date.now() / 1000,
      transcript,
      mime_type: audio ? audio.type : "text/plain",
      has_audio: Boolean(audio),
      audio_blob: audio || null
    });
    transaction.oncomplete = () => resolve(request.result);
    transaction.onabort = () => reject(transaction.error || request.error || new Error("Local save failed"));
    transaction.onerror = () => reject(transaction.error || request.error);
  });
}

async function listLocalMessages(before) {
  const db = await database();
  return new Promise((resolve, reject) => {
    const messages = [];
    let hasMore = false;
    const transaction = db.transaction("messages", "readonly");
    const range = before ? IDBKeyRange.upperBound(Number(before), true) : null;
    const request = transaction.objectStore("messages").openCursor(range, "prev");
    request.onsuccess = () => {
      const cursor = request.result;
      if (!cursor) return;
      if (messages.length === 50) { hasMore = true; return; }
      messages.push(cursor.value);
      cursor.continue();
    };
    transaction.oncomplete = () => resolve({ messages: messages.reverse(), hasMore, latestPatientMessage: null });
    transaction.onabort = transaction.onerror = () => reject(transaction.error || request.error);
  });
}

export { saveLocalMessage, listLocalMessages };
