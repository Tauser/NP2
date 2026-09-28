/* Touch-only WPA2 provisioning modal. All calls occur in the LVGL task. */
#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_setup_view_open(lv_obj_t *parent);
esp_err_t wifi_setup_view_open_for_ssid(lv_obj_t *parent, const char *ssid);

#ifdef __cplusplus
}
#endif
