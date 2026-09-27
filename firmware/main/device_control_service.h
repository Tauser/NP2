/* Runtime controls for board-local display and audio hardware. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool ready;
    uint8_t brightness_percent;
    uint8_t volume_percent;
    bool audio_ready;
    esp_err_t brightness_result;
    esp_err_t volume_result;
    bool persistence_pending;
    uint32_t save_completion_id;
    uint8_t save_completion_mask;
    esp_err_t save_result;
} device_control_status_t;

#define DEVICE_CONTROL_BRIGHTNESS_MASK 0x01U
#define DEVICE_CONTROL_VOLUME_MASK     0x02U

/* Starts the owner task. It restores and persists preferences asynchronously
 * through FlashCoordinator; touch callbacks only submit requests. */
esp_err_t device_control_service_start(void);

/* Coalesced, non-blocking requests safe to issue from the LVGL task. */
esp_err_t device_control_set_brightness(uint8_t percent);
esp_err_t device_control_set_volume(uint8_t percent);

void device_control_get_status(device_control_status_t *out_status);

#ifdef __cplusplus
}
#endif
