"""Offline OAuth helper tests: loopback/PKCE, scopes, token secrecy, USB ACK."""
import base64
import contextlib
import hashlib
import io
import json
import tempfile
import types
import unittest
import urllib.parse
from pathlib import Path
from unittest.mock import patch

import link_google_agenda as helper


class OAuthTests(unittest.TestCase):
    def run_usb(self, *, check_only=False, reject=False, linked=True, short_write=False):
        credentials = {"client_id": "fixture.apps.googleusercontent.com", "client_secret": "fixture-private-client",
                       "refresh_token": "fixture-private-refresh" + "x" * 400, "subject": "12345"}
        output = io.StringIO()
        test = self

        class FirmwareSerial:
            def __init__(self):
                self.armed = False
                self.windows = 0
                self.commands = []
                self.partial = bytearray()
                self.replies = []
                self.clock = 0

            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def reset_input_buffer(self):
                self.replies.clear()

            def flush(self):
                pass

            def write(self, data):
                # The firmware has a small RX ring and consumes one command
                # per arm. Its ACK never means a durable commit by itself.
                test.assertLessEqual(len(data), 32)
                if short_write:
                    return len(data) - 1
                test.assertTrue(self.armed)
                self.partial.extend(data)
                if self.partial.endswith(b"\n"):
                    command = bytes(self.partial)
                    self.partial.clear()
                    self.commands.append(command)
                    self.armed = False
                    if command.startswith(b"GOOGLE_AUTH "):
                        test.assertEqual(json.loads(command[12:]), credentials)
                        self.replies.append(b"ERR ESP_ERR_INVALID_ARG\n" if reject else b"OK\n")
                    else:
                        test.assertEqual(command, b"GOOGLE_STATUS\n")
                        self.replies.append(b"GOOGLE_LINKED\n" if linked else b"GOOGLE_UNLINKED\n")
                return len(data)

            def readline(self, _size):
                self.clock += 1
                return self.replies.pop(0) if self.replies else b""

        connection = FirmwareSerial()

        def arm(_prompt):
            if connection.windows:
                self.assertNotIn("Conta vinculada e credenciais persistidas", output.getvalue())
            connection.armed = True
            connection.windows += 1
            return ""

        fake_serial = types.SimpleNamespace(Serial=lambda **_kwargs: connection)
        with patch.dict("sys.modules", {"serial": fake_serial}), patch("builtins.input", side_effect=arm), \
             patch.object(helper.time, "sleep"), patch.object(helper.time, "monotonic", side_effect=lambda: connection.clock), \
             contextlib.redirect_stdout(output):
            helper.provision("fixture-port", None if check_only else credentials)
        for private in (credentials["client_secret"], credentials["refresh_token"]):
            self.assertNotIn(private, output.getvalue())
        return connection, output.getvalue()

    def test_usb_paced_handoff_and_separate_physical_confirmation(self):
        connection, output = self.run_usb()
        self.assertEqual(connection.windows, 2)
        self.assertEqual(len(connection.commands), 2)
        self.assertIn("Conta vinculada e credenciais persistidas", output)

    def test_usb_check_only_does_not_authorize_or_send_credentials(self):
        connection, _output = self.run_usb(check_only=True)
        self.assertEqual(connection.windows, 1)
        self.assertEqual(connection.commands, [b"GOOGLE_STATUS\n"])

    def test_usb_ack_alone_never_confirms_commit(self):
        with self.assertRaisesRegex(RuntimeError, "ainda não confirmou"):
            self.run_usb(linked=False)

    def test_usb_rejection_and_short_write_fail(self):
        with self.assertRaisesRegex(RuntimeError, "recusou"):
            self.run_usb(reject=True)
        with self.assertRaisesRegex(RuntimeError, "incompleto"):
            self.run_usb(short_write=True)

    def test_loopback_pkce_and_private_result(self):
        client_id = "fixture.apps.googleusercontent.com"
        client_secret = "fixture-private-client"
        authorization = {}

        def browser(url):
            parsed = urllib.parse.urlsplit(url)
            self.assertEqual(parsed.netloc, "accounts.google.com")
            fields = urllib.parse.parse_qs(parsed.query)
            self.assertEqual(fields["code_challenge_method"], ["S256"])
            self.assertEqual(fields["access_type"], ["offline"])
            authorization.update(fields)
            redirect = urllib.parse.urlsplit(fields["redirect_uri"][0])
            self.assertEqual(redirect.hostname, "127.0.0.1")

            return True

        class FakeSocket:
            def __init__(self, request):
                self.request = request
                self.response = bytearray()

            def makefile(self, *_args, **_kwargs):
                return io.BytesIO(self.request)

            def sendall(self, data):
                self.response.extend(data)

        class FakeServer:
            def __init__(self, address, handler):
                self_test.assertEqual(address, ("127.0.0.1", 0))
                self.handler = handler
                self.server_port = 35487
                self.calls = 0

            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def handle_request(self):
                self.calls += 1
                query = {"state": "invalid" if self.calls == 1 else authorization["state"][0], "code": "fixture-private-code"}
                request = ("GET /?" + urllib.parse.urlencode(query) + " HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n").encode()
                connection = FakeSocket(request)
                self.handler(connection, ("127.0.0.1", 50000), self)
                self_test.assertTrue(connection.response.startswith(b"HTTP/1.0 400" if self.calls == 1 else b"HTTP/1.0 200"))

        class Response:
            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def read(self, _limit):
                payload = base64.urlsafe_b64encode(json.dumps({"aud": client_id, "iss": "https://accounts.google.com", "sub": "12345"}).encode()).rstrip(b"=").decode()
                return json.dumps({"refresh_token": "fixture-private-refresh", "id_token": f"header.{payload}.signature", "scope": " ".join(helper.SCOPES)}).encode()

        class Opener:
            def open(self, request, timeout):
                self_test.assertEqual(request.full_url, "https://oauth2.googleapis.com/token")
                self_test.assertEqual(timeout, 30)
                fields = urllib.parse.parse_qs(request.data.decode())
                verifier = fields["code_verifier"][0]
                challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
                self_test.assertEqual(authorization["code_challenge"], [challenge])
                self_test.assertEqual(fields["code"], ["fixture-private-code"])
                return Response()

        self_test = self
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "firmware/build") as temporary:
            path = Path(temporary) / "desktop.json"
            path.write_text(json.dumps({"installed": {"client_id": client_id, "client_secret": client_secret}}))
            output = io.StringIO()
            with patch.object(helper.webbrowser, "open", browser), patch.object(helper.http.server, "HTTPServer", FakeServer), patch.object(helper.urllib.request, "build_opener", return_value=Opener()), contextlib.redirect_stdout(output):
                credentials = helper.authorize(path)
            self.assertEqual(credentials["subject"], "12345")
            self.assertEqual(credentials["refresh_token"], "fixture-private-refresh")
            for private in (client_secret, "fixture-private-code", "fixture-private-refresh"):
                self.assertNotIn(private, output.getvalue())


if __name__ == "__main__":
    unittest.main()
