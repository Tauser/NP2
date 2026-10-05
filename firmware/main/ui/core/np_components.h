/*
 * NovaPanel v5 -- componentes (camada 3).
 *
 * Home V2 adiciona componentes chapados sem remover os contratos legados.
 */

#ifndef NP_COMPONENTS_H
#define NP_COMPONENTS_H

#include "lvgl.h"
#include "np_form.h"
#include "np_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- escrita guardada ---------------- */

void np_set_text(lv_obj_t *label, const char *text);
void np_set_text_color(lv_obj_t *obj, lv_color_t color);
void np_set_bg_color(lv_obj_t *obj, lv_color_t color);
void np_set_bg_opa(lv_obj_t *obj, lv_opa_t opa);
void np_set_radius(lv_obj_t *obj, int32_t radius);
void np_set_visible(lv_obj_t *obj, bool visible);

/* ---------------- primitivas ---------------- */

lv_obj_t *np_scene(lv_obj_t *parent);
lv_obj_t *np_group(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);

lv_obj_t *np_surface(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
lv_obj_t *np_raised(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
lv_obj_t *np_glass(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);

/* Home V2 */
lv_obj_t *np_panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);

lv_obj_t *np_fill(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  lv_color_t color, lv_opa_t opa, int32_t radius);

lv_obj_t *np_image(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
void np_image_set_source(lv_obj_t *image, const void *source);
void np_set_clip_corner(lv_obj_t *obj, bool enabled);

lv_obj_t *np_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                   lv_color_t color, int32_t x, int32_t y, int32_t w,
                   lv_text_align_t align);

lv_obj_t *np_hline(lv_obj_t *parent, int32_t x, int32_t y, int32_t w);
lv_obj_t *np_vline(lv_obj_t *parent, int32_t x, int32_t y, int32_t h);
lv_obj_t *np_dot(lv_obj_t *parent, int32_t x, int32_t y, int32_t size, lv_color_t color);

lv_obj_t *np_icon_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t size, const char *symbol);

/* ---------------- compostos legados ---------------- */

typedef struct {
    lv_obj_t *caption;
    lv_obj_t *value;
} np_metric_t;

np_metric_t np_metric(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                      const char *caption, const char *value, const lv_font_t *value_font);

typedef struct {
    lv_obj_t *dot;
    lv_obj_t *label;
} np_pill_t;

np_pill_t np_status_pill(lv_obj_t *parent, int32_t x, int32_t y, np_data_state_t state);
void np_status_pill_set(np_pill_t *pill, np_data_state_t state);

/* ---------------- Home V2 ---------------- */

typedef struct {
    lv_obj_t *dot;
} np_status_dot_t;

np_status_dot_t np_status_dot(lv_obj_t *parent, int32_t x, int32_t y,
                              np_data_state_t state);
void np_status_dot_set(np_status_dot_t *status, np_data_state_t state);

typedef enum {
    NP_METRIC_ICON_WIND = 0,
    NP_METRIC_ICON_HUMIDITY,
    NP_METRIC_ICON_TEMPERATURE,
    NP_METRIC_ICON_UV,
} np_metric_icon_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *icon;
    lv_obj_t *caption;
    lv_obj_t *value;
} np_metric_tile_t;

np_metric_tile_t np_metric_tile(lv_obj_t *parent, int32_t x, int32_t y,
                                int32_t w, int32_t h,
                                np_metric_icon_t icon_kind, lv_color_t icon_color,
                                const char *caption, const char *value);

typedef enum {
    NP_MARKET_ICON_DOLLAR = 0,
    NP_MARKET_ICON_IBOV,
} np_market_icon_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *lead;
    lv_obj_t *title;
    lv_obj_t *ticker;
    lv_obj_t *value;
    lv_obj_t *change_icon;
    lv_obj_t *change;
    np_status_dot_t status;
} np_market_strip_t;

np_market_strip_t np_market_strip(lv_obj_t *parent, int32_t x, int32_t y,
                                  int32_t w, int32_t h,
                                  np_market_icon_t icon_kind,
                                  const char *title, const char *ticker);

