/*
 * First local hardware bring-up for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 * This module deliberately owns only P4-local display, touch and PSRAM setup.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_bringup_start(void);

#ifdef __cplusplus
}
#endif
