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
    esp_err_t init_result;
    esp_err_t last_result;
} flash_coordinator_status_t;

esp_err_t flash_coordinator_start(void);

/*
 * Queues a single, rate-limited NVS commit used only by the Phase 2 display
 * qualification. It contains no credentials or product data.
 */
esp_err_t flash_coordinator_request_nvs_probe(void);


/* Safe from the LVGL task; returns a short critical-section snapshot. */
void flash_coordinator_get_status(flash_coordinator_status_t *out_status);
