"""One-time, local Google OAuth + physically armed NP2 USB provisioning.

No credentials are written by this program or printed. Supply the Desktop
OAuth JSON from a local ignored credentials/ directory. Uses Python stdlib and
pyserial (included in the ESP-IDF environment).
"""
import argparse
import base64
import hashlib
import http.server
import json
import secrets
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path

SCOPES = (
    "openid",
    "https://www.googleapis.com/auth/calendar.events",
    "https://www.googleapis.com/auth/tasks",
)


def authorize(client_path):
    raw = Path(client_path).read_bytes()
    if len(raw) > 16384:
        raise ValueError("Arquivo OAuth excede 16 KiB.")
    installed = json.loads(raw).get("installed")
    if not isinstance(installed, dict):
        raise ValueError("Use um cliente OAuth do tipo App para computador.")
    client_id, client_secret = installed["client_id"], installed["client_secret"]
    if not client_id.endswith(".apps.googleusercontent.com"):
        raise ValueError("Cliente OAuth inválido.")
    verifier, state = secrets.token_urlsafe(64), secrets.token_urlsafe(32)
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).rstrip(b"=").decode("ascii")
    callback = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_args):
            pass  # callback URLs contain authorization codes

        def do_GET(self):
            parsed = urllib.parse.urlsplit(self.path)
            fields = urllib.parse.parse_qs(parsed.query, max_num_fields=12)
            if parsed.path != "/" or fields.get("state") != [state]:
                self.send_error(400, "Callback invalido")
                return
            if "error" in fields:
                callback["error"] = True
            elif len(fields.get("code", [])) == 1:
                callback["code"] = fields["code"][0]
            else:
                self.send_error(400, "Codigo ausente")
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write("Autorização recebida. Volte ao terminal do vinculador.".encode())

    with http.server.HTTPServer(("127.0.0.1", 0), Handler) as server:
        server.timeout = 1
        redirect = f"http://127.0.0.1:{server.server_port}/"
        query = urllib.parse.urlencode({
            "client_id": client_id, "redirect_uri": redirect,
            "response_type": "code", "scope": " ".join(SCOPES),
            "state": state, "code_challenge": challenge,
            "code_challenge_method": "S256", "access_type": "offline",
            "prompt": "consent",
        })
        print("Autorize Calendar e Tasks no navegador aberto pelo vinculador.")
        if not webbrowser.open("https://accounts.google.com/o/oauth2/v2/auth?" + query):
            raise RuntimeError("Não foi possível abrir o navegador padrão.")
        deadline = time.monotonic() + 180
        while not callback and time.monotonic() < deadline:
            server.handle_request()
    if "code" not in callback:
        raise RuntimeError("Autorização cancelada ou expirada.")
    body = urllib.parse.urlencode({
        "client_id": client_id, "client_secret": client_secret,
        "code": callback.pop("code"), "code_verifier": verifier,
        "redirect_uri": redirect, "grant_type": "authorization_code",
    }).encode("ascii")
    request = urllib.request.Request("https://oauth2.googleapis.com/token", data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"}, method="POST")
    # Reject redirects rather than forwarding a secret-bearing token request.
    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *_args, **_kwargs):
            return None
    with urllib.request.build_opener(NoRedirect).open(request, timeout=30) as response:
        raw = response.read(16385)
    if len(raw) > 16384:
        raise RuntimeError("Resposta OAuth excedeu o limite.")
    token = json.loads(raw)
    if not token.get("refresh_token"):
        raise RuntimeError("Google não forneceu refresh token. Revogue a autorização NP2 e vincule novamente.")
    granted = set(token.get("scope", "").split())
    if not set(SCOPES) <= granted:
        raise RuntimeError("Autorize as duas permissões solicitadas: Calendar e Tasks.")
    # This ID token came directly from Google's verified HTTPS token endpoint.
    # The subject is only an account boundary, never an offline JWT login.
    identity = token.get("id_token", "").split(".")
    if len(identity) != 3:
        raise RuntimeError("Google não retornou a identificação OpenID da conta.")
    claims = json.loads(base64.urlsafe_b64decode(identity[1] + "=" * (-len(identity[1]) % 4)))
    if claims.get("aud") != client_id or claims.get("iss") not in ("https://accounts.google.com", "accounts.google.com"):
        raise RuntimeError("Identificação da conta incompatível com o cliente OAuth.")
    subject = claims.get("sub")
    if not isinstance(subject, str) or not subject or len(subject) > 64:
        raise RuntimeError("Identificação OpenID inválida.")
    return {"client_id": client_id, "client_secret": client_secret, "refresh_token": token["refresh_token"], "subject": subject}


