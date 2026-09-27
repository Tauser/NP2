#!/usr/bin/env python3
"""Create a local, non-production P4 OTA artifact set.

The output directory is intentionally expected below ignored artifacts/ or
keys/.  The private key never enters the repository; its public DER companion
is the only material that firmware needs for a laboratory keyring.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path


MANIFEST_BYTES = 104
SIGNATURE_BYTES = 384


def integer(value: str) -> int:
    return int(value, 0)


def command(args: list[str]) -> None:
    subprocess.run(args, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True,
                        help="candidate ESP-IDF app binary")
    parser.add_argument("--output", type=Path, required=True,
                        help="ignored local output directory")
    parser.add_argument("--product-id", type=integer, required=True)
    parser.add_argument("--board-id", type=integer, required=True)
    parser.add_argument("--revision", type=integer, required=True)
    parser.add_argument("--app-version", type=integer, required=True)
    parser.add_argument("--security-version", type=integer, default=0)
    parser.add_argument("--schema", type=integer, default=1)
    parser.add_argument("--key-id", type=integer, default=1)
    parser.add_argument("--openssl", default="openssl")
    parser.add_argument("--private-key", type=Path,
                        help="existing laboratory signing key (never copied into public/)")
    args = parser.parse_args()

    if not args.image.is_file() or any(
        value <= 0 for value in (args.product_id, args.board_id, args.revision,
                                 args.app_version, args.schema, args.key_id)
    ):
        raise ValueError("image and all identity/version arguments must be nonzero")
    if args.revision > 0xFFFF or args.schema > 0xFFFF:
        raise ValueError("revision and schema must fit uint16")
    for value in (args.product_id, args.board_id, args.app_version,
                  args.security_version, args.key_id):
        if not 0 <= value <= 0xFFFFFFFF:
            raise ValueError("numeric argument does not fit uint32")

    openssl = shutil.which(args.openssl) or args.openssl
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    image = args.image.resolve()
    public = output / "public"
    public.mkdir(exist_ok=True)
    candidate = public / "candidate.bin"
    manifest_path = public / "manifest.bin"
    signature_path = public / "manifest.sig"
    private_key = args.private_key.resolve() if args.private_key else output / "development-private.pem"
    if args.private_key and not private_key.is_file():
        raise ValueError("the supplied signing key does not exist")
    if private_key.is_relative_to(public):
        raise ValueError("the signing key must remain outside the public directory")
    public_key = output / "development-public.der"
    shutil.copyfile(image, candidate)
    image_bytes = candidate.read_bytes()
    if not image_bytes or len(image_bytes) > 0xFFFFFFFF:
        raise ValueError("candidate image size is invalid")

    manifest = bytearray(MANIFEST_BYTES)
    struct.pack_into("<IHH", manifest, 0, 0x4E50324D, 1, MANIFEST_BYTES)
    manifest[8:24] = os.urandom(16)
    manifest[24] = 1  # UPDATE_TARGET_ESP32P4
    struct.pack_into("<IIHHIIIHHHHHBB", manifest, 28,
                     args.product_id, args.board_id, args.revision, args.revision,
                     args.app_version, args.security_version, len(image_bytes),
                     args.schema, args.schema,
                     3, 0, 6, 2, 1)  # C6 3.0.6, RPC v2, SW_AGGR
    manifest[64:96] = hashlib.sha256(image_bytes).digest()
    manifest_path.write_bytes(manifest)

    if not private_key.exists():
        command([openssl, "genpkey", "-algorithm", "RSA",
                 "-pkeyopt", "rsa_keygen_bits:3072", "-out", str(private_key)])
    command([openssl, "pkey", "-in", str(private_key), "-pubout", "-outform", "DER",
             "-out", str(public_key)])
    raw_signature = output / ".signature.raw"
    try:
        command([openssl, "dgst", "-sha256", "-sign", str(private_key),
                 "-sigopt", "rsa_padding_mode:pss", "-sigopt", "rsa_pss_saltlen:32",
                 "-sigopt", "rsa_mgf1_md:sha256", "-out", str(raw_signature),
                 str(manifest_path)])
        signature = raw_signature.read_bytes()
    finally:
        raw_signature.unlink(missing_ok=True)
    if len(signature) != SIGNATURE_BYTES:
        raise ValueError("OpenSSL did not produce an RSA-3072 signature")
    signature_path.write_bytes(struct.pack("<I", args.key_id) + signature)

    print(f"candidate={candidate}")
    print(f"manifest={manifest_path}")
    print(f"signature={signature_path}")
    print(f"public_der={public_key}")
    print(f"candidate_sha256={hashlib.sha256(image_bytes).hexdigest()}")
    print(f"serve_only={public}")
    print("private key stays outside public/ and firmware")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
