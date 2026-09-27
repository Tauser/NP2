#include <assert.h>
#include <stdio.h>

#include "update_keyring.h"

static void test_lookup(void)
{
    static const uint8_t first_der[] = {0x30, 0x01, 0x00};
    static const uint8_t second_der[] = {0x30, 0x01, 0x01};
    const update_keyring_entry_t entries[] = {
        {.trusted_key = {.key_id = 7, .public_key_der = first_der,
                         .public_key_der_bytes = sizeof(first_der)}, .enabled = true},
        {.trusted_key = {.key_id = 9, .public_key_der = second_der,
                         .public_key_der_bytes = sizeof(second_der)}, .enabled = false},
    };
    const update_keyring_t keyring = {.entries = entries, .entries_count = 2};
    const update_trusted_key_t *key = NULL;
    assert(update_keyring_lookup(&keyring, 7, &key) == UPDATE_KEYRING_OK);
    assert(key == &entries[0].trusted_key);
    assert(update_keyring_lookup(&keyring, 8, &key) == UPDATE_KEYRING_UNKNOWN_KEY);
    assert(key == NULL);
    assert(update_keyring_lookup(&keyring, 9, &key) == UPDATE_KEYRING_REVOKED_KEY);
    assert(key == NULL);
}

static void test_rejections(void)
{
    static const uint8_t der[] = {0x30, 0x01, 0x00};
    const update_keyring_entry_t malformed[] = {
        {.trusted_key = {.key_id = 3, .public_key_der = NULL, .public_key_der_bytes = 3},
         .enabled = true},
    };
    const update_keyring_entry_t duplicate[] = {
        {.trusted_key = {.key_id = 4, .public_key_der = der, .public_key_der_bytes = sizeof(der)},
         .enabled = true},
        {.trusted_key = {.key_id = 4, .public_key_der = der, .public_key_der_bytes = sizeof(der)},
         .enabled = false},
    };
    const update_trusted_key_t *key = (const update_trusted_key_t *)der;
    assert(update_keyring_lookup(NULL, 1, &key) == UPDATE_KEYRING_INVALID_ARGUMENT);
    assert(key == NULL);
    const update_keyring_t empty = {0};
    assert(update_keyring_lookup(&empty, 1, &key) == UPDATE_KEYRING_INVALID_ARGUMENT);
    const update_keyring_t invalid = {.entries = malformed, .entries_count = 1};
    assert(update_keyring_lookup(&invalid, 3, &key) == UPDATE_KEYRING_MALFORMED);
    const update_keyring_t ambiguous = {.entries = duplicate, .entries_count = 2};
    assert(update_keyring_lookup(&ambiguous, 4, &key) == UPDATE_KEYRING_AMBIGUOUS_KEY);
}

int main(void)
{
    test_lookup();
    test_rejections();
    puts("update_keyring_host_test: all checks passed");
    return 0;
}
