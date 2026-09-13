#include "offline_dashboard.h"

#include <stdio.h>

#include "app_state.h"
#include "diagnostic_ui.h"
#include "esp_check.h"
#include "offline_value_format.h"

#define DASHBOARD_REFRESH_PERIOD_MS 1000U

typedef struct {
    lv_display_t *display;
    lv_indev_t *touch_indev;
    lv_obj_t *network_label;
    lv_obj_t *storage_label;
    lv_obj_t *weather_label;
    lv_obj_t *market_label;
    lv_obj_t *status_label;
    lv_timer_t *refresh_timer;
} offline_dashboard_state_t;

static offline_dashboard_state_t s_state;

static const char *network_state_text(app_network_state_t state)
{
    switch (state) {
    case APP_NETWORK_STATE_ONLINE:
        return "conectado";
    case APP_NETWORK_STATE_ASSOCIATING:
    case APP_NETWORK_STATE_WAITING_FOR_IP:
        return "conectando";
    case APP_NETWORK_STATE_BACKOFF:
    case APP_NETWORK_STATE_RECOVERING_LINK:
        return "recuperando";
    case APP_NETWORK_STATE_LINK_UP:
    case APP_NETWORK_STATE_WIFI_READY:
    case APP_NETWORK_STATE_SCANNING:
    case APP_NETWORK_STATE_SCAN_COMPLETE:
        return "sem credencial";
    case APP_NETWORK_STATE_FAILED:
    case APP_NETWORK_STATE_LINK_DOWN:
        return "offline";
    case APP_NETWORK_STATE_IDLE:
    case APP_NETWORK_STATE_STARTING:
    default:
        return "inicializando";
    }
}

static const char *snapshot_state_text(bool stale)
{
    return stale ? "DADO LOCAL ANTIGO" : "DADO LOCAL";
}

static const char *age_text(uint32_t age_s, char *out_text, size_t out_size)
{
    if (age_s == UINT32_MAX) {
        return "hora nao confiavel";
    }
    const unsigned long minutes = (unsigned long)(age_s / 60U);
    const int written = snprintf(out_text, out_size, "ha %lu min", minutes);
    return written > 0 && (size_t)written < out_size ? out_text : "idade indisponivel";
}

static void set_weather_label(const offline_weather_data_t *weather, uint32_t age_s)
{
    if (!weather->available) {
        lv_label_set_text(s_state.weather_label, "CLIMA\nNenhuma leitura local valida.");
        return;
    }
    char temperature[12] = {0};
    char age[24] = {0};
    if (!offline_format_temperature(weather->temperature_deci_c, temperature, sizeof(temperature))) {
        (void)snprintf(temperature, sizeof(temperature), "--.-");
    }
    lv_label_set_text_fmt(s_state.weather_label, "CLIMA | %s\n%s C | umidade %u%% | %s",
                          snapshot_state_text(weather->stale), temperature,
                          (unsigned int)weather->relative_humidity_percent,
                          age_text(age_s, age, sizeof(age)));
}

static void set_market_label(const offline_market_data_t *market, uint32_t age_s)
{
    if (!market->available) {
        lv_label_set_text(s_state.market_label, "MERCADO\nNenhuma leitura local valida.");
        return;
    }
    char change[16] = {0};
    char age[24] = {0};
    if (!offline_format_market_change(market->change_24h_basis_points, change, sizeof(change))) {
        (void)snprintf(change, sizeof(change), "--.--");
    }
    lv_label_set_text_fmt(s_state.market_label,
                          "BTC/USD | %s\nUS$ %lu.%02lu | 24h %s%% | %s",
                          snapshot_state_text(market->stale),
                          (unsigned long)(market->bitcoin_usd_cents / 100U),
                          (unsigned long)(market->bitcoin_usd_cents % 100U),
                          change, age_text(age_s, age, sizeof(age)));
}

