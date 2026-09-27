"""Verify the whole SDIO source, run only the official patch, prove idempotence.

Run with the IDF Python. The upstream reference is downloaded separately and
checked against a pinned SHA-256; no credentials or automatic network access.
"""

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import sys

REFERENCE_SHA256 = "32041dcbbd0e1f4db26af901c68a3e0804b5d01142b291314c5016ca0ea0a6aa"
HOSTED_HASH = "1b1c2aa8f82e0826950ec92ff16fd8f327abd2de6c8a3899301ad8cfb4747879"
SOURCE_PATH = "components/esp_driver_sdio/src/sdio_slave.c"
OLD = 'SDIO_SLAVE_CHECK(len > 0 && len <= 4092, "length out of range: (0, 4092]", ESP_ERR_INVALID_ARG);'
NEW = 'SDIO_SLAVE_CHECK(len > 0, "len <= 0", ESP_ERR_INVALID_ARG);'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalized(data):
    return data.decode("utf-8").replace("\r\n", "\n")


def expected_patch(original):
    if original.count(OLD) != 1 or NEW in original:
        raise ValueError("Reference does not contain exactly the official original guard")
    return original.replace(OLD, NEW)


def verify_source(original, local, allow_unpatched=False):
    patched = expected_patch(original)
    if local != patched and not (allow_unpatched and local == original):
        raise ValueError("SDIO source differs beyond the one official patch; build blocked")
    return patched


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idf-path", required=True, type=Path)
    parser.add_argument("--hosted-path", required=True, type=Path)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    original_bytes = args.reference.read_bytes()
    if digest(original_bytes) != REFERENCE_SHA256:
        raise ValueError("Unexpected upstream reference SHA-256")
    if (args.hosted_path / ".component_hash").read_text().strip() != HOSTED_HASH:
        raise ValueError("Expected pinned ESP-Hosted 3.0.6 component")
    original = normalized(original_bytes)
    source = args.idf_path / SOURCE_PATH
    before = source.read_bytes()
    verify_source(original, normalized(before), allow_unpatched=True)
    patch_tool = args.hosted_path / "tools/eh.py"
    command = [sys.executable, str(patch_tool.resolve()), "patch-idf",
               "--idf-path", str(args.idf_path.resolve())]
    subprocess.run(command, check=True)
    after = source.read_bytes()
    patched = verify_source(original, normalized(after))
    subprocess.run(command, check=True)
    if source.read_bytes() != after:
        raise ValueError("Official patch is not byte-idempotent")
    diff = "".join(difflib.unified_diff(original.splitlines(keepends=True),
                                      patched.splitlines(keepends=True),
                                      fromfile="a/" + SOURCE_PATH,
                                      tofile="b/" + SOURCE_PATH))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "esp-idf-5.5.4-sdio-sw-aggr.patch").write_bytes(diff.encode())
    report = {
        "reference_url": "https://raw.githubusercontent.com/espressif/esp-idf/v5.5.4/" + SOURCE_PATH,
        "source": SOURCE_PATH, "hosted_component_hash": HOSTED_HASH,
        "reference_sha256": digest(original_bytes), "before_raw_sha256": digest(before),
        "after_raw_sha256": digest(after),
        "patched_lf_sha256": digest(patched.encode()),
        "patch_sha256": digest(diff.encode()),
        "patch_tool_sha256": digest(patch_tool.read_bytes()),
        "already_patched": normalized(before) == patched, "idempotent": True,
        "scope": "Whole sdio_slave.c only; not an audit of every SDK file",
    }
    (args.output / "idf-patch-audit.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
