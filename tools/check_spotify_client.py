"""Check a locally stored Spotify app key; never print or persist its token.

This is a PC prerequisite check, not a Spotify player or user login flow.
"""
import argparse
import base64
import json
import re
import ssl
from pathlib import Path
from urllib import error, request

MAX_RESPONSE = 8192
TOKEN_URL = "https://accounts.spotify.com/api/token"


class NoRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def read_client(path):
    with Path(path).open("rb") as file:
        data = file.read(2049)
    if len(data) > 2048:
        raise ValueError("Invalid client file")
    client = json.loads(data.decode("utf-8-sig"))
    if not isinstance(client, dict) or set(client) != {"client_id", "client_secret"}:
        raise ValueError("Invalid client fields")
    for value in client.values():
        if (not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9_-]{16,256}", value)
                or value.startswith(("COLE_", "YOUR_"))):
            raise ValueError("Invalid client value")
    return client


def check_client(client, opener=None):
    if opener is None:
        opener = request.build_opener(NoRedirect(), request.HTTPSHandler(context=ssl.create_default_context()))
    basic = base64.b64encode((client["client_id"] + ":" + client["client_secret"]).encode()).decode("ascii")
    req = request.Request(TOKEN_URL, data=b"grant_type=client_credentials", method="POST",
        headers={"Authorization": "Basic " + basic, "Content-Type": "application/x-www-form-urlencoded"})
    try:
        with opener.open(req, timeout=8) as response:
            status = response.status
            if status != 200:
                return False, status
            raw = response.read(MAX_RESPONSE + 1)
            if len(raw) > MAX_RESPONSE:
                return False, status
            body = json.loads(raw)
            valid = (isinstance(body, dict) and isinstance(body.get("access_token"), str)
                and 0 < len(body["access_token"]) <= MAX_RESPONSE
                and body.get("token_type", "").lower() == "bearer"
                and type(body.get("expires_in")) is int and 0 < body["expires_in"] <= 86400)
            return valid, status
    except error.HTTPError as failure:
        # Do not read/print the provider's body, Location, URL or exception text.
        status = failure.code
        failure.close()
        return False, status
    except (error.URLError, TimeoutError, OSError, ValueError, TypeError, AttributeError):
        return False, 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--credentials", type=Path,
        default=Path(__file__).resolve().parents[1] / "credentials/spotify-client.json")
    args = parser.parse_args(argv)
    try:
        client = read_client(args.credentials)
    except (OSError, ValueError):
        print("Arquivo local de cliente ausente ou inválido; consulte o exemplo. Não envie segredos no chat.")
        return 1
    valid, status = check_client(client)
    if valid:
        print("Cliente Spotify validado (HTTP 200). Isso ainda não valida login do usuário ou reprodução.")
        return 0
    print(f"Cliente não validado (HTTP {status}). Detalhes e tokens foram omitidos.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
