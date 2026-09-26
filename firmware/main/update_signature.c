#include "update_signature.h"

#include <stdbool.h>
#include <string.h>

#include "mbedtls/md.h"
#include "mbedtls/pk.h"

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool all_zero(const uint8_t *bytes, size_t bytes_count)
{
    uint8_t value = 0;
    for (size_t index = 0; index < bytes_count; ++index) {
        value |= bytes[index];
    }
    return value == 0;
}

update_signature_result_t update_signature_parse(const uint8_t *wire,
                                                 size_t wire_bytes,
                                                 update_signature_t *out_signature)
{
    if (wire == NULL || out_signature == NULL) {
        return UPDATE_SIGNATURE_INVALID_ARGUMENT;
    }
    if (wire_bytes != UPDATE_SIGNATURE_WIRE_BYTES) {
        return UPDATE_SIGNATURE_MALFORMED;
    }
    const uint32_t key_id = read_u32_le(wire);
    if (key_id == 0U || all_zero(&wire[4], UPDATE_SIGNATURE_RSA3072_BYTES)) {
        return UPDATE_SIGNATURE_MALFORMED;
    }
    update_signature_t parsed = {.key_id = key_id};
    memcpy(parsed.signature, &wire[4], sizeof(parsed.signature));
    *out_signature = parsed;
    return UPDATE_SIGNATURE_OK;
}

update_signature_result_t update_signature_verify_manifest(
    const uint8_t *manifest_wire, size_t manifest_wire_bytes,
    const update_signature_t *signature, const update_trusted_key_t *trusted_key)
{
    if (manifest_wire == NULL || signature == NULL || trusted_key == NULL) {
        return UPDATE_SIGNATURE_INVALID_ARGUMENT;
    }
    if (signature->key_id == 0U || trusted_key->key_id == 0U ||
        trusted_key->public_key_der == NULL || trusted_key->public_key_der_bytes == 0U ||
        trusted_key->public_key_der_bytes > UPDATE_SIGNATURE_PUBLIC_KEY_MAX_BYTES) {
        return UPDATE_SIGNATURE_INVALID_ARGUMENT;
    }
    if (signature->key_id != trusted_key->key_id) {
        return UPDATE_SIGNATURE_UNKNOWN_KEY;
    }
    const uint8_t *signed_bytes = NULL;
    size_t signed_bytes_len = 0;
    if (update_manifest_signed_bytes(manifest_wire, manifest_wire_bytes,
                                     &signed_bytes, &signed_bytes_len) != UPDATE_MANIFEST_OK) {
        return UPDATE_SIGNATURE_MALFORMED;
    }

    const mbedtls_md_info_t *const sha256 = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    uint8_t digest[32] = {0};
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    update_signature_result_t result = UPDATE_SIGNATURE_INVALID;
    if (sha256 == NULL || mbedtls_md(sha256, signed_bytes, signed_bytes_len, digest) != 0) {
        result = UPDATE_SIGNATURE_UNSUPPORTED_KEY;
        goto cleanup;
    }
    if (mbedtls_pk_parse_public_key(&key, trusted_key->public_key_der,
                                    trusted_key->public_key_der_bytes) != 0 ||
        !mbedtls_pk_can_do(&key, MBEDTLS_PK_RSA) ||
        mbedtls_pk_get_bitlen(&key) != 3072U) {
        result = UPDATE_SIGNATURE_UNSUPPORTED_KEY;
        goto cleanup;
    }
    const mbedtls_pk_rsassa_pss_options pss_options = {
        .mgf1_hash_id = MBEDTLS_MD_SHA256,
        .expected_salt_len = 32,
    };
    result = mbedtls_pk_verify_ext(MBEDTLS_PK_RSASSA_PSS, &pss_options, &key,
                                   MBEDTLS_MD_SHA256, digest, sizeof(digest),
                                   signature->signature, sizeof(signature->signature)) == 0 ?
             UPDATE_SIGNATURE_OK : UPDATE_SIGNATURE_INVALID;

cleanup:
    mbedtls_pk_free(&key);
    memset(digest, 0, sizeof(digest));
    return result;
}
