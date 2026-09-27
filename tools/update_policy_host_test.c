#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "update_policy.h"

static update_health_sample_t healthy(uint64_t now)
{
    return (update_health_sample_t){
        .display_ready = true, .first_frame_presented = true,
        .local_services_ready = true, .app_progress_seen = true,
        .ui_progress_seen = true, .app_progress_ms = now,
        .ui_progress_ms = now, .fallback_bootable = true,
    };
}

static void test_metadata(void)
{
    /* IDs are test fixtures, not assigned production identities. */
    const update_environment_t baseline = {
        .product_id = 1, .board_id = 2, .revision = 103,
        .inactive_slot_bytes = 0x800000, .security_version = 0,
        .persisted_schema = 1, .current_c6 = {3, 0, 6, 2, true},
        .current_app_confirmed = true, .transaction_idle = true,
    };
    const update_metadata_t candidate = {
        .target = UPDATE_TARGET_ESP32P4, .product_id = 1, .board_id = 2,
        .revision_min = 100, .revision_max = 199, .image_bytes = 0x800000,
        .security_version = 0, .schema_min = 1, .schema_max = 1,
        .required_c6 = {3, 0, 6, 2, true},
    };
    update_metadata_t m = candidate;
    update_environment_t e = baseline;
    assert(update_metadata_check(&m, &e) == UPDATE_METADATA_MATCH);
    assert(update_metadata_check(NULL, &e) == UPDATE_METADATA_INVALID);
    assert(update_metadata_check(&m, NULL) == UPDATE_METADATA_INVALID);
#define BAD_META(field, value, result) do { m = candidate; m.field = (value); \
    assert(update_metadata_check(&m, &baseline) == (result)); } while (0)
#define BAD_ENV(field, value, result) do { e = baseline; e.field = (value); \
    assert(update_metadata_check(&candidate, &e) == (result)); } while (0)
    BAD_META(product_id, 0, UPDATE_METADATA_INVALID);
    BAD_META(board_id, 0, UPDATE_METADATA_INVALID);
    BAD_META(revision_min, 200, UPDATE_METADATA_INVALID);
    BAD_META(schema_min, 0, UPDATE_METADATA_INVALID);
    BAD_META(schema_max, 0, UPDATE_METADATA_INVALID);
    BAD_ENV(product_id, 0, UPDATE_METADATA_INVALID);
    BAD_ENV(board_id, 0, UPDATE_METADATA_INVALID);
    BAD_ENV(persisted_schema, 0, UPDATE_METADATA_INVALID);
    BAD_ENV(current_app_confirmed, false, UPDATE_METADATA_BUSY);
    BAD_ENV(transaction_idle, false, UPDATE_METADATA_BUSY);
    BAD_META(replaces_c6, true, UPDATE_METADATA_UNSUPPORTED_OPERATION);
    BAD_META(replaces_bootloader, true, UPDATE_METADATA_UNSUPPORTED_OPERATION);
    BAD_META(replaces_partition_table, true, UPDATE_METADATA_UNSUPPORTED_OPERATION);
    BAD_META(target, UPDATE_TARGET_ESP32C6, UPDATE_METADATA_WRONG_DEVICE);
    BAD_META(target, UPDATE_TARGET_UNKNOWN, UPDATE_METADATA_WRONG_DEVICE);
    BAD_META(target, (update_target_t)99, UPDATE_METADATA_WRONG_DEVICE);
    BAD_META(product_id, 3, UPDATE_METADATA_WRONG_DEVICE);
    BAD_META(board_id, 3, UPDATE_METADATA_WRONG_DEVICE);
    BAD_ENV(revision, 99, UPDATE_METADATA_WRONG_DEVICE);
    BAD_ENV(revision, 200, UPDATE_METADATA_WRONG_DEVICE);
    BAD_META(image_bytes, 0, UPDATE_METADATA_IMAGE_SIZE);
    BAD_META(image_bytes, 0x800001, UPDATE_METADATA_IMAGE_SIZE);
    BAD_META(image_bytes, UINT32_MAX, UPDATE_METADATA_IMAGE_SIZE);
    BAD_ENV(inactive_slot_bytes, 0, UPDATE_METADATA_IMAGE_SIZE);
    BAD_META(security_version, 1, UPDATE_METADATA_SECURITY_VERSION);
    BAD_ENV(security_version, 1, UPDATE_METADATA_SECURITY_VERSION);
    BAD_ENV(persisted_schema, 2, UPDATE_METADATA_SCHEMA);
    BAD_META(schema_min, 2, UPDATE_METADATA_INVALID);
    m = candidate; m.schema_min = 2; m.schema_max = 3;
    assert(update_metadata_check(&m, &baseline) == UPDATE_METADATA_SCHEMA);
    BAD_META(required_c6.major, 2, UPDATE_METADATA_C6_MISMATCH);
    BAD_META(required_c6.minor, 1, UPDATE_METADATA_C6_MISMATCH);
    BAD_META(required_c6.patch, 7, UPDATE_METADATA_C6_MISMATCH);
    BAD_META(required_c6.rpc, 1, UPDATE_METADATA_C6_MISMATCH);
    BAD_META(required_c6.sdio_sw_aggr, false, UPDATE_METADATA_C6_MISMATCH);
    BAD_ENV(current_c6.patch, 7, UPDATE_METADATA_C6_MISMATCH);
    e = baseline; m = candidate;
    e.current_c6.patch = m.required_c6.patch = 7;
    assert(update_metadata_check(&m, &e) == UPDATE_METADATA_C6_MISMATCH);
    e = baseline; e.revision = 100;
    assert(update_metadata_check(&candidate, &e) == UPDATE_METADATA_MATCH);
    e.revision = 199;
    assert(update_metadata_check(&candidate, &e) == UPDATE_METADATA_MATCH);
