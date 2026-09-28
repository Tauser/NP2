#include "np_settings_wifi.h"

#include <stdio.h>
#include <string.h>

#define SETTINGS_WIFI_MODAL_X 160
#define SETTINGS_WIFI_MODAL_Y 42
#define SETTINGS_WIFI_MODAL_W 704
#define SETTINGS_WIFI_MODAL_H 516

static void row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        np_settings_wifi_show(lv_event_get_user_data(event));
    }
}

static void select_row(np_settings_wifi_t *wifi, uint8_t selected)
{
    if (wifi->selected_index == selected) return;
    wifi->selected_index = selected;
    if (selected < NP_SETTINGS_WIFI_VISIBLE_RESULTS) {
        (void)snprintf(wifi->selected_ssid, sizeof(wifi->selected_ssid), "%s",
                       lv_label_get_text(wifi->network_names[selected]));
        lv_obj_remove_state(wifi->connect_button, LV_STATE_DISABLED);
    } else {
        wifi->selected_ssid[0] = '\0';
        lv_obj_add_state(wifi->connect_button, LV_STATE_DISABLED);
    }
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        const bool active = i == selected;
        np_set_bg_color(wifi->network_rows[i], active ? np_c_accent_bg() : np_c_surface_raised());
        lv_obj_set_style_border_color(wifi->network_rows[i],
                                      active ? np_c_accent() : np_c_hairline(), 0);
        lv_obj_set_style_border_width(wifi->network_rows[i], active ? 2 : 1, 0);
    }
    np_set_text(lv_obj_get_child(wifi->connect_button, 0),
                selected < NP_SETTINGS_WIFI_VISIBLE_RESULTS ? "Conectar rede" : "Selecione uma rede");
}

static void network_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_wifi_t *const wifi = lv_event_get_user_data(event);
    if (wifi == NULL) return;
    lv_obj_t *const row = lv_event_get_target(event);
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        if (wifi->network_rows[i] == row) {
            select_row(wifi, i);
            return;
        }
    }
}

