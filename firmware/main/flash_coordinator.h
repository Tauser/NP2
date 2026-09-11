#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

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
    bool cache_valid;
    uint32_t cache_generation;
    uint16_t cache_schema_version;
    esp_err_t cache_result;
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

/* Queues a small two-slot NVS configuration-journal write for G4 validation. */
esp_err_t flash_coordinator_request_config_journal_write(void);

/* Corrupts only the newest diagnostic configuration record to verify fallback. */
esp_err_t flash_coordinator_request_config_corrupt_newest(void);


/* Safe from the LVGL task; returns a short critical-section snapshot. */
void flash_coordinator_get_status(flash_coordinator_status_t *out_status);
