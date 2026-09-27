"""Check the effective C6 laboratory recipe and record artifact identities."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

import yaml

HOSTED_HASH = "1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879"
REQUIRED = {
    "CONFIG_IDF_TARGET": '"esp32c6"',
    "CONFIG_APP_REPRODUCIBLE_BUILD": "y",
    "CONFIG_ESPTOOLPY_FLASHSIZE_4MB": "y",
    "CONFIG_ESP_HOSTED": "y",
    "CONFIG_ESP_HOSTED_CP": "y",
    "CONFIG_ESP_HOSTED_CP_FOR_MCU": "y",
    "CONFIG_ESP_HOSTED_CP_RPC_V2": "y",
    "CONFIG_ESP_HOSTED_CP_FEAT_WIFI": "y",
    "CONFIG_EH_TRANSPORT_CP_SDIO": "y",
    "CONFIG_EH_TRANSPORT_CP_SDIO_MODE_SW_AGGR": "y",
    "CONFIG_PARTITION_TABLE_CUSTOM": "y",
}
DISABLED = (
    "CONFIG_SECURE_BOOT", "CONFIG_SECURE_FLASH_ENC_ENABLED",
    "CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK", "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE",
    "CONFIG_BT_ENABLED", "CONFIG_SPI_FLASH_AUTO_SUSPEND",
    "CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT",
)


def config_values(text):
    values = {}
    for line in text.splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            values[line[2:-11]] = "n"
    return values


def check_config(values):
    for key, expected in REQUIRED.items():
        if values.get(key) != expected:
            raise ValueError(f"Effective C6 configuration mismatch: {key}")
    for key in DISABLED:
        if values.get(key, "n") != "n":
            raise ValueError(f"Unexpected feature enabled: {key}")


def partitions(blob):
    entries = []
    for offset in range(0, len(blob) - 31, 32):
        magic, kind, subtype, start, size, label, flags = struct.unpack_from("<HBBII16sI", blob, offset)
        if magic != 0x50AA:
            break
        entries.append(dict(type=kind, subtype=subtype, offset=start, size=size,
                            label=label.rstrip(b"\0").decode("ascii"), flags=flags))
    expected = [
        ("nvs", 1, 2, 0x9000, 0x4000), ("otadata", 1, 0, 0xD000, 0x2000),
        ("phy_init", 1, 1, 0xF000, 0x1000),
        ("ota_0", 0, 0x10, 0x10000, 0x1C0000),
        ("ota_1", 0, 0x11, 0x1D0000, 0x1C0000),
    ]
    actual = [(e["label"], e["type"], e["subtype"], e["offset"], e["size"]) for e in entries]
    if actual != expected or any(e["flags"] != 0 for e in entries):
        raise ValueError("Generated C6 partition table differs from the audited layout")
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    description = json.loads((build / "project_description.json").read_text())
    if description["target"] != "esp32c6":
        raise ValueError("Expected C6 build")
    config = Path(description["config_file"])
    values = config_values(config.read_text())
    check_config(values)
    lock_path = Path(description["project_path"]) / "dependencies.lock"
    lock = yaml.safe_load(lock_path.read_text())
    hosted = lock["dependencies"]["espressif/esp_hosted"]
    if hosted["version"] != "3.0.6" or hosted["component_hash"] != HOSTED_HASH:
        raise ValueError("Unexpected Hosted resolution")
    if lock["dependencies"]["idf"]["version"] != "5.5.4" or lock["target"] != "esp32c6":
        raise ValueError("Unexpected IDF version/lock target")
    table = build / "partition_table/partition-table.bin"
    entries = partitions(table.read_bytes())
    image = build / description["app_bin"]
    size = image.stat().st_size
    if not 0 < size <= min(0x1C0000, 0x200000):
        raise ValueError("App does not fit both C6 slots and P4 staging")
    artifacts = {
        "app": image, "elf": Path(description["app_elf"]),
        "bootloader": build / "bootloader/bootloader.bin", "partition_table": table,
        "sdkconfig": config, "lock": lock_path,
    }
    hashes = {}
    for name, path in artifacts.items():
        if not path.is_absolute():
            path = build / path
        hashes[name] = {"file": path.name, "bytes": path.stat().st_size,
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    report = {
        "target": "esp32c6", "idf": "5.5.4", "hosted": "3.0.6",
        "artifacts": hashes, "partitions": entries,
        "slot_free_bytes": 0x1C0000 - size,
        "staging_free_bytes": 0x200000 - size,
        "effective_config": {key: values.get(key, "n") for key in (*REQUIRED, *DISABLED)},
        "installed": False, "signed": False,
        "activation_gate": "BLOCKED: installed C6 bootloader/layout and autonomous recovery unproven",
    }
    (build / "build-audit.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
