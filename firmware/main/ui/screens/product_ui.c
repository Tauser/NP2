#include "product_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_state.h"
#include "diagnostic_ui.h"
#include "esp_check.h"
#include "esp_log.h"
#include "offline_value_format.h"
#include "np_screens.h"
#include "weather_condition.h"

#define UI_REFRESH_PERIOD_MS 250U
#define BOOT_MINIMUM_MS 1200U
#define BOOT_MAXIMUM_MS 6000U

typedef struct {
    lv_display_t *display;
    lv_indev_t *touch_indev;
    np_boot_view_t boot;
    np_home_view_t home;
    lv_timer_t *refresh_timer;
    uint32_t started_at_tick;
    uint32_t rendered_revision;
    bool home_visible;
} product_ui_state_t;

static product_ui_state_t s_ui;
static const char *const TAG = "product_ui";

static np_data_state_t data_state(bool available, bool stale)
{
    if (!available) return NP_DATA_UNAVAILABLE;
    return stale ? NP_DATA_STALE : NP_DATA_LIVE;
}

static bool projection_local_time(const app_ui_projection_t *projection, struct tm *local)
{
    if (projection == NULL || local == NULL ||
        !projection->time_trusted || projection->current_unix_s == 0U) {
        return false;
    }

    const time_t now = (time_t)projection->current_unix_s;
    return localtime_r(&now, local) != NULL;
}

static void format_grouped_u32(uint32_t value, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) return;

    if (value >= 1000000U) {
        (void)snprintf(out, out_size, "%lu.%03lu.%03lu",
                       (unsigned long)(value / 1000000U),
                       (unsigned long)((value / 1000U) % 1000U),
                       (unsigned long)(value % 1000U));
    } else if (value >= 1000U) {
        (void)snprintf(out, out_size, "%lu.%03lu",
                       (unsigned long)(value / 1000U),
                       (unsigned long)(value % 1000U));
    } else {
        (void)snprintf(out, out_size, "%lu", (unsigned long)value);
    }
}

static void format_btc_whole(uint32_t cents, char *out, size_t out_size)
{
    format_grouped_u32(cents / 100U, out, out_size);
}

static void format_basis_points(int16_t basis_points, char *out, size_t out_size)
{
    char value[16] = {0};

    if (!offline_format_market_change(basis_points, value, sizeof(value))) {
        (void)snprintf(out, out_size, "--");
        return;
    }

    char *separator = strchr(value, '.');
    if (separator != NULL) *separator = ',';

    (void)snprintf(out, out_size, "%s%s%%",
                   basis_points > 0 ? "+" : "", value);
}

#if OFFLINE_DATA_SCHEMA_VERSION >= 4

static void format_temperature_deci(int16_t deci_c, char *out, size_t out_size)
{
    char value[16] = {0};
    if (!offline_format_temperature(deci_c, value, sizeof(value))) {
        (void)snprintf(out, out_size, "--");
        return;
    }

    char *separator = strchr(value, '.');
    if (separator != NULL) *separator = ',';
    (void)snprintf(out, out_size, "%s°", value);
}

static void format_usd_metric(uint32_t cents, char *out, size_t out_size)
{
    char whole[20] = {0};
    format_btc_whole(cents, whole, sizeof(whole));
    (void)snprintf(out, out_size, "%s", whole);
}

static void format_volume_short(uint64_t cents, char *out, size_t out_size)
{
    const uint64_t dollars = cents / 100U;

    if (dollars >= UINT64_C(1000000000)) {
        const uint64_t whole = dollars / UINT64_C(1000000000);
        const uint64_t hundredths =
            (dollars % UINT64_C(1000000000)) / UINT64_C(10000000);
        (void)snprintf(out, out_size, "%llu,%02llu bi",
                       (unsigned long long)whole,
                       (unsigned long long)hundredths);
    } else if (dollars >= UINT64_C(1000000)) {
        const uint64_t whole = dollars / UINT64_C(1000000);
        const uint64_t hundredths =
            (dollars % UINT64_C(1000000)) / UINT64_C(10000);
        (void)snprintf(out, out_size, "%llu,%02llu mi",
                       (unsigned long long)whole,
                       (unsigned long long)hundredths);
    } else {
        (void)snprintf(out, out_size, "%llu",
                       (unsigned long long)dollars);
    }
}

#endif

