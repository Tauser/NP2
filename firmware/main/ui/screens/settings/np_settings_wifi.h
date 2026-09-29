#pragma once

#include "connectivity_diagnostic.h"
#include "np_modal.h"
#include "np_keyboard.h"

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

typedef struct np_settings_wifi_view np_settings_wifi_view_t;

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
    /* Non-NULL only for the dedicated scene; legacy modal stays supported. */
    np_settings_wifi_view_t *scene;
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

struct np_settings_wifi_view {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button, *add_button;
    np_settings_wifi_t wifi;
    lv_obj_t *current_row, *current_icon, *current_name, *current_status;
    lv_obj_t *selected_name, *selected_status, *selected_icon;
    lv_obj_t *signal, *security, *ip, *gateway;
    lv_obj_t *previous_page, *next_page, *page_label, *empty_label;
    connectivity_scan_result_t results[CONNECTIVITY_DIAGNOSTIC_MAX_SCAN_RESULTS];
    char connected_ssid[33], ip_address[16], gateway_address[16];
    np_wifi_status_t status;
    uint8_t count, page;
};

np_settings_wifi_view_t np_settings_wifi_scene_build(lv_obj_t *parent);
void np_settings_wifi_scene_sync(np_settings_wifi_view_t *view, np_wifi_status_t status,
    const char *connected_ssid, uint8_t count, const connectivity_scan_result_t *results,
    const char *ip, const char *gateway, bool scanning);
/* Attach callbacks only after storing the returned view at its final address. */
void np_settings_wifi_scene_bind(np_settings_wifi_view_t *view);

/* Public SSID only. Password entry remains a separate provisioning session. */
typedef struct {
    np_modal_t modal;
    lv_obj_t *ssid, *secure_switch, *continue_button;
    np_keyboard_t *keyboard;
} np_settings_wifi_add_t;
void np_settings_wifi_add_create(np_settings_wifi_add_t *view, lv_obj_t *parent,
                                 np_keyboard_t *keyboard);

#ifdef __cplusplus
}
#endif
