#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Domain-only contracts, not a wire format or an authorization to write flash.
 * Signature, image hash/header and journal validation belong to later adapters.
 * A metadata match alone MUST NOT authorize download activation. */
typedef enum {
    UPDATE_TARGET_UNKNOWN = 0,
    UPDATE_TARGET_ESP32P4,
    UPDATE_TARGET_ESP32C6,
} update_target_t;

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint8_t rpc;
    bool sdio_sw_aggr;
} update_link_version_t;

typedef struct {
    update_target_t target;
    uint32_t product_id;
    uint32_t board_id;
    uint16_t revision_min;
    uint16_t revision_max;
    uint32_t image_bytes; /* Complete signed image including padding. */
    uint32_t security_version;
    uint16_t schema_min;
    uint16_t schema_max;
    update_link_version_t required_c6;
    bool replaces_c6;
    bool replaces_bootloader;
    bool replaces_partition_table;
} update_metadata_t;

typedef struct {
    uint32_t product_id; /* Nonzero local identities; numeric registry TBD. */
    uint32_t board_id;
    uint16_t revision;
    uint32_t inactive_slot_bytes;
    uint32_t security_version; /* This phase requires the same security version. */
    uint16_t persisted_schema;
    update_link_version_t current_c6;
    bool current_app_confirmed;
    bool transaction_idle;
} update_environment_t;

typedef enum {
    UPDATE_METADATA_MATCH = 0,
    UPDATE_METADATA_INVALID,
    UPDATE_METADATA_BUSY,
    UPDATE_METADATA_UNSUPPORTED_OPERATION,
    UPDATE_METADATA_WRONG_DEVICE,
    UPDATE_METADATA_IMAGE_SIZE,
    UPDATE_METADATA_SECURITY_VERSION,
    UPDATE_METADATA_SCHEMA,
    UPDATE_METADATA_C6_MISMATCH,
} update_metadata_result_t;

update_metadata_result_t update_metadata_check(const update_metadata_t *metadata,
                                               const update_environment_t *environment);

#define UPDATE_HEALTH_STABLE_MS UINT64_C(15000)
#define UPDATE_HEALTH_DEADLINE_MS UINT64_C(60000)
#define UPDATE_HEALTH_SAMPLE_GAP_MS UINT64_C(500)
#define UPDATE_HEALTH_APP_AGE_MS UINT64_C(1000)
#define UPDATE_HEALTH_UI_AGE_MS UINT64_C(500)

typedef struct {
    bool display_ready;
    bool first_frame_presented;
    bool local_services_ready; /* Offline/defaults may be a healthy state. */
    bool app_progress_seen;
    bool ui_progress_seen;
    uint64_t app_progress_ms;
    uint64_t ui_progress_ms;
    bool fallback_bootable; /* Must be rechecked by the platform before rollback. */
} update_health_sample_t;

typedef struct {
    uint64_t boot_ms;
    uint64_t last_sample_ms;
    uint64_t healthy_since_ms;
    bool pending_verify;
    bool tracking_health;
    bool initialized;
    bool clock_fault;
} update_boot_policy_t;

typedef enum {
    UPDATE_BOOT_NOT_PENDING = 0,
    UPDATE_BOOT_WAIT,
    UPDATE_BOOT_CONFIRM,
    UPDATE_BOOT_ROLLBACK,
    UPDATE_BOOT_RECOVERY,
} update_boot_action_t;

/* Initialize once per boot from the running slot, before local bring-up.
 * Single-owner policy state. Use monotonic milliseconds, never wall time. */
void update_boot_policy_init(update_boot_policy_t *policy, uint64_t boot_ms,
                             bool pending_verify);
/* Poll at <=500 ms; missed observations restart the continuous-health window.
 * This returns a recommendation only. Confirm/rollback require coordinated
 * flash I/O and a checked API result; the caller must not equate it to success.
 * No AP, C6, DNS, NTP or HTTPS health participates in boot confirmation. */
update_boot_action_t update_boot_policy_poll(update_boot_policy_t *policy,
                                             uint64_t now_ms,
                                             const update_health_sample_t *sample);
