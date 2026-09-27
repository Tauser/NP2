/* Runtime controls for board-local display and audio hardware. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t brightness_percent;
    uint8_t volume_percent;
    bool audio_ready;
    esp_err_t brightness_result;
    esp_err_t volume_result;
} device_control_status_t;

/* Starts the owner task. Values are runtime-only and deliberately do not
 * write NVS from a touch callback. */
esp_err_t device_control_service_start(void);

/* Coalesced, non-blocking requests safe to issue from the LVGL task. */
esp_err_t device_control_set_brightness(uint8_t percent);
esp_err_t device_control_set_volume(uint8_t percent);

void device_control_get_status(device_control_status_t *out_status);

#ifdef __cplusplus
}
#endif
