"""Check the audio requests used by browser and embedded media players."""
import http.client
from pathlib import Path
import tempfile
import threading
import unittest

import message_server


class AudioRequests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.original_db = message_server.DB_PATH
        message_server.DB_PATH = Path(cls.folder.name) / "messages.sqlite3"
        message_server.initialize_database()
        cls.audio = b"audio fixture bytes"
        with message_server.database() as db:
            db.execute("""INSERT INTO messages
                (sender_id, recipient_role, created_at, mime_type, audio)
                VALUES (1, 'patient', 1, 'audio/webm', ?)""", (cls.audio,))
        cls.server = message_server.ThreadingHTTPServer(("127.0.0.1", 0), message_server.Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        message_server.DB_PATH = cls.original_db
        cls.folder.cleanup()

    def request(self, method="GET", byte_range=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port)
        try:
            connection.request(method, "/api/voice/messages/1/audio",
                               headers={"Range": byte_range} if byte_range else {})
            response = connection.getresponse()
            return response.status, dict(response.getheaders()), response.read()
        finally:
            connection.close()

    def test_full_recording_and_head(self):
        status, headers, data = self.request()
        self.assertEqual((status, data), (200, self.audio))
        self.assertEqual(headers["Accept-Ranges"], "bytes")
        status, headers, data = self.request("HEAD")
        self.assertEqual((status, data), (200, b""))
        self.assertEqual(int(headers["Content-Length"]), len(self.audio))

    def test_probe_and_seek(self):
        for byte_range, start, end in [("bytes=0-1", 0, 1), ("bytes=3-", 3, len(self.audio)-1),
                                       ("bytes=-4", len(self.audio)-4, len(self.audio)-1),
                                       ("bytes=2-999", 2, len(self.audio)-1)]:
            with self.subTest(byte_range=byte_range):
                status, headers, data = self.request(byte_range=byte_range)
                self.assertEqual(status, 206)
                self.assertEqual(data, self.audio[start:end+1])
                self.assertEqual(headers["Content-Range"], f"bytes {start}-{end}/{len(self.audio)}")
                self.assertEqual(int(headers["Content-Length"]), len(data))

    def test_unsatisfiable_ranges(self):
        for byte_range in ["bytes=999-", "bytes=5-2", "bytes=-0"]:
            with self.subTest(byte_range=byte_range):
                status, headers, data = self.request(byte_range=byte_range)
                self.assertEqual((status, data), (416, b""))
                self.assertEqual(headers["Content-Range"], f"bytes */{len(self.audio)}")


if __name__ == "__main__":
    unittest.main()
