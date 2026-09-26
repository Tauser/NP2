/*
 * First local hardware bring-up for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 * This module deliberately owns only P4-local display, touch and PSRAM setup.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_bringup_start(void);

typedef struct {
    bool display_ready;
    bool first_frame_presented;
    uint64_t last_ui_progress_ms;
} board_bringup_health_t;

/* First render plus LVGL-owner timer heartbeat, including static screens. */
void board_bringup_get_health(board_bringup_health_t *out_health);

#ifdef __cplusplus
}
#endif