/* ------------------------------------------------------------------ */
/* Header                                                              */
/* ------------------------------------------------------------------ */

static void update_clock(const app_ui_projection_t *projection)
{
    struct tm local = {0};

    if (!projection_local_time(projection, &local)) {
        np_clock_set(&s_ui.home.header.clock, "--:--");
        np_set_text(s_ui.home.weather_date, "Data indisponível");
        return;
    }

    static const char *const weekdays_long[] = {
        "Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira",
        "Quinta-feira", "Sexta-feira", "Sábado",
    };
    static const char *const months[] = {
        "janeiro", "fevereiro", "março", "abril", "maio", "junho",
        "julho", "agosto", "setembro", "outubro", "novembro", "dezembro",
    };

    char clock[6] = {0};
    char weather_date[64] = {0};

    (void)snprintf(clock, sizeof(clock), "%02d:%02d",
                   local.tm_hour, local.tm_min);
    (void)snprintf(weather_date, sizeof(weather_date), "%s, %d de %s",
                   weekdays_long[local.tm_wday], local.tm_mday,
                   months[local.tm_mon]);

    np_clock_set(&s_ui.home.header.clock, clock);
    np_set_text(s_ui.home.weather_date, weather_date);
}

/* ------------------------------------------------------------------ */
/* Clima                                                               */
/* ------------------------------------------------------------------ */

static void update_weather(const app_ui_projection_t *projection)
{
    const offline_weather_data_t *weather = &projection->offline_data.weather;

    np_home_set_weather_icon_source(
        &s_ui.home, projection->weather_assets.icon_source);

    np_status_dot_set(&s_ui.home.weather_status,
                      data_state(weather->available, weather->stale));

    if (!weather->available) {
        np_set_text(s_ui.home.weather_temperature, "--°");
        np_set_text(s_ui.home.weather_place, "Brasília, DF");
        np_set_text(s_ui.home.weather_summary, "Sem dados de clima");
        np_set_text(s_ui.home.weather_wind.value, "--");
        np_set_text(s_ui.home.weather_humidity.value, "--");
        np_set_text(s_ui.home.weather_feels_like.value, "--");
        np_set_text(s_ui.home.weather_uv.value, "--");
        return;
    }

    char display_temperature[20] = {0};
    char humidity[12] = {0};

    /* A Home prioriza leitura a distância, como no protótipo: temperatura
     * inteira e grande. O dado original continua preservado no modelo. */
    const int16_t rounded_c = weather->temperature_deci_c >= 0
                                  ? (int16_t)((weather->temperature_deci_c + 5) / 10)
                                  : (int16_t)((weather->temperature_deci_c - 5) / 10);
    (void)snprintf(display_temperature, sizeof(display_temperature),
                   "%d°", (int)rounded_c);

    (void)snprintf(humidity, sizeof(humidity), "%u%%",
                   (unsigned int)weather->relative_humidity_percent);

    np_set_text(s_ui.home.weather_temperature, display_temperature);
    np_set_text(s_ui.home.weather_place, "Brasília, DF");
    np_set_text(s_ui.home.weather_summary,
                weather_condition_summary(
                    weather_condition_from_code(weather->weather_code)));
    np_set_text(s_ui.home.weather_humidity.value, humidity);

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    char feels_like[20] = {0};
    char wind[20] = {0};
    char uv[16] = {0};

    if (weather->wind_speed_available) {
        const unsigned int whole =
            (unsigned int)(weather->wind_speed_deci_kmh / 10U);
        const unsigned int decimal =
            (unsigned int)(weather->wind_speed_deci_kmh % 10U);
        (void)snprintf(wind, sizeof(wind), "%u,%u km/h", whole, decimal);
        np_set_text(s_ui.home.weather_wind.value, wind);
    } else {
        np_set_text(s_ui.home.weather_wind.value, "--");
    }

    if (weather->apparent_temperature_available) {
        format_temperature_deci(weather->apparent_temperature_deci_c,
                                feels_like, sizeof(feels_like));
        np_set_text(s_ui.home.weather_feels_like.value, feels_like);
    } else {
        np_set_text(s_ui.home.weather_feels_like.value, "--");
    }

    if (weather->uv_index_available) {
        const unsigned int whole =
            (unsigned int)(weather->uv_index_deci / 10U);
        const unsigned int decimal =
            (unsigned int)(weather->uv_index_deci % 10U);
        (void)snprintf(uv, sizeof(uv), "%u,%u", whole, decimal);
        np_set_text(s_ui.home.weather_uv.value, uv);
    } else {
        np_set_text(s_ui.home.weather_uv.value, "--");
    }
#else
    np_set_text(s_ui.home.weather_wind.value, "--");
    np_set_text(s_ui.home.weather_feels_like.value, "--");
    np_set_text(s_ui.home.weather_uv.value, "--");
#endif
}

