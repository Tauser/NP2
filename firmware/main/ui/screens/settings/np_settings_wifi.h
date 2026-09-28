#pragma once

#include "connectivity_diagnostic.h"
#include "np_modal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NP_SETTINGS_WIFI_VISIBLE_RESULTS 5U

typedef enum {
    NP_WIFI_STATUS_OFFLINE = 0,
    NP_WIFI_STATUS_ASSOCIATED_PENDING_IP,
    NP_WIFI_STATUS_ONLINE,
    NP_WIFI_STATUS_CONNECTING,
    NP_WIFI_STATUS_FAILED,
} np_wifi_status_t;

typedef struct {
    np_modal_t modal;
    lv_obj_t *status_icon;
    lv_obj_t *detail_value;
    lv_obj_t *scan_button;
    lv_obj_t *manage_button;
    lv_obj_t *forget_button;
    lv_obj_t *connect_button;
    lv_obj_t *network_rows[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    lv_obj_t *network_names[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    lv_obj_t *network_locks[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    lv_obj_t *network_signals[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
    uint8_t selected_index;
    char selected_ssid[33];
    bool network_secure[NP_SETTINGS_WIFI_VISIBLE_RESULTS];
} np_settings_wifi_t;

void np_settings_wifi_create(np_settings_wifi_t *wifi, lv_obj_t *parent);
void np_settings_wifi_show(np_settings_wifi_t *wifi);
void np_settings_wifi_hide(np_settings_wifi_t *wifi);
void np_settings_wifi_bind_row(np_settings_wifi_t *wifi, lv_obj_t *row);
void np_settings_wifi_sync(np_settings_wifi_t *wifi, np_wifi_status_t status,
                           const char *connected_ssid,
                           uint8_t scan_results_count,
                           const connectivity_scan_result_t *scan_results);
bool np_settings_wifi_copy_selected_ssid(const np_settings_wifi_t *wifi,
                                         char *out_ssid, size_t out_size);

#ifdef __cplusplus
}
#endif
