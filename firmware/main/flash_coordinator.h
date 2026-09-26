#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_partition.h"
#include "offline_data_model.h"
#include "update_journal.h"

/*
 * The coordinator is the sole owner of normal flash writes. Callers only
 * enqueue an intent; they never call NVS or filesystem APIs from UI callbacks.
 */
typedef struct {
    bool completed;
    bool clock_24h;
    uint8_t timezone_index;
} onboarding_profile_t;

typedef struct {
    bool ready;
    bool busy;
    bool pending;
    uint32_t completed_count;
    uint32_t rejected_count;
    uint32_t last_sequence;
    uint32_t last_duration_ms;
    uint32_t last_batch_writes;
    uint32_t last_free_entries_before;
    uint32_t last_free_entries_after;
    uint32_t last_littlefs_writes;
    uint32_t last_littlefs_verified_bytes;
    bool littlefs_ready;
    bool last_littlefs_format;
    bool full_probe_active;
    bool full_probe_syncing;
    uint32_t full_probe_written_bytes;
    uint32_t full_probe_target_bytes;
    bool power_cut_window_active;
    bool power_cut_after_rename;
    uint32_t power_cut_remaining_ms;
    bool cache_valid;
    uint32_t cache_generation;
    uint16_t cache_schema_version;
    esp_err_t cache_result;
    bool offline_data_valid;
    offline_data_snapshot_t offline_data;
    bool config_valid;
    uint32_t config_generation;
    uint16_t config_schema_version;
    esp_err_t config_result;
    bool onboarding_profile_valid;
    uint32_t onboarding_profile_generation;
    onboarding_profile_t onboarding_profile;
    esp_err_t onboarding_profile_result;
    /* Credential-vault presence only. Credential data is never exposed here. */
    bool credential_vault_valid;
    uint32_t credential_vault_generation;
    esp_err_t credential_vault_result;
    bool update_journal_valid;
    uint32_t update_journal_generation;
    uint16_t update_journal_state;
    esp_err_t update_journal_result;
    bool p4_ota_active;
    bool p4_ota_finished;
    uint32_t p4_ota_expected_bytes;
    uint32_t p4_ota_written_bytes;
    const esp_partition_t *p4_ota_partition;
    esp_err_t init_result;
    esp_err_t littlefs_init_result;
    esp_err_t last_result;
} flash_coordinator_status_t;

esp_err_t flash_coordinator_start(void);

/*
 * Queues a single, rate-limited NVS commit used only by the Phase 2 display
 * qualification. It contains no credentials or product data.
 */
esp_err_t flash_coordinator_request_nvs_probe(void);

/*
 * Queues the Phase 2 NVS compaction workload: 64 commits of a 512-byte blob
 * in the diagnostic namespace. It is allowed only as an explicit bench test
 * while the render workload is active; it never erases another partition.
 */
esp_err_t flash_coordinator_request_nvs_compaction_probe(void);

/* Runs 64 write -> fsync -> rename -> read/verify cycles in /lfsdiag. */
esp_err_t flash_coordinator_request_littlefs_probe(void);

/* Explicitly formats only the `storage` partition, then mounts it. */
esp_err_t flash_coordinator_request_littlefs_format(void);

/*
 * Physical-bench fault injection for G4. It corrupts one byte only in the
 * newest validated cache generation and refuses to run without an older valid
 * generation to fall back to. It never formats storage.
 */
esp_err_t flash_coordinator_request_cache_corrupt_newest(void);

/*
 * Physical G4 test only. Fills LittleFS through an isolated temporary file,
 * verifies that a proposed cache generation is rejected, then removes the
 * filler and confirms the last valid generation remains selected.
 */
esp_err_t flash_coordinator_request_cache_full_probe(void);

/* Physical power-cut windows at the tmp+fsync and rename durability boundaries. */
esp_err_t flash_coordinator_request_cache_cut_before_rename(void);
esp_err_t flash_coordinator_request_cache_cut_after_rename(void);

/* Persists one validated product snapshot through the serialized coordinator. */
esp_err_t flash_coordinator_request_offline_data_write(const offline_data_snapshot_t *snapshot);

/* Queues a small two-slot NVS configuration-journal write for G4 validation. */
esp_err_t flash_coordinator_request_config_journal_write(void);

/* Corrupts only the newest diagnostic configuration record to verify fallback. */
esp_err_t flash_coordinator_request_config_corrupt_newest(void);

/* Persists only non-secret onboarding preferences through the flash owner. */
esp_err_t flash_coordinator_request_onboarding_profile_write(const onboarding_profile_t *profile);

/*
 * Credential-vault storage. Production requires NVS Encryption and active
 * Flash Encryption. An explicitly compiled development profile may retain
 * credentials in local NVS on an unlocked P4. The driver keeps
 * WIFI_STORAGE_RAM; only this coordinator writes the vault.
 */
bool flash_coordinator_credential_vault_ready(void);
esp_err_t flash_coordinator_request_credential_vault_write(const char *ssid,
                                                            const char *password);
esp_err_t flash_coordinator_request_credential_vault_clear(void);

/* Private handoff for the connectivity worker; no UI/status getter exists. */
esp_err_t flash_coordinator_copy_credential_vault(char *out_ssid, size_t ssid_size,
                                                   char *out_password, size_t password_size);

/* Persists one sealed OTA journal transition through the sole flash owner. */
esp_err_t flash_coordinator_request_update_journal(const update_journal_record_t *record);

/* Reads the newest validated OTA journal through its storage owner. */
esp_err_t flash_coordinator_get_update_journal(update_journal_record_t *out_record);

/* Blocking only for the dedicated network executor; the coordinator task
 * remains the sole owner of esp_ota begin/write/end/select operations. */
esp_err_t flash_coordinator_p4_ota_begin(uint32_t expected_bytes,
                                         const esp_partition_t **out_partition);
esp_err_t flash_coordinator_p4_ota_append(const uint8_t *bytes, uint32_t bytes_count);
esp_err_t flash_coordinator_p4_ota_finish(void);
esp_err_t flash_coordinator_p4_ota_abort(void);
esp_err_t flash_coordinator_p4_ota_activate(const update_journal_record_t *journal);

/* Final OTA-data mutations, serialized with all other product flash writes. */
esp_err_t flash_coordinator_request_p4_ota_confirm(void);
esp_err_t flash_coordinator_request_p4_ota_rollback(void);


/* Safe from the LVGL task; returns a short critical-section snapshot. */
void flash_coordinator_get_status(flash_coordinator_status_t *out_status);
