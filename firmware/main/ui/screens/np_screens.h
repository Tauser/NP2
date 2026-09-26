/*
 * NovaPanel v5 -- modelos de tela (camada 4).
 *
 * Home V2: Dark Graphite, 1024x600.
 */

#ifndef NP_SCREENS_H
#define NP_SCREENS_H

#include "np_components.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    np_segbar_t progress;
    lv_obj_t *status;
    lv_obj_t *detail;
} np_boot_view_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;

    /* Clima */
    lv_obj_t *weather_card;
    lv_obj_t *weather_icon;
    lv_obj_t *weather_icon_fallback;
    np_status_dot_t weather_status;
    lv_obj_t *weather_place;
    lv_obj_t *weather_temperature;
    lv_obj_t *weather_summary;
    lv_obj_t *weather_date;
    np_metric_tile_t weather_wind;
    np_metric_tile_t weather_humidity;
    np_metric_tile_t weather_feels_like;
    np_metric_tile_t weather_uv;

    /* Bitcoin */
    lv_obj_t *btc_card;
    np_status_dot_t btc_status;
    lv_obj_t *btc_price_prefix;
    lv_obj_t *btc_price;
    lv_obj_t *btc_change_icon;
    lv_obj_t *btc_change;
    np_spark_t btc_spark;
    lv_obj_t *btc_high_24h;
    lv_obj_t *btc_low_24h;
    lv_obj_t *btc_volume_24h;

    /* Mercado secundario */
    np_market_strip_t usd;
    np_market_strip_t ibov;
} np_home_view_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *home_button;
    lv_obj_t *left_card;
    lv_obj_t *middle_card;
    lv_obj_t *right_card;
} np_settings_view_t;

np_boot_view_t np_boot_build(lv_obj_t *parent);
np_home_view_t np_home_build(lv_obj_t *parent);
np_settings_view_t np_settings_build(lv_obj_t *parent);
void np_settings_reset_stages(np_settings_view_t *view);

void np_home_set_weather_icon_source(np_home_view_t *view, const void *source);
void np_home_set_btc_spark(np_home_view_t *view, const int32_t *samples,
                           uint8_t count, lv_color_t color);

/* Wrappers para o catalogo/simulador. */
lv_obj_t *np_boot_create(lv_obj_t *parent);
lv_obj_t *np_home_create(lv_obj_t *parent);
lv_obj_t *np_market_create(lv_obj_t *parent);
lv_obj_t *np_setup_create(lv_obj_t *parent);

/* Aspiracional */
lv_obj_t *np_weather_create(lv_obj_t *parent);
lv_obj_t *np_timer_create(lv_obj_t *parent);
lv_obj_t *np_agenda_create(lv_obj_t *parent);
lv_obj_t *np_alarms_create(lv_obj_t *parent);
lv_obj_t *np_notifications_create(lv_obj_t *parent);
lv_obj_t *np_devices_create(lv_obj_t *parent);
lv_obj_t *np_settings_create(lv_obj_t *parent);
lv_obj_t *np_sheets_create(lv_obj_t *parent);

typedef struct {
    const char *id;
    const char *title;
    const char *archetype;
    lv_obj_t *(*create)(lv_obj_t *parent);
} np_screen_entry_t;

const np_screen_entry_t *np_screen_catalog(void);
int np_screen_catalog_count(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_SCREENS_H */
