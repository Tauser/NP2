#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mbedtls/sha256.h"
#include "update_manifest.h"

#define UPDATE_IMAGE_HASH_CHUNK_MAX_BYTES 4096U

typedef struct {
    mbedtls_sha256_context context;
    uint8_t expected_sha256[UPDATE_MANIFEST_SHA256_BYTES];
    uint32_t expected_bytes;
    uint32_t received_bytes;
    bool initialized;
    bool active;
} update_image_hash_session_t;

typedef enum {
    UPDATE_IMAGE_HASH_OK = 0,
    UPDATE_IMAGE_HASH_INVALID_ARGUMENT,
    UPDATE_IMAGE_HASH_INVALID_STATE,
    UPDATE_IMAGE_HASH_CHUNK_TOO_LARGE,
    UPDATE_IMAGE_HASH_LENGTH_MISMATCH,
    UPDATE_IMAGE_HASH_MISMATCH,
    UPDATE_IMAGE_HASH_CRYPTO_FAILURE,
} update_image_hash_result_t;

/* The owner initializes once, begins from an authenticated manifest, feeds the
 * exact streaming bytes, then calls finish or abort. This session never owns
 * a network buffer, flash partition or OTA handle. */
void update_image_hash_session_init(update_image_hash_session_t *session);
update_image_hash_result_t update_image_hash_begin(update_image_hash_session_t *session,
                                                    const update_manifest_t *manifest);
update_image_hash_result_t update_image_hash_append(update_image_hash_session_t *session,
                                                     const uint8_t *bytes, size_t bytes_count);
update_image_hash_result_t update_image_hash_finish(update_image_hash_session_t *session);
void update_image_hash_abort(update_image_hash_session_t *session);
