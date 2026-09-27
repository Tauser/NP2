"""Render the local animated Meteocons SVGs into one LVGL frame pack per icon.

Requires Chrome or Edge and Pillow. The output is generated material for
/sdcard/np2/weather/icons; the SVGs in design/clima remain the source of truth.
"""

from __future__ import annotations

import argparse
import base64
import io
import json
import math
import re
import struct
import subprocess
import tempfile
import time
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

import websocket
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "design" / "clima"
OUTPUT = ROOT / "design" / "clima" / "generated-160"
SIZE = 160
FPS = 8
FRAME_MS = 1000 // FPS
HEADER = struct.Struct("<4sHHHHHHII")
NAMES = {
    "clear-day": "np_icon_day_clear",
    "clear-night": "np_icon_night_clear",
    "fog-day": "np_icon_day_fog",
    "fog-night": "np_icon_night_fog",
    "overcast-day": "np_icon_day_variable",
    "overcast-night": "np_icon_night_variable",
    "overcast-day-rain": "np_icon_day_rain_showers",
    "overcast-night-rain": "np_icon_night_rain_showers",
    "overcast-day-snow": "np_icon_day_snow_showers",
    "overcast-night-snow": "np_icon_night_snow_showers",
    "partly-cloudy-day": "np_icon_day_partly_cloudy",
    "partly-cloudy-night": "np_icon_night_partly_cloudy",
    "partly-cloudy-day-drizzle": "np_icon_day_drizzle",
    "partly-cloudy-night-drizzle": "np_icon_night_drizzle",
    "partly-cloudy-day-rain": "np_icon_day_rain",
    "partly-cloudy-night-rain": "np_icon_night_rain",
    "partly-cloudy-day-snow": "np_icon_day_snow",
    "partly-cloudy-night-snow": "np_icon_night_snow",
    "thunderstorms-day-rain": "np_icon_day_thunderstorm",
    "thunderstorms-night-rain": "np_icon_night_thunderstorm",
}