/* Ícones vetoriais leves usados pela Home; não dependem do subset da fonte. */
lv_obj_t *np_location_icon(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t size, lv_color_t color);
lv_obj_t *np_bitcoin_badge(lv_obj_t *parent, int32_t x, int32_t y, int32_t size);

/* ---------------- relogio ---------------- */

typedef struct {
    lv_obj_t *slot[5];
} np_clock_t;

np_clock_t np_clock(lv_obj_t *parent, int32_t x, int32_t y, const lv_font_t *font,
                    lv_color_t color);
void np_clock_set(np_clock_t *clock, const char *hhmm);

/* ---------------- botoes/header ---------------- */

lv_obj_t *np_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                    const char *text, bool primary);

const char *np_wifi_signal_icon(int8_t rssi);

typedef struct {
    lv_obj_t *menu_button;
    lv_obj_t *drawer_home_button;
    lv_obj_t *drawer_weather_button;
    lv_obj_t *drawer_market_button;
    lv_obj_t *drawer_iot_button;
    lv_obj_t *drawer_pomodoro_button;
    lv_obj_t *drawer_settings_button;
    lv_obj_t *settings_button;
    lv_obj_t *wifi_button;
    lv_obj_t *bluetooth_button;
    lv_obj_t *notifications_button;
    lv_obj_t *notification_badge;
    lv_obj_t *alert_dot;

    lv_obj_t *brand_nova;
    lv_obj_t *brand_panel;
    lv_obj_t *user_greeting;
    lv_obj_t *divider;
    np_clock_t clock;
    lv_obj_t *date;

    lv_obj_t *drawer_scrim;
    lv_obj_t *drawer;
} np_header_t;

np_header_t np_header(lv_obj_t *parent);
void np_header_set_drawer_active(np_header_t *header, bool settings_active);
void np_header_set_drawer_weather_active(np_header_t *header, bool weather_active);
void np_header_set_drawer_market_active(np_header_t *header, bool market_active);
void np_header_set_drawer_iot_active(np_header_t *header, bool iot_active);
void np_header_set_drawer_pomodoro_active(np_header_t *header, bool active);
void np_header_set_connections(np_header_t *header, bool wifi_online,
                               int8_t wifi_rssi, bool wifi_rssi_measured,
                               bool bluetooth_online, bool has_alert);
void np_header_set_notifications_enabled(np_header_t *header, bool enabled);
void np_header_set_notification_count(np_header_t *header, uint8_t unread_count);

/* ---------------- componentes existentes ---------------- */

typedef struct {
    lv_obj_t *root;
    lv_obj_t *lead;
    lv_obj_t *title;
    lv_obj_t *meta;
    lv_obj_t *accent;
} np_row_t;

np_row_t np_row(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
void np_row_set(np_row_t *row, const char *lead, const char *title, const char *meta,
                lv_color_t accent_color, bool visible);

typedef struct {
    lv_obj_t *root;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *state;
} np_tile_t;

np_tile_t np_tile(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  const char *icon, const char *title, const char *state);
void np_tile_set_on(np_tile_t *tile, bool on, const char *state_text);

#define NP_SPARK_MAX_POINTS 24U

typedef struct {
    /*
     * O spark do BTC usa LV_CHART_TYPE_LINE. O preenchimento degradê segue
     * cada segmento da curva até a base do gráfico.
     */
    lv_obj_t *chart;
    lv_chart_series_t *series;
    lv_obj_t *last_dot;
} np_spark_t;

np_spark_t np_spark(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h);
void np_spark_set(np_spark_t *spark, const int32_t *samples, uint8_t count,
                  int32_t w, int32_t h, lv_color_t color);

typedef struct {
    lv_obj_t *seg[6];
    uint8_t count;
} np_segbar_t;

np_segbar_t np_segbar(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, uint8_t count);
void np_segbar_set(np_segbar_t *bar, uint8_t done, lv_color_t color);

lv_obj_t *np_dots(lv_obj_t *parent, int32_t y, uint8_t count, uint8_t active);

#ifdef __cplusplus
}
#endif

#endif /* NP_COMPONENTS_H */
