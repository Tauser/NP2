"""Bounded COM capture of fixed diagnostics; no raw logs or provider data."""
import argparse
import re
import time
from pathlib import Path
import serial
from pair_spotify_panel import PLAYBACK_RECORDS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=150, choices=range(1, 601))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--reset", action="store_true", help="Reset the P4 through the verified USB serial/JTAG connection before capture")
    args = parser.parse_args()
    patterns = [
        r"np2_spotify_remote: cover width=\d+ height=\d+ bytes=\d+ fallback=[01]",
        r"np2_spotify_remote: admission free=\d+ required=\d+",
        r"np2_spotify_remote: memory min_internal=\d+ largest=\d+ stack_min_bytes=\d+ integrity=\d+",
        r"np2_spotify_remote: (?:http token=\d+ status=-?\d+ result=-?\d+ bytes=\d+|complete action=\d+ result=-?\d+ internal=\d+|credentials completion result=-?\d+)",
        r"np2_connect: (?:station received an IP address|Hosted SDIO transport is up|Hosted SDIO transport is down; local UI remains offline-ready|Hosted startup exceeded 30s; local UI remains offline-ready)",
        r"np2_connect: station (?:authentication rejected|disconnected) \(reason=\d+\); [A-Za-z -]+",
        r"np2_connect: (?:Wi-Fi scan request failed|Hosted recovery request ended): ESP_[A-Z0-9_]+",
        r"np2_connect: Hosted recovery lifecycle completed",
        r"np2_spotify: pair phase=\d+ result=-?\d+ internal=\d+ largest=\d+ stack_min_bytes=\d+ integrity=\d+",
        r"photo_frame: Catalog count=\d+",
        r"photo_frame: Thumbnails ready page=\d+ count=\d+ internal=\d+ psram=\d+",
        r"device_controls: Photo preferences restored idle_ms=\d+ interval_ms=\d+ selected=\d+(?: exit_ms=\d+)?",
        r"device_controls: Photo preferences saved result=-?\d+ selected=\d+ idle_ms=\d+ interval_ms=\d+(?: exit_ms=\d+)?",
        r"photo_frame: Photo ready index=\d+ bytes=\d+ internal=\d+ psram=\d+",
        r"photo_frame: Photo rejected index=\d+ result=-?\d+",
        r"product_ui: Photo frame timeout",
        r"product_ui: Photo frame visible=[01](?: generation=\d+)?",
        r"weather_assets: microSD mounted at /sdcard; animated weather icons (?:ready|unavailable)",
        r"device_controls: Melody complete category=\d+ result=-?\d+",
        r"device_controls: Melody playback failed: ESP_ERR_[A-Z_]+",
        r"np2_music: probe result=-?\d+ pcm=\d+ peak_buffer=\d+ queue_bytes=\d+ internal_observed=\d+ psram_before=\d+ psram_allocated=\d+ psram_after=\d+ stack_min_bytes=\d+ integrity=\d+",
        r"device_controls: Controls restored: brightness=\d+ volume=\d+",
        r"np2_google: c6=\d+\.\d+\.\d+",
        r"np2_google: admission free=\d+ largest=\d+ required=\d+",
        r"np2_google: http endpoint=(?:token|tasks|calendar) method=\d+ status=\d+ result=-?\d+ bytes=\d+",
        r"np2_google: complete result=-?\d+ min_internal=\d+ after=\d+ largest=\d+ stack_min_bytes=\d+ integrity=\d+",
        r"np2_google: internal memory floor violated",
        r"np2_netcheck: HTTPS complete provider=(?:coingecko|open-meteo|bcb|alternative-me|brapi|control|diagnostic-or-update) phase=[a-z-]+ result=ESP_[A-Z0-9_]+ status=\d+ bytes=\d+ tls=0x[0-9a-f]+ code=-?\d+ flags=0x[0-9a-f]+ errno=-?\d+ duration=\d+ms internal_free=\d+ largest=\d+",
        r"np2_netcheck: network work complete mode=\d+ domain=\d+ dns=ESP_[A-Z0-9_]+ ntp=ESP_[A-Z0-9_]+ https=ESP_[A-Z0-9_]+ duration=\d+ms",
        r"np2_agenda: ready=\d+ items=\d+ pending=\d+ linked=\d+ sync_busy=\d+ sync_result=-?\d+ app_stack_min_bytes=\d+",
        r"resources cpu=\d+% cores=\d+/\d+ internal_free=\d+ largest=\d+ psram_free=\d+ uptime=\d+s",
        r"LVGL object pools total=\d+ free=\d+ largest=\d+ PSRAM_added=\d+",
        r"PSRAM=\d+ bytes, free=\d+; internal free=\d+",
        r"App ELF SHA256: [0-9a-f.]+",
        r"ESP-IDF: v5\.5\.4(?:-dirty)?",
        r"target=esp32p4, revision=\d+, cores=\d+",
        r"Display ready[^\r\n]*",
        r"P4 local bring-up ready[^\r\n]*",
        r"(?:failed|unavailable): ESP_ERR_[A-Z_]+",
    ]
    patterns += [pattern.pattern for pattern in PLAYBACK_RECORDS]
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.5)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    with connection, args.output.open("w", encoding="utf-8") as output:
        if args.reset:
            from esptool.reset import HardReset
            HardReset(connection, uses_usb=True).reset()
        start = time.monotonic()
        while time.monotonic() - start < args.seconds:
            line = connection.readline(2048).decode("utf-8", errors="replace")
            record = None
            if re.search(r"Guru Meditation|Stack protection fault|Task watchdog|assert failed|panic'ed", line):
                record = "runtime_fault_detected (raw dump omitted)"
            else:
                for pattern in patterns:
                    match = re.search(pattern, line)
                    if match:
                        record = match.group(0)
                        break
            if record:
                record = f"{time.monotonic() - start:.1f}s {record}"
                print(record, flush=True)
                output.write(record + "\n")
                output.flush()


if __name__ == "__main__":
    main()
