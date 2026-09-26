#include "update_image_hash.h"

#include <string.h>

static bool constant_time_equal(const uint8_t *left, const uint8_t *right, size_t bytes_count)
{
    uint8_t difference = 0;
    for (size_t index = 0; index < bytes_count; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

void update_image_hash_session_init(update_image_hash_session_t *session)
{
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    mbedtls_sha256_init(&session->context);
    session->initialized = true;
}

void update_image_hash_abort(update_image_hash_session_t *session)
{
    if (session == NULL || !session->initialized) {
        return;
    }
    mbedtls_sha256_free(&session->context);
    mbedtls_sha256_init(&session->context);
    memset(session->expected_sha256, 0, sizeof(session->expected_sha256));
    session->expected_bytes = 0;
    session->received_bytes = 0;
    session->active = false;
}

update_image_hash_result_t update_image_hash_begin(update_image_hash_session_t *session,
                                                    const update_manifest_t *manifest)
{
    if (session == NULL || manifest == NULL) {
        return UPDATE_IMAGE_HASH_INVALID_ARGUMENT;
    }
    if (!session->initialized || session->active || manifest->metadata.image_bytes == 0U) {
        return UPDATE_IMAGE_HASH_INVALID_STATE;
    }
    if (mbedtls_sha256_starts(&session->context, 0) != 0) {
        return UPDATE_IMAGE_HASH_CRYPTO_FAILURE;
    }
    memcpy(session->expected_sha256, manifest->image_sha256, sizeof(session->expected_sha256));
    session->expected_bytes = manifest->metadata.image_bytes;
    session->received_bytes = 0;
    session->active = true;
    return UPDATE_IMAGE_HASH_OK;
}

update_image_hash_result_t update_image_hash_append(update_image_hash_session_t *session,
                                                     const uint8_t *bytes, size_t bytes_count)
{
    if (session == NULL || bytes == NULL || bytes_count == 0U) {
        return UPDATE_IMAGE_HASH_INVALID_ARGUMENT;
    }
    if (!session->initialized || !session->active) {
        return UPDATE_IMAGE_HASH_INVALID_STATE;
    }
    if (bytes_count > UPDATE_IMAGE_HASH_CHUNK_MAX_BYTES) {
        return UPDATE_IMAGE_HASH_CHUNK_TOO_LARGE;
    }
    if (session->received_bytes > session->expected_bytes ||
        bytes_count > (size_t)(session->expected_bytes - session->received_bytes)) {
        return UPDATE_IMAGE_HASH_LENGTH_MISMATCH;
    }
    if (mbedtls_sha256_update(&session->context, bytes, bytes_count) != 0) {
        update_image_hash_abort(session);
        return UPDATE_IMAGE_HASH_CRYPTO_FAILURE;
    }
    session->received_bytes += (uint32_t)bytes_count;
    return UPDATE_IMAGE_HASH_OK;
}

update_image_hash_result_t update_image_hash_finish(update_image_hash_session_t *session)
{
    if (session == NULL) {
        return UPDATE_IMAGE_HASH_INVALID_ARGUMENT;
    }
    if (!session->initialized || !session->active) {
        return UPDATE_IMAGE_HASH_INVALID_STATE;
    }
    if (session->received_bytes != session->expected_bytes) {
        update_image_hash_abort(session);
        return UPDATE_IMAGE_HASH_LENGTH_MISMATCH;
    }
    uint8_t digest[UPDATE_MANIFEST_SHA256_BYTES] = {0};
    if (mbedtls_sha256_finish(&session->context, digest) != 0) {
        update_image_hash_abort(session);
        return UPDATE_IMAGE_HASH_CRYPTO_FAILURE;
    }
    const bool match = constant_time_equal(digest, session->expected_sha256, sizeof(digest));
    memset(digest, 0, sizeof(digest));
    update_image_hash_abort(session);
    return match ? UPDATE_IMAGE_HASH_OK : UPDATE_IMAGE_HASH_MISMATCH;
}