#undef BAD_META
#undef BAD_ENV
}

static void test_boot(void)
{
    update_boot_policy_t p = {0};
    update_health_sample_t s = healthy(0);
    assert(update_boot_policy_poll(NULL, 0, &s) == UPDATE_BOOT_RECOVERY);
    assert(update_boot_policy_poll(&p, 0, &s) == UPDATE_BOOT_RECOVERY);
    update_boot_policy_init(NULL, 0, true);
    update_boot_policy_init(&p, 0, false);
    assert(update_boot_policy_poll(&p, 60000, NULL) == UPDATE_BOOT_NOT_PENDING);
    update_boot_policy_init(&p, 0, true);
    for (uint64_t t = 0; t <= 15000; t += 250) {
        s = healthy(t);
        assert(update_boot_policy_poll(&p, t, &s) ==
               (t == 15000 ? UPDATE_BOOT_CONFIRM : UPDATE_BOOT_WAIT));
    }
    /* One good observation after a long pause does not prove health. */
    update_boot_policy_init(&p, 0, true);
    s = healthy(0);
    assert(update_boot_policy_poll(&p, 0, &s) == UPDATE_BOOT_WAIT);
    s = healthy(15000);
    assert(update_boot_policy_poll(&p, 15000, &s) == UPDATE_BOOT_WAIT);
    /* Every required source resets continuity independently. */
    for (unsigned fault = 0; fault < 10; ++fault) {
        update_boot_policy_init(&p, 0, true);
        for (uint64_t t = 0; t <= 30000; t += 250) {
            s = healthy(t);
            if (t == 14750) {
                switch (fault) {
                case 0: s.display_ready = false; break;
                case 1: s.first_frame_presented = false; break;
                case 2: s.local_services_ready = false; break;
                case 3: s.app_progress_seen = false; break;
                case 4: s.ui_progress_seen = false; break;
                case 5: s.app_progress_ms = t - 1001; break;
                case 6: s.ui_progress_ms = t - 501; break;
                case 7: s.app_progress_ms = t + 1; break;
                case 8: s.ui_progress_ms = t + 1; break;
                default: break;
                }
            }
            const update_health_sample_t *sample =
                (t == 14750 && fault == 9) ? NULL : &s;
            assert(update_boot_policy_poll(&p, t, sample) ==
                   (t == 30000 ? UPDATE_BOOT_CONFIRM : UPDATE_BOOT_WAIT));
        }
    }
    /* A stalled task cannot keep a pending image alive through old timestamps. */
    update_boot_policy_init(&p, 0, true);
    s = healthy(0);
    for (uint64_t t = 0; t < 60000; t += 250) {
        assert(update_boot_policy_poll(&p, t, &s) == UPDATE_BOOT_WAIT);
    }
    assert(update_boot_policy_poll(&p, 60000, &s) == UPDATE_BOOT_ROLLBACK);
    s.fallback_bootable = false;
    assert(update_boot_policy_poll(&p, 60000, &s) == UPDATE_BOOT_RECOVERY);
    assert(update_boot_policy_poll(&p, 60000, NULL) == UPDATE_BOOT_RECOVERY);
    /* Deadline wins even when health would mature on exactly 60 seconds. */
    update_boot_policy_init(&p, 0, true);
    for (uint64_t t = 45000; t <= 60000; t += 250) {
        s = healthy(t);
        assert(update_boot_policy_poll(&p, t, &s) ==
               (t == 60000 ? UPDATE_BOOT_ROLLBACK : UPDATE_BOOT_WAIT));
    }
    /* High monotonic uptime and backwards clocks cannot wrap into a pass. */
    const uint64_t start = UINT64_MAX - 60000;
    update_boot_policy_init(&p, start, true);
    for (uint64_t elapsed = 0; elapsed <= 15000; elapsed += 250) {
        s = healthy(start + elapsed);
        assert(update_boot_policy_poll(&p, start + elapsed, &s) ==
               (elapsed == 15000 ? UPDATE_BOOT_CONFIRM : UPDATE_BOOT_WAIT));
    }
    assert(update_boot_policy_poll(&p, 0, &s) == UPDATE_BOOT_RECOVERY);
    assert(update_boot_policy_poll(&p, UINT64_MAX, &s) == UPDATE_BOOT_RECOVERY);
    update_boot_policy_init(&p, 10000, true);
    s = healthy(9999);
    assert(update_boot_policy_poll(&p, 10000, &s) == UPDATE_BOOT_WAIT);
    assert(!p.tracking_health);
}

int main(void)
{
    test_metadata();
    test_boot();
    puts("update_policy: metadata and boot health tests passed");
    return 0;
}
