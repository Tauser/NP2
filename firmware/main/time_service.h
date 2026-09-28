/* Trusted wall-clock owner for the P4. It exposes state, never UI objects. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "timezone_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_SERVICE_NTP_HOST "time.cloudflare.com"
#define TIME_SERVICE_TIMEZONE_SAO_PAULO TIMEZONE_CATALOG_SAO_PAULO

typedef struct {
    bool ready;
    bool sync_in_progress;
    bool trusted;
    uint16_t timezone_index;
    uint32_t last_sync_unix_s;
    uint32_t last_duration_ms;
    uint32_t completed_syncs;
    esp_err_t last_result;
} time_service_status_t;

esp_err_t time_service_start(void);

/*
 * Runs in the one network worker. The service keeps SNTP active after its
 * first initialization so a later reader has one authoritative time source.
 */
esp_err_t time_service_sync(void);

/* Applies a supported local-time policy. It is called by app_loop after the
 * persisted onboarding preference changes, never by the LVGL task. */
esp_err_t time_service_set_timezone_index(uint16_t timezone_index);

void time_service_get_status(time_service_status_t *out_status);

#ifdef __cplusplus
}
#endif
