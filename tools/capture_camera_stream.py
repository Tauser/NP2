"""Bounded COM capture of the live camera session; does not reset the board.

Keeps an allow-list of camera counters, so RTSP headers and credentials can never
reach the output even if a future log line carries them. Panic stack dumps can hold
credentials, so after a fault only the reason and code addresses are kept.
"""
import argparse
import re
import time
from pathlib import Path

import serial

RECORDS = [re.compile(pattern) for pattern in (
    r"camera_stream: Trying RTSP path /stream\d+",
    r"camera_stream: RTSP [A-Z_]+ (?:response status=\d+|rejected \(status=\d+[^)]*\))",
    r"camera_stream: H264 SPS stream=/stream\d+ profile_idc=\d+ constraints=0x[0-9a-f]+ level_idc=\d+",
    r"camera_stream: video [a-z_0-9=\s-]+",
    r"camera_stream: Video not admitted[^\r\n]*",
    r"camera_stream: account save result=-?\d+",
    r"camera_stream: inventory save result=-?\d+",
    r"camera_stream: Camera registration result=\d+",
    r"onvif_discovery: Saved cameras restored count=\d+",
    r"onvif_discovery: Subnet sweep complete hosts=\d+ open=\d+ refused=\d+ silent=\d+ cameras=\d+ ms=\d+",
    r"camera_stream: Camera probe result=\d+",
    r"camera_stream: Frame path: [A-Za-z ]+",
    r"task_wdt: Task watchdog got triggered[^\r\n]*",
    r"task_wdt: CPU \d: [A-Za-z0-9_]+",
    r"camera_stream: PPA conversion failed[^\r\n]*",
    r"camera_stream: PPA wrote \d+ bytes past the end of frame buffer \d+",
    r"camera_stream: video session ended reason=[a-z-]+ detail=-?\d+ after_ms=\d+ lag_ms=-?\d+",
    r"camera_stream: H264 (?:stream uses an unsupported feature|decode error) \(nal_type=\d+\)[^\r\n]*",
    r"camera_stream: RTSP paths reached PLAY[^\r\n]*",
    r"camera_stream: Could not establish[^\r\n]*",
    r"resources cpu=\d+% cores=\d+/\d+ internal_free=\d+ largest=\d+ psram_free=\d+ uptime=\d+s(?: lvgl_stack_min=\d+)?",
)]
FAULT = re.compile(r"Asserted at expression|Guru Meditation|Stack protection fault|Task watchdog|assert failed|abort\(\)|Rebooting")
FAULT_DETAIL = [re.compile(pattern) for pattern in (
    r"Guru Meditation Error: [A-Za-z0-9 ]+panic'ed \([A-Za-z ]+\)",
    r"Stack protection fault",
    r"abort\(\) was called at PC 0x[0-9a-f]+",
    r"Detected in task \"[A-Za-z0-9_ ]+\"",
    r"MEPC\s*:\s*0x[0-9a-f]+",
    r"RA\s*:\s*0x[0-9a-f]+",
    r"MCAUSE\s*:\s*0x[0-9a-f]+",
    r"SP\s*:\s*0x[0-9a-f]+",
    r"Backtrace:(?: 0x[0-9a-f]+(?::0x[0-9a-f]+)?)+",
    r"abort\(\) was called at PC 0x[0-9a-f]+ on core \d",
    r"Core\s+\d register dump:",
    r"Task name : [A-Za-z0-9_ ]+",
    r"Asserted at expression: [^\r\n]*",
    r"assert failed: [^\r\n]*",
    r"lv_[a-z0-9_]+\.c:\d+[^\r\n]*",
)]
FAULT_DETAIL_SECONDS = 8.0
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=300, choices=range(1, 1801))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.5)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    fault = False
    deadline = None
    with connection, args.output.open("w", encoding="utf-8") as output:
        start = time.monotonic()
        while time.monotonic() - start < args.seconds:
            now = time.monotonic()
            if deadline is not None and now > deadline:
                break
            line = ANSI.sub("", connection.readline(2048).decode("utf-8", errors="replace"))
            if deadline is None and FAULT.search(line) is not None:
                fault = True
                deadline = now + FAULT_DETAIL_SECONDS
            patterns = FAULT_DETAIL if fault else RECORDS
            records = [match.group(0).strip() for match in map(lambda p: p.search(line), patterns) if match]
            for record in records:
                text = f"{now - start:.1f}s {record}"
                print(text, flush=True)
                output.write(text + "\n")
                output.flush()
    raise SystemExit(1 if fault else 0)


if __name__ == "__main__":
    main()
