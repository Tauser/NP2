"""Offline checks: bounded token handling, rejected redirects, no secret output."""
import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib import error
import check_spotify_client as helper


CLIENT = {"client_id": "a" * 32, "client_secret": "b" * 32}
TOKEN = {"access_token": "private-token-fixture", "token_type": "Bearer", "expires_in": 3600}


class Response(io.BytesIO):
    status = 200


class Opener:
    def __init__(self, data):
        self.data = data

    def open(self, req, timeout):
        assert req.full_url == helper.TOKEN_URL and req.method == "POST" and timeout == 8
        assert req.data == b"grant_type=client_credentials"
        assert "client_secret" not in req.full_url
        return Response(self.data)


class ClientTest(unittest.TestCase):
    def test_response(self):
        self.assertEqual(helper.check_client(CLIENT, Opener(json.dumps(TOKEN).encode())), (True, 200))
        for data in (b"{}", b"null", b"[]", b"not-json", b"x" * (helper.MAX_RESPONSE + 1)):
            self.assertFalse(helper.check_client(CLIENT, Opener(data))[0])
        for field, value in (("token_type", "Other"), ("expires_in", -1), ("expires_in", True),
                             ("expires_in", 86401), ("access_token", ""), ("access_token", 5)):
            body = dict(TOKEN, **{field: value})
            self.assertFalse(helper.check_client(CLIENT, Opener(json.dumps(body).encode()))[0])

    def test_transport(self):
        for failure in (error.HTTPError(helper.TOKEN_URL, 401, "private", {}, io.BytesIO(b"private")),
                        error.URLError("private"), TimeoutError("private")):
            with patch.object(Opener, "open", side_effect=failure):
                self.assertFalse(helper.check_client(CLIENT, Opener(b""))[0])
        self.assertIsNone(helper.NoRedirect().redirect_request(None, None, 302, "", {}, "https://other.invalid"))

    def test_file_and_output(self):
        base = Path(__file__).resolve().parents[1] / "firmware/build/spotify-client-host"
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as folder:
            assert Path(folder).resolve().is_relative_to(base.resolve())
            path = Path(folder) / "client.json"
            for body in (dict(CLIENT, password="private"), {"client_id": "a"}, [],
                         dict(CLIENT, client_secret="bad\nprivate"),
                         dict(CLIENT, client_id="COLE_O_CLIENT_ID_AQUI")):
                path.write_text(json.dumps(body))
                with self.assertRaises(ValueError):
                    helper.read_client(path)
            path.write_text(json.dumps(CLIENT), encoding="utf-8-sig")
            self.assertEqual(helper.read_client(path), CLIENT)
            output = io.StringIO()
            with patch.object(helper, "check_client", return_value=(True, 200)), contextlib.redirect_stdout(output):
                self.assertEqual(helper.main(["--credentials", str(path)]), 0)
            for private in (*CLIENT.values(), TOKEN["access_token"]):
                self.assertNotIn(private, output.getvalue())
            path.write_bytes(b"x" * 2049)
            with self.assertRaises(ValueError):
                helper.read_client(path)


if __name__ == "__main__":
    unittest.main()
