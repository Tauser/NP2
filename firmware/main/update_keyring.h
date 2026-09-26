#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "update_signature.h"

/* The platform injects this immutable public-key registry at startup. Public
 * keys are release trust material, never private signing material. */
typedef struct {
    update_trusted_key_t trusted_key;
    bool enabled;
} update_keyring_entry_t;

typedef struct {
    const update_keyring_entry_t *entries;
    size_t entries_count;
} update_keyring_t;

typedef enum {
    UPDATE_KEYRING_OK = 0,
    UPDATE_KEYRING_INVALID_ARGUMENT,
    UPDATE_KEYRING_UNKNOWN_KEY,
    UPDATE_KEYRING_REVOKED_KEY,
    UPDATE_KEYRING_MALFORMED,
    UPDATE_KEYRING_AMBIGUOUS_KEY,
} update_keyring_result_t;

/* Returns a borrowed immutable key only if exactly one valid, enabled entry
 * matches key_id. An entry marked disabled is an explicit revocation. */
update_keyring_result_t update_keyring_lookup(const update_keyring_t *keyring,
                                              uint32_t key_id,
                                              const update_trusted_key_t **out_key);
