"""Offline PKCE callback/scope/privacy checks; never accesses Spotify."""
import contextlib
import io
import json
import tempfile
import threading
import unittest
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from unittest.mock import patch
import link_spotify_remote as helper


class PKCETest(unittest.TestCase):
    def test_callback_and_private_handoff(self):
        requests = []
        callbacks = []

        def browser(url):
            query = urllib.parse.parse_qs(urllib.parse.urlsplit(url).query)
            self.assertEqual(query["redirect_uri"], [helper.REDIRECT])
            self.assertEqual(query["code_challenge_method"], ["S256"])
            self.assertEqual(set(query["scope"][0].split()), set(helper.SCOPES))
            self.assertNotIn("client_secret", query)

            def callback():
                try:
                    with urllib.request.urlopen(helper.REDIRECT + "?code=fixture-code&state=wrong", timeout=3):
                        pass
                except urllib.error.HTTPError as error:
                    callbacks.append(error.code)
                    error.close()
                good = helper.REDIRECT + "?" + urllib.parse.urlencode({"code": "fixture-code", "state": query["state"][0]})
                with urllib.request.urlopen(good, timeout=3) as response:
                    callbacks.append(response.status)
            thread = threading.Thread(target=callback)
            thread.start()
            return True

        def request(value):
            requests.append(value)
            if len(requests) == 1:
                form = urllib.parse.parse_qs(value.data.decode())
                self.assertEqual(form["grant_type"], ["authorization_code"])
                self.assertEqual(form["code"], ["fixture-code"])
                self.assertTrue(43 <= len(form["code_verifier"][0]) <= 128)
                self.assertNotIn("client_secret", form)
                return {"refresh_token": "fixture-refresh", "access_token": "fixture-access", "scope": " ".join(helper.SCOPES)}
            self.assertEqual(value.full_url, "https://api.spotify.com/v1/me/player")
            return {}

        fixture_root = (Path(__file__).resolve().parent.parent / "firmware/build/spotify-remote-host").resolve()
        assert fixture_root.is_relative_to(Path(__file__).resolve().parent.parent)
        fixture_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=fixture_root) as directory:
            client = Path(directory) / "client.json"
            client.write_text(json.dumps({"client_id": "0123456789abcdef0123456789abcdef", "client_secret": "ignored-fixture-secret"}))
            output = io.StringIO()
            with patch.object(helper.webbrowser, "open", browser), patch.object(helper, "bounded_request", request), contextlib.redirect_stdout(output):
                credentials = helper.authorize(client)
            self.assertEqual(set(credentials), {"client_id", "refresh_token"})
            self.assertEqual(callbacks, [400, 200])
            for private in ("fixture-refresh", "fixture-access", "ignored-fixture-secret", "fixture-code"):
                self.assertNotIn(private, output.getvalue())
        self.assertIsNone(helper.NoRedirect().redirect_request(None))


if __name__ == "__main__":
    unittest.main()
