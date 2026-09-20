#!/usr/bin/env python3
"""Local patient/caregiver demo voice message server for Granny Nanny. No packages required."""

import base64
import binascii
from contextlib import closing
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import sqlite3
import ssl
import time
from urllib.parse import urlparse


ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
DB_PATH = Path(os.environ.get("VOICE_DB_PATH", ROOT / "var" / "voice_messages.sqlite3"))
MAX_BODY = 6_000_000
MAX_AUDIO = 4_000_000
ALLOWED_AUDIO = {"audio/webm", "audio/mp4", "audio/ogg", "audio/wav", "audio/mpeg", "audio/mp3"}


def database():
    connection = sqlite3.connect(DB_PATH, timeout=10)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    return connection


def initialize_database():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    with closing(database()) as db, db:
        db.executescript("""
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY,
                role TEXT NOT NULL UNIQUE CHECK(role IN ('patient', 'caregiver')),
                display_name TEXT NOT NULL,
                salt BLOB NOT NULL,
                password_hash BLOB NOT NULL
            );
            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY,
                sender_id INTEGER NOT NULL REFERENCES users(id),
                recipient_role TEXT NOT NULL CHECK(recipient_role IN ('patient', 'caregiver')),
                created_at INTEGER NOT NULL,
                mime_type TEXT NOT NULL,
                audio BLOB NOT NULL,
                note TEXT NOT NULL DEFAULT ''
            );
        """)
        # Preserve recordings created before speech-to-text was added.
        columns = {row["name"] for row in db.execute("PRAGMA table_info(messages)")}
        if "transcript" not in columns:
            db.execute("ALTER TABLE messages ADD COLUMN transcript TEXT NOT NULL DEFAULT ''")

        # Keep legacy user IDs and credential columns so existing recordings survive.
        # Demo participants have no password and can be selected freely in the UI.
        for role, name in (("patient", "Patient"), ("caregiver", "Caregiver")):
            db.execute("""INSERT OR IGNORE INTO users (role, display_name, salt, password_hash)
                          VALUES (?, ?, ?, ?)""", (role, name, b"", b""))


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DOCS), **kwargs)

    def end_headers(self):
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "same-origin")
        super().end_headers()

    def json_response(self, status, data):
        payload = json.dumps(data, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def read_json(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError as exc:
            raise ValueError("Invalid request length") from exc
        if length < 2 or length > MAX_BODY:
            raise ValueError("Request is empty or too large")
        if self.headers.get("Content-Type", "").split(";")[0].strip() != "application/json":
            raise ValueError("JSON is required")
        try:
            value = json.loads(self.rfile.read(length))
        except (ValueError, UnicodeDecodeError) as exc:
            raise ValueError("Invalid JSON") from exc
        if not isinstance(value, dict):
            raise ValueError("Expected a JSON object")
        return value

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/voice/messages":
            with closing(database()) as db:
                rows = db.execute("""
                    SELECT messages.id, messages.created_at, messages.mime_type, messages.note, messages.transcript,
                           users.role AS sender_role, users.display_name AS sender_name
                    FROM messages JOIN users ON users.id = messages.sender_id
                    ORDER BY messages.id DESC LIMIT 50
                """).fetchall()
                latest = db.execute("""
                    SELECT messages.id, messages.created_at, messages.mime_type, messages.note, messages.transcript,
                           users.role AS sender_role, users.display_name AS sender_name
                    FROM messages JOIN users ON users.id = messages.sender_id
                    WHERE users.role = 'caregiver'
                    ORDER BY messages.id DESC LIMIT 1
                """).fetchone()
            self.json_response(200, {
                "messages": [dict(row) for row in reversed(rows)],
                "latestCaregiverMessage": dict(latest) if latest else None,
            })
            return
        match = re.fullmatch(r"/api/voice/messages/(\d+)/audio", path)
        if match:
            with closing(database()) as db:
                row = db.execute("""
                    SELECT mime_type, audio FROM messages
                    WHERE id = ?
                """, (int(match.group(1)),)).fetchone()
            if not row:
                self.json_response(404, {"error": "Message not found"})
                return
            self.send_response(200)
            self.send_header("Content-Type", row["mime_type"])
            self.send_header("Cache-Control", "private, no-store")
            self.send_header("Content-Length", str(len(row["audio"])))
            self.end_headers()
            self.wfile.write(row["audio"])
            return
        if path.startswith("/api/"):
            self.json_response(404, {"error": "Not found"})
            return
        super().do_GET()

    def do_POST(self):
        path = urlparse(self.path).path
        origin = self.headers.get("Origin")
        if origin:
            parsed = urlparse(origin)
            expected_scheme = "https" if isinstance(self.request, ssl.SSLSocket) else "http"
            if parsed.netloc != self.headers.get("Host") or parsed.scheme != expected_scheme:
                self.json_response(403, {"error": "Requests must come from this site."})
                return
        try:
            if path == "/api/voice/messages":
                self.send_message()
            else:
                self.json_response(404, {"error": "Not found"})
        except ValueError as exc:
            self.json_response(400, {"error": str(exc)})
        except (sqlite3.Error, OSError):
            self.json_response(500, {"error": "The message service could not save this request."})

    def send_message(self):
        body = self.read_json()
        role = body.get("senderRole")
        if role not in ("patient", "caregiver"):
            raise ValueError("Choose the patient or caregiver view before sending.")
        with closing(database()) as db:
            user = db.execute("SELECT id, role FROM users WHERE role = ?", (role,)).fetchone()
        audio = body.get("audio")
        if not isinstance(audio, dict) or not isinstance(audio.get("data"), str):
            raise ValueError("Record an audio message first.")
        mime_type = str(audio.get("mimeType", "")).split(";")[0].lower()
        if mime_type not in ALLOWED_AUDIO:
            raise ValueError("Use a WebM, MP4, OGG, WAV, or MP3 recording.")
        try:
            data = base64.b64decode(audio["data"], validate=True)
        except (binascii.Error, ValueError) as exc:
            raise ValueError("The audio recording could not be read.") from exc
        if not 1 <= len(data) <= MAX_AUDIO:
            raise ValueError("The audio recording must be under 4 MB.")
        note = str(body.get("note", "")).strip()[:160]
        transcript = body.get("transcript", "")
        if not isinstance(transcript, str) or len(transcript) > 4000:
            raise ValueError("The transcript must be text of up to 4000 characters.")
        recipient_role = "patient" if user["role"] == "caregiver" else "caregiver"
        with closing(database()) as db, db:
            db.execute("""INSERT INTO messages (sender_id, recipient_role, created_at, mime_type, audio, note, transcript)
                          VALUES (?, ?, ?, ?, ?, ?, ?)""",
                       (user["id"], recipient_role, int(time.time()), mime_type, data, note, transcript.strip()))
        self.json_response(201, {"ok": True})


def main():
    host = os.environ.get("VOICE_HOST", "127.0.0.1")
    port = int(os.environ.get("VOICE_PORT", "8787"))
    cert = os.environ.get("VOICE_TLS_CERT")
    key = os.environ.get("VOICE_TLS_KEY")
    if bool(cert) != bool(key):
        raise SystemExit("Set both VOICE_TLS_CERT and VOICE_TLS_KEY for HTTPS.")
    if host not in ("127.0.0.1", "localhost", "::1") and not cert:
        raise SystemExit("Use HTTPS when serving voice messages to other devices.")
    initialize_database()
    server = ThreadingHTTPServer((host, port), Handler)
    if cert:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(cert, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
    protocol = "https" if cert else "http"
    print(f"Granny Nanny message server: {protocol}://{host}:{port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