/* ------------------------------------------------------------------ */
/* Bitcoin                                                             */
/* ------------------------------------------------------------------ */

static void clear_btc_details(void)
{
    np_set_text(s_ui.home.btc_high_24h, "--");
    np_set_text(s_ui.home.btc_low_24h, "--");
    np_set_text(s_ui.home.btc_volume_24h, "--");
    np_home_set_btc_spark(&s_ui.home, NULL, 0U, np_c_text_3());
}

static void update_market(const app_ui_projection_t *projection)
{
    const offline_market_data_t *market = &projection->offline_data.market;

    np_status_dot_set(&s_ui.home.btc_status,
                      data_state(market->available, market->stale));

    if (!market->available) {
        np_set_text(s_ui.home.btc_price_prefix, "US$");
        np_set_text(s_ui.home.btc_price, "--");
        np_set_text(s_ui.home.btc_change_icon, NP_ICON_RISE);
        np_set_text(s_ui.home.btc_change, "--");
        np_set_text_color(s_ui.home.btc_change_icon, np_c_text_3());
        np_set_text_color(s_ui.home.btc_change, np_c_text_3());
        clear_btc_details();
        return;
    }

    char price[24] = {0};
    char change[20] = {0};

    format_btc_whole(market->bitcoin_usd_cents, price, sizeof(price));
    format_basis_points(market->change_24h_basis_points,
                        change, sizeof(change));

    const lv_color_t change_color =
        market->change_24h_basis_points >= 0
            ? np_c_positive()
            : np_c_negative();

    np_set_text(s_ui.home.btc_price_prefix, "US$");
    np_set_text(s_ui.home.btc_price, price);
    np_set_text(s_ui.home.btc_change_icon,
                market->change_24h_basis_points >= 0
                    ? NP_ICON_RISE
                    : NP_ICON_FALL);
    np_set_text(s_ui.home.btc_change, change);
    np_set_text_color(s_ui.home.btc_change_icon, change_color);
    np_set_text_color(s_ui.home.btc_change, change_color);

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    char high[24] = {0};
    char low[24] = {0};
    char volume[24] = {0};

    if (market->high_24h_available) {
        format_usd_metric(market->high_24h_usd_cents,
                          high, sizeof(high));
        np_set_text(s_ui.home.btc_high_24h, high);
    } else {
        np_set_text(s_ui.home.btc_high_24h, "--");
    }

    if (market->low_24h_available) {
        format_usd_metric(market->low_24h_usd_cents,
                          low, sizeof(low));
        np_set_text(s_ui.home.btc_low_24h, low);
    } else {
        np_set_text(s_ui.home.btc_low_24h, "--");
    }

    if (market->volume_24h_available) {
        format_volume_short(market->volume_24h_usd_cents,
                            volume, sizeof(volume));
        np_set_text(s_ui.home.btc_volume_24h, volume);
    } else {
        np_set_text(s_ui.home.btc_volume_24h, "--");
    }

    if (market->history_count >= 2U) {
        int32_t samples[24] = {0};
        uint8_t count = market->history_count;
        if (count > 24U) count = 24U;

        for (uint8_t i = 0U; i < count; ++i) {
            samples[i] = (int32_t)(market->history_usd_cents[i] / 100U);
        }

        np_home_set_btc_spark(&s_ui.home,
                              samples,
                              count,
                              change_color);
    } else {
        np_home_set_btc_spark(&s_ui.home,
                              NULL, 0U, change_color);
    }
#else
    clear_btc_details();
#endif
}

/* ------------------------------------------------------------------ */
/* USD/BRL                                                             */
/* ------------------------------------------------------------------ */