static lv_obj_t *network_lock(lv_obj_t *parent)
{
    lv_obj_t *const root = np_group(parent, 246, 12, 20, 24);
    lv_obj_t *const shackle = lv_arc_create(root);
    lv_obj_remove_style_all(shackle);
    lv_obj_set_pos(shackle, 4, 1);
    lv_obj_set_size(shackle, 12, 14);
    lv_obj_set_style_pad_all(shackle, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(shackle, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(shackle, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(shackle, np_c_text_2(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(shackle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(shackle, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(shackle, 180, 360);
    lv_obj_remove_flag(shackle, LV_OBJ_FLAG_CLICKABLE);
    (void)np_fill(root, 3, 11, 14, 10, np_c_text_2(), LV_OPA_COVER, 2);
    return root;
}

static lv_obj_t *network_row(np_settings_wifi_t *wifi, uint8_t index)
{
    lv_obj_t *const row = np_fill(wifi->modal.content, 360, 70 + index * 49,
                                  320, 48, np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, np_c_hairline(), 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    wifi->network_names[index] = np_label(row, "", NP_FONT_SM, np_c_text(),
                                          14, 16, 224, LV_TEXT_ALIGN_LEFT);
    wifi->network_locks[index] = network_lock(row);
    wifi->network_signals[index] = np_label(row, "", NP_FONT_ICON, np_c_text_2(),
                                            278, 12, 28, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(row, network_event_cb, LV_EVENT_CLICKED, wifi);
    return row;
}

void np_settings_wifi_create(np_settings_wifi_t *wifi, lv_obj_t *parent)
{
    if (wifi == NULL || parent == NULL) return;
    *wifi = (np_settings_wifi_t){0};
    wifi->selected_index = NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    np_modal_create(&wifi->modal, parent,
                    SETTINGS_WIFI_MODAL_X, SETTINGS_WIFI_MODAL_Y,
                    SETTINGS_WIFI_MODAL_W, SETTINGS_WIFI_MODAL_H,
                    NP_ICON_WIFI, np_c_positive(), "Wi-Fi", "Rede e conectividade");

    lv_obj_t *const content = wifi->modal.content;
    if (content == NULL) return;

    lv_obj_t *const status = np_fill(content, 24, 20, 316, 306,
                                     np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(status, "Status", NP_FONT_SM, np_c_text_3(), 28, 28, 220, LV_TEXT_ALIGN_LEFT);
    wifi->status_icon = np_label(status, NP_ICON_WIFI_OFF, NP_FONT_ICON, np_c_text_3(),
                                 28, 64, 28, LV_TEXT_ALIGN_CENTER);
    wifi->detail_value = np_label(status, "Nenhuma rede associada", NP_FONT_MD,
                                  np_c_text_2(), 68, 69, 220, LV_TEXT_ALIGN_LEFT);
    np_hline(status, 28, 119, 260);
    np_label(status, "Credenciais protegidas pelo servico", NP_FONT_SM,
             np_c_text_3(), 28, 145, 260, LV_TEXT_ALIGN_LEFT);
    wifi->manage_button = np_button(status, 28, 204, 260, 52, "Gerenciar redes", false);
    wifi->forget_button = np_button(content, 24, 342, 316, 52, "Esquecer rede", false);

    lv_obj_t *const networks = np_fill(content, 360, 20, 320, 306,
                                       np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(networks, "Redes disponiveis", NP_FONT_MD, np_c_text_2(),
             16, 21, 230, LV_TEXT_ALIGN_LEFT);
    wifi->scan_button = np_icon_button(networks, 264, 12, 42, NP_ICON_SEARCH);
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        wifi->network_rows[i] = network_row(wifi, i);
        np_set_visible(wifi->network_rows[i], false);
    }
    wifi->connect_button = np_button(content, 360, 342, 320, 52, "Selecione uma rede", true);
    lv_obj_add_state(wifi->connect_button, LV_STATE_DISABLED);
    lv_obj_set_style_opa(wifi->connect_button, LV_OPA_40, LV_STATE_DISABLED);
}

void np_settings_wifi_show(np_settings_wifi_t *wifi)
{
    if (wifi != NULL) np_modal_show(&wifi->modal);
}

void np_settings_wifi_hide(np_settings_wifi_t *wifi)
{
    if (wifi != NULL) np_modal_hide(&wifi->modal);
}

void np_settings_wifi_bind_row(np_settings_wifi_t *wifi, lv_obj_t *row)
{
    if (wifi != NULL && row != NULL) {
        lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, wifi);
    }
}

void np_settings_wifi_sync(np_settings_wifi_t *wifi, np_wifi_status_t status,
                           const char *connected_ssid,
                           uint8_t scan_results_count,
                           const connectivity_scan_result_t *scan_results)
{
    if (wifi == NULL) return;
    const char *status_icon = NP_ICON_WIFI_OFF;
    const char *status_text = "Nenhuma rede associada";
    lv_color_t status_color = np_c_text_3();
    switch (status) {
    case NP_WIFI_STATUS_ONLINE:
        status_icon = NP_ICON_WIFI;
        status_text = connected_ssid != NULL && connected_ssid[0] != '\0'
                          ? connected_ssid : "Conectado";
        status_color = np_c_positive();
        break;
    case NP_WIFI_STATUS_ASSOCIATED_PENDING_IP:
        status_icon = NP_ICON_WIFI_OFF;
        status_text = connected_ssid != NULL && connected_ssid[0] != '\0'
                          ? connected_ssid : "Aguardando IP";
        status_color = np_c_warning();
        break;
    case NP_WIFI_STATUS_CONNECTING:
        status_icon = NP_ICON_SEARCH;
        status_text = "Conectando";
        status_color = np_c_accent();
        break;
    case NP_WIFI_STATUS_FAILED:
        status_icon = NP_ICON_WARNING;
        status_text = "Conexao indisponivel";
        status_color = np_c_negative();
        break;
    case NP_WIFI_STATUS_OFFLINE:
    default:
        break;
    }
    np_set_text(wifi->status_icon, status_icon);
    np_set_text_color(wifi->status_icon, status_color);
    np_set_text(wifi->detail_value, status_text);
    np_set_text_color(wifi->detail_value, status_color);

    /* Settings projects network state before this lazy modal exists. */
    if (wifi->network_rows[0] == NULL) return;

    const uint8_t count = scan_results_count < NP_SETTINGS_WIFI_VISIBLE_RESULTS
                              ? scan_results_count : NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    uint8_t selected = NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        const bool visible = scan_results != NULL && i < count &&
                             scan_results[i].ssid[0] != '\0';
        np_set_visible(wifi->network_rows[i], visible);
        if (!visible) continue;

        np_set_text(wifi->network_names[i], scan_results[i].ssid);
        np_set_visible(wifi->network_locks[i], scan_results[i].secure);
        np_set_text(wifi->network_signals[i], np_wifi_signal_icon(scan_results[i].rssi));
        wifi->network_secure[i] = scan_results[i].secure;
        if (strcmp(wifi->selected_ssid, scan_results[i].ssid) == 0) selected = i;
    }
    select_row(wifi, selected);
}

bool np_settings_wifi_copy_selected_ssid(const np_settings_wifi_t *wifi,
                                         char *out_ssid, size_t out_size)
{
    if (wifi == NULL || out_ssid == NULL || out_size == 0U ||
        wifi->selected_index >= NP_SETTINGS_WIFI_VISIBLE_RESULTS ||
        wifi->network_names[wifi->selected_index] == NULL ||
        lv_obj_has_flag(wifi->network_rows[wifi->selected_index], LV_OBJ_FLAG_HIDDEN)) return false;
    const char *const ssid = wifi->selected_ssid;
    const size_t length = strnlen(ssid, out_size);
    if (length == 0U || length >= out_size) return false;
    memcpy(out_ssid, ssid, length + 1U);
    return true;
}
