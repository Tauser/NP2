#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "offline_data_model.h"

/*
 * The coordinator is the sole owner of normal flash writes. Callers only
 * enqueue an intent; they never call NVS or filesystem APIs from UI callbacks.
 */
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


/* Safe from the LVGL task; returns a short critical-section snapshot. */
void flash_coordinator_get_status(flash_coordinator_status_t *out_status);
