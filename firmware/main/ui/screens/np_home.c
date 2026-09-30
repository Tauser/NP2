/*
 * NovaPanel Home V2 -- Dark Graphite, 1024x600.
 *
 * Layout:
 *   Header       0,0      1024x64
 *   Clima       24,76      560x376
 *   Bitcoin    600,76      400x376
 *   USD/BRL     24,468     560x96
 *   Ibovespa   600,468     400x96
 *
 * O card de clima usa a superficie neutra e um icone animado.
 */

#include "np_components.h"
#include "np_screens.h"
#include "np_styles.h"

#include "np_tokens.h"
#include "../assets/np_weather_icon_assets.h"

/* ------------------------------------------------------------------ */
/* Geometria da Home V2                                                */
/* ------------------------------------------------------------------ */

#define NP_HOME_LEFT_X       24
#define NP_HOME_RIGHT_X      600
#define NP_HOME_GAP          16

#define NP_HOME_LEFT_W       560
#define NP_HOME_RIGHT_W      400
#define NP_HOME_MAIN_H       376
#define NP_HOME_BOTTOM_H     96
#define NP_HOME_TOP_Y        (NP_HEADER_H + 12)
#define NP_HOME_BOTTOM_Y     (NP_HOME_TOP_Y + NP_HOME_MAIN_H + NP_HOME_GAP)

#define NP_HOME_WEATHER_ICON_DISPLAY_SIZE NP_WEATHER_ICON_SIZE
#define NP_HOME_WEATHER_VALUE_X 200
#define NP_HOME_WEATHER_TEXT_W (NP_HOME_LEFT_W - NP_HOME_WEATHER_VALUE_X - 24)

#define NP_HOME_BTC_SPARK_W  390
#define NP_HOME_BTC_SPARK_H  202
#define NP_HOME_WEATHER_FRAME_MS 250U

/* ------------------------------------------------------------------ */
/* Clima                                                               */
/* ------------------------------------------------------------------ */