def write_bounded_command(connection, command):
    # IDF's USB/JTAG RX ring is 256 bytes and its ISR drops excess packets.
    # Leave time for the one-byte firmware consumer rather than bursting JSON.
    for offset in range(0, len(command), 32):
        chunk = command[offset:offset + 32]
        if connection.write(chunk) != len(chunk):
            raise RuntimeError("Envio USB incompleto. Reabra a janela no painel e tente novamente.")
        connection.flush()
        time.sleep(0.05)


def confirm_link(connection):
    # Every maintenance command consumes the physical window, including AUTH.
    # A fresh touch is required for the separate read-only commit confirmation.
    input("No painel, toque no status Google da Agenda > Liberar USB; depois pressione Enter aqui para confirmar. ")
    time.sleep(0.7)
    connection.reset_input_buffer()
    write_bounded_command(connection, b"GOOGLE_STATUS\n")
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        line = connection.readline(1024).strip()
        if line == b"GOOGLE_LINKED":
            print("Conta vinculada e credenciais persistidas no painel.")
            return
        if line == b"GOOGLE_UNLINKED":
            raise RuntimeError("Painel ainda não confirmou a gravação. Repita apenas a consulta com --check-link.")
    raise RuntimeError("Consulta USB sem resposta. Reabra a janela física e use --check-link; não considere a conta vinculada.")


def provision(port, credentials=None):
    import serial
    payload = None
    if credentials is not None:
        payload = json.dumps(credentials, separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if len(payload) > 1536:
            raise ValueError("Credenciais excedem o limite do painel.")
        limits = {"client_id": 192, "client_secret": 96, "refresh_token": 512, "subject": 64}
        if any(not isinstance(credentials.get(key), str) or not 0 < len(credentials[key]) <= limit
               for key, limit in limits.items()):
            raise ValueError("Formato ou tamanho das credenciais incompatível com o painel.")
        input("No painel, toque no ícone de conectar Google no topo da Agenda; depois pressione Enter aqui. ")
    connection = serial.Serial(port=None, baudrate=115200, timeout=1, write_timeout=5)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    with connection:
        if payload is None:
            confirm_link(connection)
            return
        time.sleep(0.7)  # physical provisioner deliberately drains stale input first
        connection.reset_input_buffer()
        write_bounded_command(connection, b"GOOGLE_AUTH " + payload + b"\n")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            line = connection.readline(1024).strip()
            if line == b"OK":
                print("Credencial recebida; falta confirmar a gravação.")
                confirm_link(connection)
                return
            if line.startswith(b"ERR "):
                raise RuntimeError("Painel recusou a vinculação. Confira a janela USB e se a conta já está vinculada.")
    raise RuntimeError("Não houve ACK USB. A gravação pode ter ocorrido: use --check-link antes de repetir OAuth; não considere o vínculo confirmado sem essa consulta.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-secrets", type=Path)
    parser.add_argument("--port", required=True)
    parser.add_argument("--check-link", action="store_true", help="Consulta a gravação sem nova autorização Google.")
    args = parser.parse_args()
    if not args.check_link and args.client_secrets is None:
        parser.error("Informe --client-secrets ou use --check-link.")
    credentials = None
    try:
        if not args.check_link:
            credentials = authorize(args.client_secrets)
        provision(args.port, credentials)
        print("Vinculação confirmada. A sincronização depende também da rede e da margem de memória do firmware.")
    except urllib.error.HTTPError as exc:
        raise SystemExit(f"Google recusou a autorização (HTTP {exc.code}); nenhum corpo de erro foi exibido.") from None
    except (OSError, ValueError, KeyError, RuntimeError) as exc:
        # Do not render provider bodies, URLs, authorization codes or tokens.
        if isinstance(exc, (ValueError, KeyError, OSError)):
            raise SystemExit("Falha no arquivo OAuth, na conexão ou nos dados recebidos. Confira a configuração local.") from None
        raise SystemExit(str(exc)) from None
    finally:
        if credentials is not None:
            credentials.clear()


if __name__ == "__main__":
    main()
