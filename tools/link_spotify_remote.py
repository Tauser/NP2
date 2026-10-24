"""Local PKCE authorization + physically armed USB handoff. Never prints tokens."""
import argparse
import base64
import hashlib
import http.server
import json
import re
import secrets
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path

SCOPES = ("user-read-playback-state", "user-modify-playback-state")
REDIRECT = "http://127.0.0.1:8765/callback"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *_args, **_kwargs):
        return None


def bounded_request(request):
    with urllib.request.build_opener(NoRedirect).open(request, timeout=20) as response:
        raw = response.read(16385)
        if len(raw) > 16384:
            raise ValueError("Resposta excedeu o limite.")
        return json.loads(raw) if raw else {}


def authorize(client_path):
    with Path(client_path).open("rb") as source:
        raw = source.read(4097)
    if len(raw) > 4096:
        raise ValueError("Arquivo do cliente excede o limite.")
    client_id = json.loads(raw).get("client_id", "")
    if not re.fullmatch(r"[a-fA-F0-9]{32}", client_id):
        raise ValueError("Client ID inválido no arquivo local.")
    verifier, state = secrets.token_urlsafe(64), secrets.token_urlsafe(32)
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).rstrip(b"=").decode("ascii")
    callback = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass

        def do_GET(self):
            if len(self.path) > 4096:
                self.send_error(400)
                return
            parsed = urllib.parse.urlsplit(self.path)
            fields = urllib.parse.parse_qs(parsed.query, max_num_fields=12)
            if parsed.path != "/callback" or fields.get("state") != [state]:
                self.send_error(400)
                return
            if "error" in fields:
                callback["error"] = True
            elif len(fields.get("code", [])) == 1:
                callback["code"] = fields["code"][0]
            else:
                self.send_error(400)
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Referrer-Policy", "no-referrer")
            self.end_headers()
            self.wfile.write("Autorização recebida. Volte ao Codex e vincule pelo painel.".encode())

    with http.server.HTTPServer(("127.0.0.1", 8765), Handler) as server:
        server.timeout = 0.5
        query = urllib.parse.urlencode({"client_id": client_id, "response_type": "code",
            "redirect_uri": REDIRECT, "scope": " ".join(SCOPES), "state": state,
            "code_challenge": challenge, "code_challenge_method": "S256"})
        print("Autorize o controle remoto no navegador. O Client Secret não é usado.", flush=True)
        if not webbrowser.open("https://accounts.spotify.com/authorize?" + query):
            raise RuntimeError("Não foi possível abrir o navegador.")
        deadline = time.monotonic() + 600
        while not callback and time.monotonic() < deadline:
            server.handle_request()
    if "code" not in callback:
        raise ValueError("Autorização cancelada ou expirada.")
    body = urllib.parse.urlencode({"client_id": client_id, "grant_type": "authorization_code",
        "code": callback.pop("code"), "code_verifier": verifier, "redirect_uri": REDIRECT}).encode("ascii")
    token = bounded_request(urllib.request.Request("https://accounts.spotify.com/api/token", data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"}, method="POST"))
    refresh = token.get("refresh_token")
    if not isinstance(refresh, str) or not 0 < len(refresh) <= 512 or not all(33 <= ord(c) <= 126 for c in refresh):
        raise ValueError("Refresh token ausente ou incompatível.")
    if not set(SCOPES) <= set(token.get("scope", "").split()):
        raise ValueError("Permissões de leitura e controle não foram concedidas.")
    # Validate official player access now, with a read-only request. No body logs.
    try:
        bounded_request(urllib.request.Request("https://api.spotify.com/v1/me/player",
            headers={"Authorization": "Bearer " + token["access_token"]}))
    except urllib.error.HTTPError as error:
        raise ValueError(f"A API de controle recusou o teste de leitura (HTTP {error.code}). Confira os usuários autorizados do app.") from None
    token.clear()
    return {"client_id": client_id, "refresh_token": refresh}


def provision(port, credentials, seconds):
    import serial
    from link_google_agenda import write_bounded_command
    payload = json.dumps(credentials, separators=(",", ":"), ensure_ascii=True).encode("ascii")
    if len(payload) > 768:
        raise ValueError("Credencial excede o limite USB.")
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.5, write_timeout=5)
    connection.dtr = connection.rts = False
    connection.port = port
    print("Autorização validada. No painel: Home → Spotify → Opções → Vincular conta. Aguardando USB...", flush=True)
    acknowledged = False
    committed = False
    with connection:
        connection.reset_input_buffer()
        deadline, next_send = time.monotonic() + seconds, 0
        while time.monotonic() < deadline:
            if not acknowledged and time.monotonic() >= next_send:
                write_bounded_command(connection, b"SPOTIFY_AUTH " + payload + b"\n")
                next_send = time.monotonic() + 2
            line = connection.readline(2048)
            if line.strip() == b"OK":
                acknowledged = True
                print("Recebido pelo painel; aguardando confirmação da gravação.", flush=True)
            match = re.search(rb"np2_spotify_remote: credentials completion result=(-?\d+)", line)
            if match:
                if int(match[1]) != 0:
                    raise RuntimeError("Falha ao persistir a credencial no painel.")
                committed = True
            if committed and acknowledged:
                print("Conta vinculada e persistida. Abra Spotify no painel para controlar.", flush=True)
                return
        raise RuntimeError("Janela expirou sem confirmação de gravação; não considere a vinculação concluída.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, default=Path("credentials/spotify-client.json"))
    parser.add_argument("--port", default="COM8")
    parser.add_argument("--seconds", type=int, default=1200, choices=range(30, 3601))
    args = parser.parse_args()
    credentials = None
    try:
        credentials = authorize(args.client)
        provision(args.port, credentials, args.seconds)
    except urllib.error.HTTPError as error:
        print(f"Falha na autorização Spotify (HTTP {error.code}); corpo e URL omitidos.", flush=True)
        return 1
    except (ValueError, RuntimeError) as error:
        print(str(error), flush=True)
        return 1
    except Exception:
        print("Falha local de navegador, rede ou USB; detalhes privados omitidos.", flush=True)
        return 1
    finally:
        if credentials:
            credentials.clear()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