void np_home_set_weather_icon_source(np_home_view_t *view, const void *source)
{
    if (view == NULL || view->weather_icon == NULL) return;

    const np_weather_icon_asset_t *const asset = source;
    if (asset == NULL || asset->frames == NULL || asset->frame_count == 0U ||
        asset->frame_count > NP_WEATHER_ICON_MAX_FRAMES || asset->frame_ms != 125U) {
        (void)lv_animimg_delete(view->weather_icon);
        lv_obj_add_flag(view->weather_icon, LV_OBJ_FLAG_HIDDEN);
        np_set_visible(view->weather_icon_fallback, true);
        return;
    }
    if (lv_animimg_get_src(view->weather_icon) == asset->frames &&
        !lv_obj_has_flag(view->weather_icon, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    (void)lv_animimg_delete(view->weather_icon);
    lv_animimg_set_src(view->weather_icon, asset->frames, asset->frame_count);
    /* Os assets continuam declarando 125 ms por frame, mas a Home limita
     * a animação a 4 FPS para reduzir carga do renderer SW e evitar starvation
     * da IDLE0/watchdog. */
    lv_animimg_set_duration(view->weather_icon,
                            (uint32_t)asset->frame_count * NP_HOME_WEATHER_FRAME_MS);
    lv_animimg_set_repeat_count(view->weather_icon, LV_ANIM_REPEAT_INFINITE);
    lv_image_set_src(view->weather_icon, asset->frames[0]);
    np_set_visible(view->weather_icon_fallback, false);
    lv_obj_clear_flag(view->weather_icon, LV_OBJ_FLAG_HIDDEN);
    lv_animimg_start(view->weather_icon);
}

void np_home_set_btc_spark(np_home_view_t *view, const int32_t *samples,
                           uint8_t count, lv_color_t color)
{
    if (view == NULL) return;
    np_spark_set(&view->btc_spark, samples, count,
                 NP_HOME_BTC_SPARK_W, NP_HOME_BTC_SPARK_H, color);
}

/* ------------------------------------------------------------------ */
/* Build                                                               */
/* ------------------------------------------------------------------ */

np_home_view_t np_home_build_with_header(lv_obj_t *parent, const np_header_t *header)
{
    np_home_view_t view = {0};

    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    /* ---------------- Clima ---------------- */

    view.weather_card = np_panel(view.root,
                                 NP_HOME_LEFT_X,
                                 NP_HOME_TOP_Y,
                                 NP_HOME_LEFT_W,
                                 NP_HOME_MAIN_H);

    view.weather_icon = lv_animimg_create(view.weather_card);
    lv_obj_remove_style_all(view.weather_icon);
    lv_obj_set_pos(view.weather_icon, 24, 62);
    lv_obj_set_size(view.weather_icon, NP_HOME_WEATHER_ICON_DISPLAY_SIZE,
                    NP_HOME_WEATHER_ICON_DISPLAY_SIZE);
    /* Assets nativos já chegam no tamanho de exibição. Evite STRETCH: ele
     * força transform/recolor por software a cada frame no ESP32-P4. */
    lv_image_set_inner_align(view.weather_icon, LV_IMAGE_ALIGN_CENTER);
    lv_obj_clear_flag(view.weather_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(view.weather_icon, LV_OBJ_FLAG_HIDDEN);

    view.weather_icon_fallback = np_label(view.weather_card,
                                          NP_ICON_WEATHER,
                                          NP_FONT_ICON_BADGE,
                                          np_c_text_2(),
                                          80, 111, 48,
                                          LV_TEXT_ALIGN_CENTER);

    (void)np_location_icon(view.weather_card, 24, 20, 24, np_c_text_2());
    view.weather_place = np_label(view.weather_card,
                                  "Brasília, DF",
                                  NP_FONT_MD,
                                  np_c_text(),
                                  56, 20, 300,
                                  LV_TEXT_ALIGN_LEFT);

    view.weather_status = np_status_dot(view.weather_card,
                                        NP_HOME_LEFT_W - 24 - NP_STATUS_DOT_SIZE,
                                        26,
                                        NP_DATA_UNAVAILABLE);

    view.weather_temperature = np_label(view.weather_card,
                                        "--°",
                                        NP_FONT_HERO,
                                        np_c_text(),
                                        NP_HOME_WEATHER_VALUE_X, 58,
                                        NP_HOME_WEATHER_TEXT_W,
                                        LV_TEXT_ALIGN_LEFT);

    view.weather_summary = np_label(view.weather_card,
                                    "Sem dados de clima",
                                    NP_FONT_LG,
                                    np_c_text(),
                                    NP_HOME_WEATHER_VALUE_X, 174,
                                    NP_HOME_WEATHER_TEXT_W,
                                    LV_TEXT_ALIGN_LEFT);

    view.weather_date = np_label(view.weather_card,
                                 "Data indisponível",
                                 NP_FONT_SM,
                                 np_c_text_2(),
                                 NP_HOME_WEATHER_VALUE_X, 208,
                                 NP_HOME_WEATHER_TEXT_W,
                                 LV_TEXT_ALIGN_LEFT);

    /* 4 tiles: 24 + 4*122 + 3*8 + 24 = 560 */
    view.weather_wind =
        np_metric_tile(view.weather_card, 24, 250, 122, 102,
                       NP_METRIC_ICON_WIND, np_c_weather_wind(),
                       "Vento km/h", "--");

    view.weather_humidity =
        np_metric_tile(view.weather_card, 154, 250, 122, 102,
                       NP_METRIC_ICON_HUMIDITY, np_c_weather_water(),
                       "Umidade", "--");

    view.weather_feels_like =
        np_metric_tile(view.weather_card, 284, 250, 122, 102,
                       NP_METRIC_ICON_TEMPERATURE, np_c_weather_temp(),
                       "Sensação", "--");

    view.weather_uv =
        np_metric_tile(view.weather_card, 414, 250, 122, 102,
                       NP_METRIC_ICON_UV, np_c_weather_uv(),
                       "Índice UV", "--");

    /* ---------------- Bitcoin ---------------- */

    view.btc_card = np_panel(view.root,
                             NP_HOME_RIGHT_X,
                             NP_HOME_TOP_Y,
                             NP_HOME_RIGHT_W,
                             NP_HOME_MAIN_H);

    (void)np_bitcoin_badge(view.btc_card, 24, 20, 52);

    np_label(view.btc_card,
             "Bitcoin",
             NP_FONT_LG,
             np_c_text(),
             90, 20, 190,
             LV_TEXT_ALIGN_LEFT);

    np_label(view.btc_card,
             "BTC/USD",
             NP_FONT_SM,
             np_c_text_2(),
             90, 52, 130,
             LV_TEXT_ALIGN_LEFT);

    view.btc_status = np_status_dot(view.btc_card,
                                    NP_HOME_RIGHT_W - 24 - NP_STATUS_DOT_SIZE,
                                    28,
                                    NP_DATA_UNAVAILABLE);

    /*
     * O chart é criado antes das labels de preço/variação. Assim o gráfico e
     * o fade formam o fundo do card e as informações permanecem sempre por
     * cima. A linha usa pontos discretos e o fade linear segue a técnica do
     * exemplo oficial do LVGL até a divisória.
     */
    view.btc_spark = np_spark(view.btc_card,
                              5, 80,
                              NP_HOME_BTC_SPARK_W,
                              NP_HOME_BTC_SPARK_H);

    view.btc_price_prefix = np_label(view.btc_card,
                                     "US$",
                                     NP_FONT_SM,
                                     np_c_text_2(),
                                     24, 184, 46,
                                     LV_TEXT_ALIGN_LEFT);

    view.btc_price = np_label(view.btc_card,
                              "--",
                              NP_FONT_BRAND,
                              np_c_text(),
                              74, 154, 302,
                              LV_TEXT_ALIGN_LEFT);

    /* Variação volta a funcionar como informação semântica principal:
     * verde na alta, vermelho na queda. Mantemos NP_FONT_ICON porque esse é
     * o subset que contém os glyphs reais de subida/queda nesta build. */
    view.btc_change_icon = np_label(view.btc_card,
                                    NP_ICON_RISE,
                                    NP_FONT_ICON,
                                    np_c_text_3(),
                                    24, 238, 30,
                                    LV_TEXT_ALIGN_CENTER);

    view.btc_change = np_label(view.btc_card,
                               "--",
                               NP_FONT_LG,
                               np_c_text_3(),
                               58, 232, 170,
                               LV_TEXT_ALIGN_LEFT);

    np_hline(view.btc_card, 24, 282, NP_HOME_RIGHT_W - 48);

    np_metric_t high = np_metric(view.btc_card,
                                 24, 298, 104,
                                 "Máx. 24 h", "--", NP_FONT_SM);
    view.btc_high_24h = high.value;

    np_metric_t low = np_metric(view.btc_card,
                                148, 298, 104,
                                "Mín. 24 h", "--", NP_FONT_SM);
    view.btc_low_24h = low.value;

    np_metric_t volume = np_metric(view.btc_card,
                                   272, 298, 104,
                                   "Volume", "--", NP_FONT_SM);
    view.btc_volume_24h = volume.value;

    /* ---------------- Mercado secundário ---------------- */

    view.usd = np_market_strip(view.root,
                               NP_HOME_LEFT_X,
                               NP_HOME_BOTTOM_Y,
                               NP_HOME_LEFT_W,
                               NP_HOME_BOTTOM_H,
                               NP_MARKET_ICON_DOLLAR,
                               "Dólar PTAX",
                               "USD/BRL");

    view.ibov = np_market_strip(view.root,
                                NP_HOME_RIGHT_X,
                                NP_HOME_BOTTOM_Y,
                                NP_HOME_RIGHT_W,
                                NP_HOME_BOTTOM_H,
                                NP_MARKET_ICON_IBOV,
                                "Ibovespa",
                                "IBOV");

    return view;
}

np_home_view_t np_home_build(lv_obj_t *parent)
{
    return np_home_build_with_header(parent, NULL);
}

lv_obj_t *np_home_create(lv_obj_t *parent)
{
    return np_home_build(parent).root;
}
