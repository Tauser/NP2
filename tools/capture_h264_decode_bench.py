"""Bounded COM capture of the decode bench; keeps its counters and boot identity only."""
import argparse
import re
import time
from pathlib import Path

import serial

RECORDS = [re.compile(pattern) for pattern in (
    r"h264_bench: [^\r\n]*",
    r"App ELF SHA256: [0-9a-f.]+",
    r"ESP-IDF: v5\.5\.4(?:-dirty)?",
    r"target=esp32p4, revision=\d+, cores=\d+",
    r"PSRAM=\d+ bytes, free=\d+; internal free=\d+",
    r"Display ready[^\r\n]*",
    r"P4 local bring-up ready[^\r\n]*",
    r"resources cpu=\d+% cores=\d+/\d+ internal_free=\d+ largest=\d+ psram_free=\d+ uptime=\d+s",
)]
FAULT = re.compile(r"Guru Meditation|Stack protection fault|Task watchdog|assert failed|abort\(\)|panic'ed")
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=600, choices=range(1, 901))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.5)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    finished = False
    with connection, args.output.open("w", encoding="utf-8") as output:
        from esptool.reset import HardReset
        HardReset(connection, uses_usb=True).reset()
        start = time.monotonic()
        while time.monotonic() - start < args.seconds and not finished:
            line = ANSI.sub("", connection.readline(2048).decode("utf-8", errors="replace"))
            record = None
            if FAULT.search(line):
                record = "runtime_fault_detected (raw dump omitted)"
            else:
                for pattern in RECORDS:
                    match = pattern.search(line)
                    if match:
                        record = match.group(0)
                        break
            if record:
                record = f"{time.monotonic() - start:.1f}s {record}"
                print(record, flush=True)
                output.write(record + "\n")
                output.flush()
                finished = "h264_bench: done" in record
    if not finished:
        raise SystemExit("bench did not report completion within the capture window")


if __name__ == "__main__":
    main()
