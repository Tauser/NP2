#pragma once

#include <stddef.h>
#include <stdint.h>

#include "update_manifest.h"

#define UPDATE_SIGNATURE_RSA3072_BYTES 384U
#define UPDATE_SIGNATURE_WIRE_BYTES (4U + UPDATE_SIGNATURE_RSA3072_BYTES)
#define UPDATE_SIGNATURE_PUBLIC_KEY_MAX_BYTES 1024U

typedef struct {
    uint32_t key_id;
    uint8_t signature[UPDATE_SIGNATURE_RSA3072_BYTES];
} update_signature_t;

typedef struct {
    uint32_t key_id;
    const uint8_t *public_key_der;
    size_t public_key_der_bytes;
} update_trusted_key_t;

typedef enum {
    UPDATE_SIGNATURE_OK = 0,
    UPDATE_SIGNATURE_INVALID_ARGUMENT,
    UPDATE_SIGNATURE_MALFORMED,
    UPDATE_SIGNATURE_UNKNOWN_KEY,
    UPDATE_SIGNATURE_UNSUPPORTED_KEY,
    UPDATE_SIGNATURE_INVALID,
} update_signature_result_t;

/* Parses the detached signature envelope: little-endian key ID then exactly
 * 384 signature bytes. The key ID selects a trusted public key; it does not
 * authorize an update until the manifest bytes verify under that key. */
update_signature_result_t update_signature_parse(const uint8_t *wire,
                                                 size_t wire_bytes,
                                                 update_signature_t *out_signature);

/* Verifies RSA-3072 RSASSA-PSS with SHA-256, MGF1 SHA-256 and a 32-byte salt.
 * public_key_der is SubjectPublicKeyInfo DER owned by the caller/keyring.
 * No private key, heap-owned key copy, flash write or OTA action occurs here. */
update_signature_result_t update_signature_verify_manifest(
    const uint8_t *manifest_wire, size_t manifest_wire_bytes,
    const update_signature_t *signature, const update_trusted_key_t *trusted_key);
