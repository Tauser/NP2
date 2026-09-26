#pragma once

#include "update_image_hash.h"
#include "update_keyring.h"
#include "update_replay_policy.h"

typedef enum {
    UPDATE_ADMISSION_OK = 0,
    UPDATE_ADMISSION_INVALID_ARGUMENT,
    UPDATE_ADMISSION_MANIFEST,
    UPDATE_ADMISSION_SIGNATURE_WIRE,
    UPDATE_ADMISSION_KEYRING,
    UPDATE_ADMISSION_SIGNATURE,
    UPDATE_ADMISSION_METADATA,
    UPDATE_ADMISSION_REPLAY,
    UPDATE_ADMISSION_HASH,
} update_admission_result_t;

/* Validates the complete release identity before a future HTTPS owner feeds
 * image bytes. It has no transport, partition, journal or activation handle. */
update_admission_result_t update_admission_begin(
    const uint8_t *manifest_wire, size_t manifest_wire_bytes,
    const uint8_t *signature_wire, size_t signature_wire_bytes,
    const update_keyring_t *keyring, const update_environment_t *environment,
    const update_replay_record_t *accepted_record,
    update_image_hash_session_t *hash_session, update_manifest_t *out_manifest);
