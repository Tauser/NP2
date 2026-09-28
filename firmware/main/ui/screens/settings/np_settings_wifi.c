#include "np_settings_wifi.h"

#include <stdio.h>

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
    wifi->selected_index = selected;
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        const bool active = i == selected;
        np_set_bg_color(wifi->network_rows[i], active ? np_c_accent_bg() : np_c_surface_raised());
        lv_obj_set_style_border_color(wifi->network_rows[i],
                                      active ? np_c_accent() : np_c_hairline(), 0);
    }
}

static void network_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_wifi_t *const wifi = lv_event_get_user_data(event);
    const uint8_t index = (uint8_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(event));
    if (wifi != NULL) select_row(wifi, index);
}

static lv_obj_t *network_row(np_settings_wifi_t *wifi, uint8_t index)
{
    lv_obj_t *const row = np_fill(wifi->modal.content, 360, 70 + index * 59,
                                  320, 54, np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, np_c_hairline(), 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(row, (void *)(uintptr_t)index);
    np_label(row, NP_ICON_WIFI, NP_FONT_ICON, np_c_text(), 14, 14, 28, LV_TEXT_ALIGN_CENTER);
    wifi->network_names[index] = np_label(row, "", NP_FONT_SM, np_c_text(),
                                          58, 6, 190, LV_TEXT_ALIGN_LEFT);
    wifi->network_details[index] = np_label(row, "", NP_FONT_SM, np_c_text_2(),
                                            58, 29, 206, LV_TEXT_ALIGN_LEFT);
    np_label(row, NP_ICON_WIFI, NP_FONT_ICON, np_c_text_2(), 278, 14, 28, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(row, network_event_cb, LV_EVENT_CLICKED, wifi);
    return row;
}

void np_settings_wifi_create(np_settings_wifi_t *wifi, lv_obj_t *parent)
{
    if (wifi == NULL || parent == NULL) return;
    *wifi = (np_settings_wifi_t){0};
    np_modal_create(&wifi->modal, parent,
                    SETTINGS_WIFI_MODAL_X, SETTINGS_WIFI_MODAL_Y,
                    SETTINGS_WIFI_MODAL_W, SETTINGS_WIFI_MODAL_H,
                    NP_ICON_WIFI, np_c_positive(), "Wi-Fi", "Rede e conectividade");

    lv_obj_t *const content = wifi->modal.content;
    if (content == NULL) return;

    lv_obj_t *const status = np_fill(content, 24, 20, 316, 306,
                                     np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(status, "Status", NP_FONT_SM, np_c_text_3(), 28, 28, 220, LV_TEXT_ALIGN_LEFT);
    wifi->status_value = np_label(status, "--", NP_FONT_MD, np_c_text(),
                                  28, 69, 240, LV_TEXT_ALIGN_LEFT);
    wifi->detail_value = np_label(status, "Aguardando estado da rede", NP_FONT_SM,
                                  np_c_text_2(), 28, 112, 260, LV_TEXT_ALIGN_LEFT);
    np_hline(status, 28, 155, 260);
    np_label(status, "Credenciais protegidas pelo servico", NP_FONT_SM,
             np_c_text_3(), 28, 181, 260, LV_TEXT_ALIGN_LEFT);
    wifi->manage_button = np_button(status, 28, 224, 260, 52, "Gerenciar redes", false);
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
    np_hline(content, 24, 418, 656);
    wifi->connect_button = np_button(content, 500, 438, 180, 52, "Conectar", true);
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

void np_settings_wifi_sync(np_settings_wifi_t *wifi, bool online,
                           uint8_t scan_results_count,
                           const connectivity_scan_result_t *scan_results)
{
    if (wifi == NULL) return;
    np_set_text(wifi->status_value, online ? "Conectado" : "Sem conexao");
    np_set_text(wifi->detail_value, online ? "Internet disponivel" :
                                            "Nenhuma rede ativa");

    const uint8_t count = scan_results_count < NP_SETTINGS_WIFI_VISIBLE_RESULTS
                              ? scan_results_count : NP_SETTINGS_WIFI_VISIBLE_RESULTS;
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        const bool visible = scan_results != NULL && i < count &&
                             scan_results[i].ssid[0] != '\0';
        np_set_visible(wifi->network_rows[i], visible);
        if (!visible) continue;

        char detail[32] = {0};
        (void)snprintf(detail, sizeof(detail), "%d dBm · %s",
                       (int)scan_results[i].rssi,
                       scan_results[i].secure ? "Protegida" : "Aberta");
        np_set_text(wifi->network_names[i], scan_results[i].ssid);
        np_set_text(wifi->network_details[i], detail);
    }
    if (count > 0U) select_row(wifi, wifi->selected_index < count ? wifi->selected_index : 0U);
}
