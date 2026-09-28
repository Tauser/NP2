#pragma once

#include "connectivity_diagnostic.h"
#include "np_modal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NP_SETTINGS_WIFI_VISIBLE_RESULTS 4U

typedef struct {
    np_modal_t modal;
    lv_obj_t *status_value;
    lv_obj_t *detail_value;
    lv_obj_t *manage_button;
    lv_obj_t *network_rows[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    lv_obj_t *network_names[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    lv_obj_t *network_details[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
} np_settings_wifi_t;

void np_settings_wifi_create(np_settings_wifi_t *wifi, lv_obj_t *parent);
void np_settings_wifi_show(np_settings_wifi_t *wifi);
void np_settings_wifi_hide(np_settings_wifi_t *wifi);
void np_settings_wifi_bind_row(np_settings_wifi_t *wifi, lv_obj_t *row);
void np_settings_wifi_sync(np_settings_wifi_t *wifi, bool online,
                           uint8_t scan_results_count,
                           const connectivity_scan_result_t *scan_results);

#ifdef __cplusplus
}
#endif
