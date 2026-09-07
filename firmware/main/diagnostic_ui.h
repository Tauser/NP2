/* Phase 2 touch diagnostic. This is not product UI. */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t diagnostic_ui_create(lv_display_t *display, lv_indev_t *touch_indev);

#ifdef __cplusplus
}
#endif
