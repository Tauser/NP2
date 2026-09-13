/* First product-facing offline view for Phase 4. */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t offline_dashboard_create(lv_display_t *display, lv_indev_t *touch_indev);

#ifdef __cplusplus
}
#endif