static void update_exchange(const app_ui_projection_t *projection)
{
    const offline_exchange_data_t *const exchange =
        &projection->offline_data.exchange;

    np_status_dot_set(&s_ui.home.usd.status,
                      data_state(exchange->available, exchange->stale));

    if (!exchange->available) {
        np_set_text(s_ui.home.usd.value, "--");
        np_set_text(s_ui.home.usd.change_icon, "");
        np_set_text(s_ui.home.usd.change, "--");
        np_set_text_color(s_ui.home.usd.change, np_c_text_3());
        np_set_visible(s_ui.home.usd.change_icon, true);
        np_set_visible(s_ui.home.usd.change, true);
        return;
    }

    /* O contrato preserva 4 casas; a Home mostra 2 para leitura a distância. */
    const uint32_t cents =
        (exchange->usd_brl_ten_thousandths + 50U) / 100U;

    char value[24] = {0};
    (void)snprintf(value, sizeof(value), "R$ %lu,%02lu",
                   (unsigned long)(cents / 100U),
                   (unsigned long)(cents % 100U));

    np_set_text(s_ui.home.usd.value, value);
    np_set_text_color(s_ui.home.usd.value, np_c_text());

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    if (exchange->change_available) {
        char change[20] = {0};
        format_basis_points(exchange->change_basis_points,
                            change, sizeof(change));

        const bool positive = exchange->change_basis_points >= 0;
        const lv_color_t color = positive ? np_c_positive() : np_c_negative();

        np_set_text(s_ui.home.usd.change_icon,
                    positive ? NP_ICON_RISE : NP_ICON_FALL);
        np_set_text(s_ui.home.usd.change, change);
        np_set_text_color(s_ui.home.usd.change_icon, color);
        np_set_text_color(s_ui.home.usd.change, color);
    } else {
        np_set_text(s_ui.home.usd.change_icon, "");
        np_set_text(s_ui.home.usd.change, "--");
        np_set_text_color(s_ui.home.usd.change, np_c_text_3());
    }
#else
    /* O schema v3 ainda não traz variação do USD/BRL. Mantemos o espaço\n     * visual correto sem inventar um percentual. */
    np_set_text(s_ui.home.usd.change_icon, "");
    np_set_text(s_ui.home.usd.change, "--");
    np_set_text_color(s_ui.home.usd.change, np_c_text_3());
#endif

    np_set_visible(s_ui.home.usd.change_icon, true);
    np_set_visible(s_ui.home.usd.change, true);
}

/* ------------------------------------------------------------------ */
/* Ibovespa                                                            */
/* ------------------------------------------------------------------ */

static void update_ibovespa(const app_ui_projection_t *projection)
{
#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    const offline_ibovespa_data_t *const ibov =
        &projection->offline_data.ibovespa;

    np_status_dot_set(&s_ui.home.ibov.status,
                      data_state(ibov->available, ibov->stale));

    if (!ibov->available) {
        np_set_text(s_ui.home.ibov.value, "--");
        np_set_text(s_ui.home.ibov.change_icon, "");
        np_set_text(s_ui.home.ibov.change, "--");
        np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
        return;
    }

    char value[24] = {0};
    format_grouped_u32(ibov->value_centi_points / 100U,
                       value, sizeof(value));
    np_set_text(s_ui.home.ibov.value, value);

    if (ibov->change_available) {
        char change[20] = {0};
        format_basis_points(ibov->change_basis_points,
                            change, sizeof(change));

        const bool positive = ibov->change_basis_points >= 0;
        const lv_color_t color = positive ? np_c_positive() : np_c_negative();

        np_set_text(s_ui.home.ibov.change_icon,
                    positive ? NP_ICON_RISE : NP_ICON_FALL);
        np_set_text(s_ui.home.ibov.change, change);
        np_set_text_color(s_ui.home.ibov.change_icon, color);
        np_set_text_color(s_ui.home.ibov.change, color);
    } else {
        np_set_text(s_ui.home.ibov.change_icon, "");
        np_set_text(s_ui.home.ibov.change, "--");
        np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
    }
#else
    (void)projection;
    np_status_dot_set(&s_ui.home.ibov.status, NP_DATA_UNAVAILABLE);
    np_set_text(s_ui.home.ibov.value, "--");
    np_set_text(s_ui.home.ibov.change_icon, "");
    np_set_text(s_ui.home.ibov.change, "--");
    np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
#endif

    np_set_visible(s_ui.home.ibov.change_icon, true);
    np_set_visible(s_ui.home.ibov.change, true);
}

