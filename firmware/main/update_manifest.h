#pragma once

#include <stddef.h>
#include <stdint.h>

#include "update_policy.h"

/*
 * Canonical, fixed-size P4-only manifest envelope.  The 104 input bytes are
 * signed as received; callers must never deserialize and serialize again
 * before checking a detached signature.  This is an internal release format,
 * not a persisted ABI.
 */
#define UPDATE_MANIFEST_MAGIC UINT32_C(0x4e50324d) /* ASCII "NP2M". */
#define UPDATE_MANIFEST_FORMAT_VERSION UINT16_C(1)
#define UPDATE_MANIFEST_WIRE_BYTES 104U
#define UPDATE_MANIFEST_ID_BYTES 16U
#define UPDATE_MANIFEST_SHA256_BYTES 32U

typedef struct {
    uint8_t manifest_id[UPDATE_MANIFEST_ID_BYTES];
    update_metadata_t metadata;
    uint32_t app_version;
    uint8_t image_sha256[UPDATE_MANIFEST_SHA256_BYTES];
} update_manifest_t;

typedef enum {
    UPDATE_MANIFEST_OK = 0,
    UPDATE_MANIFEST_INVALID_ARGUMENT,
    UPDATE_MANIFEST_MALFORMED,
    UPDATE_MANIFEST_UNSUPPORTED,
    UPDATE_MANIFEST_RANGE,
} update_manifest_result_t;

/* Requires exactly UPDATE_MANIFEST_WIRE_BYTES.  All reserved bits and bytes
 * must be zero.  C6, bootloader and partition-table replacement flags are
 * deliberately rejected; only a later, physically gated transaction may add
 * a different manifest version for those operations. */
update_manifest_result_t update_manifest_parse(const uint8_t *wire,
                                               size_t wire_bytes,
                                               update_manifest_t *out_manifest);

/* Returns the exact canonical range to pass to detached-signature verification
 * after update_manifest_parse() succeeds.  No allocation or normalization. */
update_manifest_result_t update_manifest_signed_bytes(const uint8_t *wire,
                                                      size_t wire_bytes,
                                                      const uint8_t **out_bytes,
                                                      size_t *out_bytes_len);
