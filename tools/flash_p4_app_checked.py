"""Check the installed NP2 layout/boot slot before a bounded USB app flash.

Reads only partition metadata, never NVS/storage. Default is check only.
No bootloader, partition table, otadata or C6 writes are supported.
"""
import argparse
import binascii
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys


def active_slot(data):
    if len(data) != 8192:
        raise ValueError("invalid otadata length")
    entries = []
    for offset in (0, 4096):
        sequence, = struct.unpack_from("<I", data, offset)
        state, crc = struct.unpack_from("<II", data, offset + 24)
        expected = binascii.crc32(data[offset:offset + 4], 0xFFFFFFFF) & 0xFFFFFFFF
        if sequence != 0xFFFFFFFF and crc == expected and state not in (3, 4):
            entries.append(sequence)
    if not entries:
        if data == b"\xff" * 8192:
            return 0
        raise ValueError("no valid OTA selection; refusing to guess")
    if len(entries) == 2 and abs(entries[0] - entries[1]) > 0x7FFFFFFF:
        raise ValueError("ambiguous OTA sequence wrap")
    return (max(entries) - 1) % 2


def verify_layout(installed, compiled):
    if len(installed) != 4096 or len(compiled) != 3072 or installed[:3072] != compiled:
        raise ValueError("installed partition table differs from compiled NP2 layout")
    # Check actual entries too, so matching a different build cannot authorize
    # writing an address that belongs to another firmware's user data.
    expected = {b"nvs": (1, 2, 0x11000, 0x8000),
                b"otadata": (1, 0, 0x1B000, 0x2000),
                b"ota_0": (0, 0x10, 0x20000, 0x800000),
                b"ota_1": (0, 0x11, 0x820000, 0x800000),
                b"storage": (1, 0x82, 0x1020000, 0x900000)}
    actual = {}
    for offset in range(0, 3072, 32):
        magic, kind, subtype, start, size, label, _ = struct.unpack_from("<HBBII16sI", installed, offset)
        if magic == 0x50AA:
            actual[label.rstrip(b"\0")] = (kind, subtype, start, size)
    for label, record in expected.items():
        if actual.get(label) != record:
            raise ValueError("unexpected NP2 partition definition")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--port", required=True)
    parser.add_argument("--serial-suffix", required=True)
    parser.add_argument("--flash", action="store_true")
    args = parser.parse_args()
    from serial.tools import list_ports
    ports = [p for p in list_ports.comports() if p.device.upper() == args.port.upper()]
    if len(ports) != 1:
        raise ValueError("expected one attached P4 serial port")
    port = ports[0]
    serial = "".join(c for c in (port.serial_number or "") if c.isalnum())
    if (port.vid, port.pid) != (0x303A, 0x1001) or not serial.lower().endswith(args.serial_suffix.lower()):
        raise ValueError("P4 USB identity does not match the selected board")
    build = args.build.resolve()
    config = (build / "sdkconfig").read_text()
    for required in ('CONFIG_IDF_TARGET="esp32p4"', 'CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y',
                     'CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3', 'CONFIG_LV_COLOR_DEPTH_16=y',
                     'CONFIG_ESP_HOSTED_SDIO_RESET_ACTIVE_LOW=y'):
        if required not in config.splitlines():
            raise ValueError("build configuration is outside the NP2 hardware profile")
    for prohibited in ('CONFIG_SPI_FLASH_AUTO_SUSPEND=y', 'CONFIG_SECURE_BOOT=y',
                       'CONFIG_SECURE_FLASH_ENC_ENABLED=y'):
        if prohibited in config.splitlines():
            raise ValueError("build requires a different hardware/security gate")
    binary = build / "np2_p4.bin"
    if not 0 < binary.stat().st_size < 0x800000:
        raise ValueError("invalid app size")
    def run(name, operation, before="default_reset", after="hard_reset"):
        command = [sys.executable, "-m", "esptool", "--chip", "esp32p4", "--port", args.port,
                   "--baud", "460800", "--before", before, "--after", after, *operation]
        result = subprocess.run(command, capture_output=True, timeout=180)
        (build / name).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f"esptool operation failed; inspect local {name}")
    table = build / "installed-partitions.bin"
    ota = build / "installed-otadata.bin"
    run("layout-read.log", ["read_flash", "0x10000", "0x1000", str(table)])
    verify_layout(table.read_bytes(), (build / "partition_table/partition-table.bin").read_bytes())
    run("ota-selection-read.log", ["read_flash", "0x1b000", "0x2000", str(ota)])
    slot = active_slot(ota.read_bytes())
    address = (0x20000, 0x820000)[slot]
    metadata = {"target": "esp32p4", "slot": slot, "address": hex(address),
                "bytes": binary.stat().st_size, "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "partition_sha256": hashlib.sha256(table.read_bytes()[:3072]).hexdigest(),
                "flashed": False, "verified": False}
    if args.flash:
        (build / "checked-flash.json").write_text(json.dumps(metadata, indent=2) + "\n")
        run("checked-app-flash.log", ["write_flash", "--flash_mode", "dio", "--flash_freq", "80m",
                                     "--flash_size", "32MB", hex(address), str(binary)])
        metadata["flashed"] = True
        (build / "checked-flash.json").write_text(json.dumps(metadata, indent=2) + "\n")
        run("checked-app-verify.log", ["verify_flash", hex(address), str(binary)])
        metadata["verified"] = True
    (build / "checked-flash.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    main()