static void refresh_dashboard(void)
{
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    if (!projection.ready) {
        lv_label_set_text(s_state.status_label, "Iniciando o estado local...");
        return;
    }

    const app_network_projection_t *const network = &projection.network;
    const app_storage_projection_t *const storage = &projection.storage;
    set_weather_label(&projection.offline_data.weather, projection.weather_age_s);
    set_market_label(&projection.offline_data.market, projection.market_age_s);
    lv_label_set_text_fmt(s_state.network_label,
                          "REDE LOCAL\n%s | %s | %u APs visiveis",
                          network_state_text(network->state),
                          network->online ? "IP ativo" : "sem IP",
                          (unsigned int)network->access_points_found);

    if (storage->cache_valid) {
        lv_label_set_text_fmt(s_state.storage_label,
                              "CACHE LOCAL\nGeracao %lu integra | cache v%u\ndados v%u",
                              (unsigned long)storage->cache_generation,
                              (unsigned int)storage->cache_schema_version,
                              (unsigned int)projection.offline_data.schema_version);
    } else if (storage->cache_result == ESP_ERR_NOT_FOUND) {
        lv_label_set_text(s_state.storage_label,
                          "CACHE LOCAL\nNenhum dado salvo ainda | modo offline seguro");
    } else {
        lv_label_set_text(s_state.storage_label,
                          "CACHE LOCAL\nIndisponivel | dados locais nao serao substituidos");
    }

    lv_label_set_text_fmt(s_state.status_label,
                          "Estado local r%lu | hora %s | persistencia %s",
                          (unsigned long)projection.revision,
                          projection.time_trusted ? "confiavel" : "nao confiavel",
                          storage->busy || storage->pending ? "em andamento" : "pronta");
}

static lv_obj_t *create_card(lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
                             int32_t height, const char *text, lv_color_t color)
{
    lv_obj_t *const card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x13253B), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *const label = lv_label_create(card);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    return label;
}

static void diagnostics_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_timer_delete(s_state.refresh_timer);
    lv_display_t *const display = s_state.display;
    lv_indev_t *const touch_indev = s_state.touch_indev;
    s_state = (offline_dashboard_state_t){0};
    lv_obj_clean(lv_screen_active());
    (void)diagnostic_ui_create(display, touch_indev);
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_dashboard();
}

esp_err_t offline_dashboard_create(lv_display_t *display, lv_indev_t *touch_indev)
{
    ESP_RETURN_ON_FALSE(display != NULL && touch_indev != NULL, ESP_ERR_INVALID_ARG,
                        "offline_ui", "Display or touch handle missing");

    s_state = (offline_dashboard_state_t){
        .display = display,
        .touch_indev = touch_indev,
    };
    lv_obj_t *const screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x09111F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *const title = lv_label_create(screen);
    lv_label_set_text(title, "NP2  |  PAINEL LOCAL");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 42, 28);

    lv_obj_t *const subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Funciona sem internet. Dados externos aparecerao quando estiverem integros.");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 42, 62);

    s_state.weather_label = create_card(screen, 42, 108, 456, 116,
                                         "CLIMA\nCarregando dado local...", lv_color_hex(0x4EA3FF));
    s_state.market_label = create_card(screen, 526, 108, 456, 116,
                                        "MERCADO\nCarregando dado local...", lv_color_hex(0x68E0B8));
    s_state.network_label = create_card(screen, 42, 248, 456, 116,
                                         "REDE LOCAL\nIniciando...", lv_color_hex(0xF4C95D));
    s_state.storage_label = create_card(screen, 526, 248, 456, 116,
                                         "CACHE LOCAL\nVerificando...", lv_color_hex(0xC18CFF));

    lv_obj_t *const diagnostics_button = lv_button_create(screen);
    lv_obj_set_size(diagnostics_button, 310, 50);
    lv_obj_align(diagnostics_button, LV_ALIGN_BOTTOM_MID, 0, -74);
    lv_obj_set_style_radius(diagnostics_button, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(diagnostics_button, lv_color_hex(0x183554), LV_PART_MAIN);
    lv_obj_t *const diagnostics_label = lv_label_create(diagnostics_button);
    lv_label_set_text(diagnostics_label, "ABRIR DIAGNOSTICO DE BANCADA");
    lv_obj_set_style_text_color(diagnostics_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(diagnostics_label);
    lv_obj_add_event_cb(diagnostics_button, diagnostics_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.status_label = lv_label_create(screen);
    lv_label_set_text(s_state.status_label, "Iniciando o estado local...");
    lv_obj_set_style_text_color(s_state.status_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.status_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    refresh_dashboard();
    s_state.refresh_timer = lv_timer_create(refresh_timer_cb, DASHBOARD_REFRESH_PERIOD_MS, NULL);
    return s_state.refresh_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK;
}
