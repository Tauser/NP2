/* SD-backed animated weather icon loader. It performs file I/O outside LVGL. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "weather_condition.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ready;
    bool mounted;
    bool pending;
    const void *icon_source;
    bool icon_available;
    bool icon_is_day;
    weather_condition_t icon_condition;
    esp_err_t icon_result;
    uint32_t generation;
    uint32_t load_count;
    esp_err_t last_result;
} weather_asset_service_status_t;

/* Starts one bounded worker. It mounts the card read-only from the firmware's
 * perspective and never asks the BSP to format it on a mount failure. */
esp_err_t weather_asset_service_start(void);

/* Coalesces the desired visual. This only updates a mailbox; file reads happen
 * later in the service worker. */
esp_err_t weather_asset_service_request(uint8_t hour, uint16_t weather_code);

/* Lock-protected status copy for app_loop. The icon source remains valid until
 * at least the next two successful loads, using two PSRAM buffers. */
void weather_asset_service_get_status(weather_asset_service_status_t *out_status);

#ifdef __cplusplus
}
#endif
