#include "update_keyring.h"

static bool key_is_well_formed(const update_trusted_key_t *key)
{
    return key->key_id != 0U && key->public_key_der != NULL &&
           key->public_key_der_bytes != 0U &&
           key->public_key_der_bytes <= UPDATE_SIGNATURE_PUBLIC_KEY_MAX_BYTES;
}

update_keyring_result_t update_keyring_lookup(const update_keyring_t *keyring,
                                              uint32_t key_id,
                                              const update_trusted_key_t **out_key)
{
    if (out_key != NULL) {
        *out_key = NULL;
    }
    if (keyring == NULL || out_key == NULL || key_id == 0U ||
        keyring->entries == NULL || keyring->entries_count == 0U) {
        return UPDATE_KEYRING_INVALID_ARGUMENT;
    }

    const update_keyring_entry_t *match = NULL;
    for (size_t index = 0; index < keyring->entries_count; ++index) {
        const update_keyring_entry_t *entry = &keyring->entries[index];
        if (entry->trusted_key.key_id != key_id) {
            continue;
        }
        if (match != NULL) {
            return UPDATE_KEYRING_AMBIGUOUS_KEY;
        }
        match = entry;
    }
    if (match == NULL) {
        return UPDATE_KEYRING_UNKNOWN_KEY;
    }
    if (!match->enabled) {
        return UPDATE_KEYRING_REVOKED_KEY;
    }
    if (!key_is_well_formed(&match->trusted_key)) {
        return UPDATE_KEYRING_MALFORMED;
    }
    *out_key = &match->trusted_key;
    return UPDATE_KEYRING_OK;
}