/* ------------------------------------------------------------------ */
/* Boot/Home orchestration                                             */
/* ------------------------------------------------------------------ */

static uint8_t boot_progress(const app_ui_projection_t *projection)
{
    uint8_t stage = 1U;

    if (projection->storage.ready) stage = 2U;
    if (projection->network.state >= APP_NETWORK_STATE_LINK_UP) stage = 3U;
    if (projection->time_trusted) stage = 4U;
    if (projection->offline_data.weather.available ||
        projection->offline_data.market.available) {
        stage = 5U;
    }

    return stage;
}

static void update_boot(const app_ui_projection_t *projection)
{
    const uint8_t stage = boot_progress(projection);

    static const char *const status[] = {
        "Preparando o display",
        "Verificando o armazenamento",
        "Procurando redes Wi-Fi",
        "Atualizando data e hora",
        "Carregando os dados do painel",
        "Tudo pronto",
    };

    np_segbar_set(&s_ui.boot.progress, stage, np_c_accent());
    np_set_text(s_ui.boot.status, status[stage]);

    if (stage < 3U) {
        np_set_text(s_ui.boot.detail,
                    "A inicialização continua mesmo sem internet.");
    } else if (!projection->network.online) {
        np_set_text(s_ui.boot.detail,
                    "O painel abrirá com os últimos dados salvos.");
    } else {
        np_set_text(s_ui.boot.detail,
                    "Conexão disponível; concluindo a inicialização.");
    }
}

static void update_home(const app_ui_projection_t *projection)
{
    update_clock(projection);
    update_weather(projection);
    update_market(projection);
    update_exchange(projection);
    update_ibovespa(projection);

    np_header_set_connections(
        &s_ui.home.header,
        projection->network.online,
        false,
        projection->network.state == APP_NETWORK_STATE_FAILED ||
            projection->storage.last_result != ESP_OK);
}

static void show_home(void)
{
    if (s_ui.home_visible) return;

    lv_obj_add_flag(s_ui.boot.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_ui.home.root, LV_OBJ_FLAG_HIDDEN);
    s_ui.home_visible = true;

    ESP_LOGI(TAG, "Boot transition complete; Home V2 visible");
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);

    if (projection.revision != s_ui.rendered_revision) {
        update_boot(&projection);
        update_home(&projection);
        s_ui.rendered_revision = projection.revision;
    }

    const uint32_t elapsed = lv_tick_elaps(s_ui.started_at_tick);

    if (elapsed >= BOOT_MINIMUM_MS &&
        projection.ready &&
        (projection.time_trusted || elapsed >= BOOT_MAXIMUM_MS)) {
        show_home();
    } else if (elapsed >= BOOT_MAXIMUM_MS) {
        show_home();
    }
}

static void diagnostics_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_ui.refresh_timer != NULL) {
        lv_timer_delete(s_ui.refresh_timer);
    }

    lv_display_t *display = s_ui.display;
    lv_indev_t *touch_indev = s_ui.touch_indev;

    s_ui = (product_ui_state_t){0};

    lv_obj_clean(lv_screen_active());
    (void)diagnostic_ui_create(display, touch_indev);
}

esp_err_t product_ui_create(lv_display_t *display, lv_indev_t *touch_indev)
{
    ESP_RETURN_ON_FALSE(display != NULL && touch_indev != NULL,
                        ESP_ERR_INVALID_ARG,
                        "product_ui",
                        "Display or touch handle missing");

    s_ui = (product_ui_state_t){
        .display = display,
        .touch_indev = touch_indev,
        .started_at_tick = lv_tick_get(),
        .rendered_revision = UINT32_MAX,
    };

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);

    s_ui.boot = np_boot_build(screen);
    s_ui.home = np_home_build(screen);
    np_set_visible(s_ui.home.root, false);

    lv_obj_add_event_cb(s_ui.home.header.settings_button,
                        diagnostics_button_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    s_ui.refresh_timer =
        lv_timer_create(refresh_timer_cb, UI_REFRESH_PERIOD_MS, NULL);

    ESP_RETURN_ON_FALSE(s_ui.refresh_timer != NULL,
                        ESP_ERR_NO_MEM,
                        "product_ui",
                        "UI refresh timer allocation failed");

    refresh_timer_cb(s_ui.refresh_timer);
    return ESP_OK;
}