class Chrome:
    def __enter__(self):
        self.profile = tempfile.TemporaryDirectory(prefix="np2_weather_chrome_",
                                                    ignore_cleanup_errors=True)
        candidates = [
            Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
            Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
        ]
        executable = next((path for path in candidates if path.exists()), None)
        if executable is None:
            raise RuntimeError("Chrome or Edge is required to render animated SVG")
        self.process = subprocess.Popen(
            [str(executable), "--headless=new", "--no-first-run",
             "--disable-background-networking", "--disable-extensions",
             "--remote-allow-origins=*", "--remote-debugging-port=0",
             f"--user-data-dir={self.profile.name}", "about:blank"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        port_file = Path(self.profile.name) / "DevToolsActivePort"
        for _ in range(100):
            if port_file.exists():
                break
            if self.process.poll() is not None:
                raise RuntimeError("Headless browser exited before DevTools started")
            time.sleep(0.1)
        else:
            raise RuntimeError("Timed out waiting for headless browser")
        port = port_file.read_text().splitlines()[0]
        targets = json.load(urllib.request.urlopen(f"http://127.0.0.1:{port}/json/list"))
        page = next(target for target in targets if target["type"] == "page")
        self.ws = websocket.create_connection(page["webSocketDebuggerUrl"], timeout=20)
        self.serial = 0
        self.call("Page.enable")
        self.call("Runtime.enable")
        self.call("Emulation.setDeviceMetricsOverride", {
            "width": SIZE, "height": SIZE, "deviceScaleFactor": 1, "mobile": False,
        })
        self.call("Emulation.setDefaultBackgroundColorOverride", {
            "color": {"r": 0, "g": 0, "b": 0, "a": 0},
        })
        return self

    def __exit__(self, *_):
        try:
            self.call("Browser.close")
        except (OSError, RuntimeError, websocket.WebSocketException):
            pass
        self.ws.close()
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
        self.profile.cleanup()

    def call(self, method, params=None):
        self.serial += 1
        request_id = self.serial
        self.ws.send(json.dumps({"id": request_id, "method": method, "params": params or {}}))
        while True:
            response = json.loads(self.ws.recv())
            if response.get("id") != request_id:
                continue
            if "error" in response:
                raise RuntimeError(f"DevTools {method}: {response['error']}")
            return response.get("result", {})

    def load_svg(self, source):
        svg = source.read_text(encoding="utf-8")
        html = ("<!doctype html><meta charset=utf-8><style>html,body{margin:0;"
                "background:transparent;overflow:hidden}svg{display:block;"
                f"width:{SIZE}px;height:{SIZE}px</style>" + svg)
        uri = "data:text/html;charset=utf-8," + urllib.parse.quote(html)
        self.call("Page.navigate", {"url": uri})
        for _ in range(100):
            value = self.call("Runtime.evaluate", {
                "expression": "document.readyState==='complete' && !!document.querySelector('svg')",
                "returnByValue": True,
            }).get("result", {}).get("value")
            if value:
                self.call("Runtime.evaluate", {
                    "expression": "document.querySelector('svg').pauseAnimations()",
                })
                return
            time.sleep(0.02)
        raise RuntimeError(f"SVG did not load: {source}")

    def frame(self, seconds):
        self.call("Runtime.evaluate", {
            "expression": f"document.querySelector('svg').setCurrentTime({seconds:.6f});"
                          "document.querySelector('svg').getBoundingClientRect().width",
            "returnByValue": True,
        })
        png = base64.b64decode(self.call("Page.captureScreenshot", {
            "format": "png", "fromSurface": True, "captureBeyondViewport": False,
        })["data"])
        with Image.open(io.BytesIO(png)) as image:
            if image.size != (SIZE, SIZE):
                raise RuntimeError(f"Unexpected rendered size: {image.size}")
            return image.convert("RGBA").tobytes("raw", "BGRA")


def cycle_seconds(source):
    root = ET.parse(source).getroot()
    durations = set()
    for node in root.iter():
        value = node.get("dur")
        if value and re.fullmatch(r"\d+(?:\.\d+)?s", value):
            durations.add(float(value[:-1]))
    if not durations:
        return 1
    # These supplied icons use whole-second SMIL cycles (1, 2, 3, 6).
    if any(not duration.is_integer() for duration in durations):
        raise ValueError(f"Unsupported fractional cycle in {source}")
    return math.lcm(*(int(duration) for duration in durations))


def build_one(browser, stem, output):
    source = SOURCE / f"{stem}.svg"
    period = cycle_seconds(source)
    frame_count = period * FPS
    if frame_count > 48:
        raise ValueError(f"Too many frames in {source}: {frame_count}")
    browser.load_svg(source)
    payload = bytearray()
    for index in range(frame_count):
        payload.extend(browser.frame(index / FPS))
    if len(payload) != frame_count * SIZE * SIZE * 4:
        raise AssertionError("Incorrect pixel count")
    checksum = zlib.crc32(payload)
    header = HEADER.pack(b"NPWI", 1, SIZE, SIZE, frame_count, FRAME_MS, 0,
                         len(payload), checksum)
    output.write_bytes(header + payload)
    print(f"{source.name} -> {output.name}: {frame_count} frames, {len(payload):,} B, CRC {checksum:08x}")
    return {"source": source.name, "file": output.name, "frames": frame_count,
            "duration_ms": period * 1000, "crc32": f"{checksum:08x}"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", choices=NAMES, help="Render one icon for development")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    names = [args.only] if args.only else list(NAMES)
    with Chrome() as browser:
        manifest = [build_one(browser, stem, args.output / f"{NAMES[stem]}.bin")
                    for stem in names]
    (args.output / "manifest.json").write_text(
        json.dumps({"format": "NPWI v1", "size": SIZE, "fps": FPS,
                    "icons": manifest}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
