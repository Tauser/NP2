"""Private, physically armed USB handoff for the bounded pairing laboratory.

Only fixed diagnostics are printed/saved. Never echo device input, JSON,
account names, application keys, tokens, URLs or exception strings.
"""
import argparse
import json
import re
import time
from pathlib import Path
import serial
from check_spotify_client import read_client

PHASE = re.compile(r"np2_spotify: pair phase=(\d+) result=(-?\d+) internal=(\d+) largest=(\d+) stack_min_bytes=(\d+) integrity=([01])")
NETWORK = re.compile(r"np2_netcheck: network work complete mode=\d+ domain=\d+ dns=ESP_[A-Z0-9_]+ ntp=ESP_[A-Z0-9_]+ https=ESP_[A-Z0-9_]+ duration=\d+ms")
PHASES = {1:"iniciando",2:"app verificado",3:"servidor Spotify autenticado",4:"descoberta ativa",
          5:"pareamento recebido",6:"conta autenticada",7:"janela encerrada",8:"falha no ensaio",
          9:"sessão Connect ativa",10:"reprodução encerrada"}
PLAYBACK_RECORDS = [
    re.compile(r"np2_spotify: media stage=\d+ value=-?\d+ count=\d+"),
    re.compile(r"np2_spotify: socket operation=\d+ code=-?\d+"),
    re.compile(r"np2_spotify: failure stage=\d+ reason=\d+"),
    re.compile(r"np2_spotify: decoder result=\d+ bytes=\d+ mono=\d+ peak_bytes=\d+ blocks=\d+ peak_sample=\d+ crossings=\d+ stack=\d+ integrity=[01]"),
    re.compile(r"np2_spotify: http count=\d+ kind=\d+ status=\d+ result=-?\d+ bytes=\d+ ms=\d+ before=\d+ min_internal=\d+ network_stack=\d+"),
    re.compile(r"np2_spotify: pcm queued=\d+ consumed=\d+ buffered=\d+ paused=[01] starved_polls=\d+ internal=\d+ largest=\d+ decoder_stack=\d+ worker_stack=\d+ integrity=[01]")]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True)
    parser.add_argument("--credentials",type=Path,default=Path(__file__).resolve().parents[1]/"credentials/spotify-client.json")
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--seconds",type=int,default=300,choices=range(30,3601))
    parser.add_argument("--playback",action="store_true",help="Keep capturing after account authentication for the opt-in playback image")
    parser.add_argument("--monitor-only",action="store_true",help="Capture an already active session without reading/sending app credentials")
    args=parser.parse_args()
    payload=bytearray()
    try:
        if not args.monitor_only:
            client=read_client(args.credentials)
            if any(not re.fullmatch(r"[0-9a-fA-F]{32}",value) for value in client.values()):
                raise ValueError("Invalid laboratory app format")
            payload.extend(b"SPOTIFY_PAIR "+json.dumps(client,separators=(",",":")).encode()+b"\n")
            del client
        connection=serial.Serial(port=None,baudrate=115200,timeout=0.3,write_timeout=2)
        connection.dtr=False;connection.rts=False;connection.port=args.port
        accepted=args.monitor_only;next_send=0;phase=0;pcm_observed=False
        with connection, args.output.open("w",encoding="utf-8") as output:
            start=time.monotonic()
            print("Capturando sessão existente; nenhum segredo será exibido." if args.monitor_only else
                  "Aguardando liberação física da USB; nenhum segredo será exibido.",flush=True)
            while time.monotonic()-start<args.seconds:
                now=time.monotonic()
                if not accepted and now>=next_send:
                    connection.write(payload);connection.flush();next_send=now+2
                line=connection.readline(2048).decode("utf-8",errors="replace")
                match=PHASE.search(line)
                if match:
                    accepted=True;phase=int(match[1]);record=f"{now-start:.1f}s {match.group(0)}"
                    output.write(record+"\n");output.flush()
                    print(record+" | "+PHASES.get(phase,"etapa desconhecida"),flush=True)
                    if phase in ((7,8,10) if args.playback else (6,7,8)):
                        return 0 if phase in (6,10) else 2
                elif any(pattern.search(line) for pattern in PLAYBACK_RECORDS):
                    record=next(pattern.search(line).group(0) for pattern in PLAYBACK_RECORDS if pattern.search(line))
                    record=f"{now-start:.1f}s {record}"
                    output.write(record+"\n");output.flush();print(record,flush=True)
                    consumed=re.search(r"pcm queued=\d+ consumed=(\d+)",record)
                    if consumed and int(consumed[1])>=44100: pcm_observed=True
                elif line.strip()=="OK": accepted=True;print("Solicitação privada aceita pelo painel.",flush=True)
                elif NETWORK.search(line):
                    record=f"{now-start:.1f}s {NETWORK.search(line).group(0)}"
                    output.write(record+"\n");output.flush();print(record,flush=True)
                elif line.strip().startswith("ERR ESP_ERR_"):
                    print("Solicitação recusada pelo painel; detalhes privados omitidos.",flush=True);return 2
                elif re.search(r"Guru Meditation|Stack protection fault|Task watchdog|assert failed|panic'ed",line):
                    print("runtime_fault_detected (raw dump omitted)",flush=True);output.write("runtime_fault_detected\n");return 3
            if pcm_observed:
                print("Captura encerrada com PCM consumido pelo owner de áudio; audibilidade depende da confirmação do operador.",flush=True)
                return 0
            print("Tempo limite do ensaio; etapa solicitada ainda não confirmada.",flush=True)
            return 2
    except (OSError,ValueError,serial.SerialException):
        print("Não foi possível executar o ensaio USB. Conteúdo privado e detalhes da exceção omitidos.",flush=True)
        return 1
    finally:
        payload[:]=b"\0"*len(payload)


if __name__=="__main__": raise SystemExit(main())
