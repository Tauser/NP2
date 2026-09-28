#include "np_settings_wifi.h"

#include <stdio.h>

#define SETTINGS_WIFI_MODAL_X 172
#define SETTINGS_WIFI_MODAL_Y 84
#define SETTINGS_WIFI_MODAL_W 680
#define SETTINGS_WIFI_MODAL_H 432

static void row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        np_settings_wifi_show(lv_event_get_user_data(event));
    }
}

static lv_obj_t *network_row(np_settings_wifi_t *wifi, uint8_t index)
{
    lv_obj_t *const row = np_fill(wifi->modal.content, 344, 36 + index * 66,
                                  312, 58, np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    wifi->network_names[index] = np_label(row, "", NP_FONT_SM, np_c_text(),
                                          16, 8, 214, LV_TEXT_ALIGN_LEFT);
    wifi->network_details[index] = np_label(row, "", NP_FONT_SM, np_c_text_2(),
                                            16, 31, 260, LV_TEXT_ALIGN_LEFT);
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

    np_label(content, "Status", NP_FONT_SM, np_c_text_3(),
             24, 30, 260, LV_TEXT_ALIGN_LEFT);
    wifi->status_value = np_label(content, "--", NP_FONT_MD, np_c_text(),
                                  24, 57, 260, LV_TEXT_ALIGN_LEFT);
    wifi->detail_value = np_label(content, "Aguardando estado da rede", NP_FONT_SM,
                                  np_c_text_2(), 24, 89, 270, LV_TEXT_ALIGN_LEFT);
    np_hline(content, 24, 128, 286);
    np_label(content, "Configuracao protegida", NP_FONT_SM, np_c_text_3(),
             24, 150, 270, LV_TEXT_ALIGN_LEFT);
    np_label(content, "SSID e senha permanecem no servico", NP_FONT_SM, np_c_text_2(),
             24, 176, 280, LV_TEXT_ALIGN_LEFT);
    wifi->manage_button = np_button(content, 24, 237, 286, 52,
                                    "Gerenciar rede", true);

    np_label(content, "Redes disponiveis", NP_FONT_SM, np_c_text_3(),
             344, 10, 280, LV_TEXT_ALIGN_LEFT);
    for (uint8_t i = 0; i < NP_SETTINGS_WIFI_VISIBLE_RESULTS; ++i) {
        wifi->network_rows[i] = network_row(wifi, i);
        np_set_visible(wifi->network_rows[i], false);
    }
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
}
