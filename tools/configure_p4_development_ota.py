#!/usr/bin/env python3
"""Generate a public-only, opt-in P4 laboratory OTA build header.

Pass its absolute path to CMake with -DNP2_OTA_DEVELOPMENT_HEADER=...
Serve only public/ from create_p4_development_ota.py, never the key directory.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import urlsplit


def endpoints(base_url):
    if not re.fullmatch(r"https://[a-z0-9.-]+(?:/[A-Za-z0-9_./-]*)?", base_url):
        raise ValueError("expected an HTTPS origin/path without credentials, port, query or escapes")
    parsed = urlsplit(base_url)
    host = parsed.hostname
    if not host or len(host) > 96 or any(not label or len(label) > 63 or
            label.startswith('-') or label.endswith('-') for label in host.split('.')):
        raise ValueError("invalid host")
    if any(part in ('.', '..') for part in parsed.path.split('/')):
        raise ValueError("relative path segments are not allowed")
    urls = [base_url.rstrip('/') + '/' + name
            for name in ('manifest.bin', 'manifest.sig', 'candidate.bin')]
    if any(len(url) >= 192 for url in urls):
        raise ValueError("endpoint exceeds firmware limit")
    return host, urls


def render(base_url, der, product_id, board_id, key_id):
    host, urls = endpoints(base_url)
    if not 1 <= len(der) <= 1024 or any(not 1 <= value <= 0xffffffff
                                       for value in (product_id, board_id, key_id)):
        raise ValueError("invalid public key size or identity")
    byte_rows = [', '.join(f'0x{byte:02x}' for byte in der[i:i + 16])
                 for i in range(0, len(der), 16)]
    return '\n'.join([
        '/* Generated laboratory configuration: public data only. */', '#pragma once',
        f'#define NP2_DEVELOPMENT_PRODUCT_ID UINT32_C({product_id})',
        f'#define NP2_DEVELOPMENT_BOARD_ID UINT32_C({board_id})',
        'static const uint8_t s_development_public_der[] = {',
        ',\n'.join('    ' + row for row in byte_rows), '};',
        'static const update_keyring_entry_t s_development_keys[] = {{',
        f'    .trusted_key = {{.key_id = {key_id}U,',
        '        .public_key_der = s_development_public_der,',
        '        .public_key_der_bytes = sizeof(s_development_public_der)},',
        '    .enabled = true,', '}};',
        'static const update_https_endpoints_t s_development_endpoints = {',
        f'    .allowed_host = {json.dumps(host)},',
        f'    .manifest_url = {json.dumps(urls[0])},',
        f'    .signature_url = {json.dumps(urls[1])},',
        f'    .image_url = {json.dumps(urls[2])},', '};', '',
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--public-key', type=Path, required=True)
    parser.add_argument('--product-id', type=lambda v: int(v, 0), required=True)
    parser.add_argument('--board-id', type=lambda v: int(v, 0), required=True)
    parser.add_argument('--key-id', type=lambda v: int(v, 0), default=1)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--openssl', default='openssl')
    args = parser.parse_args()
    endpoints(args.base_url)
    # -pubin rejects private key input; inspect the same immutable byte copy
    # that is embedded, avoiding a second read of a replaceable file.
    der = args.public_key.read_bytes()
    info = subprocess.run([args.openssl, 'pkey', '-pubin', '-inform', 'DER', '-text', '-noout'],
                          input=der, capture_output=True, check=True).stdout.decode('ascii')
    if 'Public-Key: (3072 bit)' not in info or 'Modulus:' not in info:
        raise ValueError('expected RSA-3072 public SubjectPublicKeyInfo DER')
    header = render(args.base_url, der, args.product_id, args.board_id, args.key_id)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header, encoding='utf-8')
    print(f'public_build_header={args.output.resolve()}')


if __name__ == '__main__':
    main()
