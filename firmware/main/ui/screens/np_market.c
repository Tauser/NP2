#include "np_market.h"

#include <math.h>
#include <stdio.h>

#include "src/draw/lv_draw_arc.h"
#include "np_styles.h"

#define MARKET_LEFT_X 18
#define MARKET_LEFT_Y 82
#define MARKET_LEFT_W 608
#define MARKET_LEFT_H 374
#define MARKET_RIGHT_X 636
#define MARKET_RIGHT_W 370
#define MARKET_BOTTOM_Y 468
#define MARKET_BOTTOM_H 120
#define MARKET_GAP 8
#define MARKET_FEAR_W 300
#define MARKET_FEAR_CENTER_X 93
#define MARKET_FEAR_CENTER_Y 108
#define MARKET_FEAR_RADIUS 67
#define MARKET_FEAR_ARC_WIDTH 14
#define MARKET_FEAR_ARC_STEPS 24
#define MARKET_FEAR_MARKER_SIZE 12
#define MARKET_BTC_SPARK_X 5
#define MARKET_BTC_SPARK_Y 80
#define MARKET_BTC_SPARK_W (MARKET_LEFT_W - 10)
#define MARKET_BTC_SPARK_H 202
#define MARKET_BOTTOM_STRIP_W \
    ((NP_SCREEN_W - 36 - MARKET_FEAR_W - 3 * MARKET_GAP) / 3)

static lv_obj_t *market_card(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t width, int32_t height)
{
    lv_obj_t *card = np_surface(parent, x, y, width, height);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    return card;
}

static void set_metric_state(lv_obj_t *icon, lv_obj_t *change, bool available,
                             bool positive)
{
    const lv_color_t color = available
        ? (positive ? np_c_positive() : np_c_negative()) : np_c_text_3();
    np_set_text(icon, available ? (positive ? NP_ICON_RISE : NP_ICON_FALL) : "");
    np_set_text_color(icon, color);
    np_set_text_color(change, color);
}

static np_data_state_t market_data_state(bool available, bool stale)
{
    if (!available) return NP_DATA_UNAVAILABLE;
    return stale ? NP_DATA_STALE : NP_DATA_LIVE;
}

static void format_grouped(uint32_t value, char *out, size_t size)
{
    if (value >= 1000000U)
        (void)snprintf(out, size, "%lu.%03lu.%03lu", (unsigned long)(value / 1000000U),
            (unsigned long)((value / 1000U) % 1000U), (unsigned long)(value % 1000U));
    else if (value >= 1000U)
        (void)snprintf(out, size, "%lu.%03lu", (unsigned long)(value / 1000U),
                       (unsigned long)(value % 1000U));
    else (void)snprintf(out, size, "%lu", (unsigned long)value);
}

static void format_index_value(uint32_t centi_points, char *out, size_t size)
{
    char points[20] = {0};
    format_grouped(centi_points / 100U, points, sizeof(points));
    (void)snprintf(out, size, "%s pts", points);
}

static void format_change(int16_t basis_points, char *out, size_t size)
{
    const int32_t hundredths = basis_points;
    const uint32_t magnitude = (uint32_t)(hundredths < 0 ? -hundredths : hundredths);
    const char *const sign = hundredths > 0 ? "+" : hundredths < 0 ? "-" : "";
    (void)snprintf(out, size, "%s%lu,%02lu%%", sign,
                   (unsigned long)(magnitude / 100U), (unsigned long)(magnitude % 100U));
}

static void format_usd(uint32_t cents, char *out, size_t size)
{
    char grouped[20] = {0};
    format_grouped(cents / 100U, grouped, sizeof(grouped));
    (void)snprintf(out, size, "%s", grouped);
}

static void format_volume(uint64_t cents, char *out, size_t size)
{
    const uint64_t dollars = cents / 100U;
    if (dollars >= UINT64_C(1000000000))
        (void)snprintf(out, size, "%llu,%02llu bi",
            (unsigned long long)(dollars / UINT64_C(1000000000)),
            (unsigned long long)((dollars % UINT64_C(1000000000)) / UINT64_C(10000000)));
    else if (dollars >= UINT64_C(1000000))
        (void)snprintf(out, size, "%llu,%02llu mi",
            (unsigned long long)(dollars / UINT64_C(1000000)),
            (unsigned long long)((dollars % UINT64_C(1000000)) / UINT64_C(10000)));
    else (void)snprintf(out, size, "%llu", (unsigned long long)dollars);
}

static void market_strip_sync(np_market_strip_view_t *strip,
                              const char *value, const char *change,
                              bool available, bool positive)
{
    np_set_text(strip->value, available ? value : "--");
    np_set_text(strip->change, available ? change : "--");
    set_metric_state(strip->change_icon, strip->change, available, positive);
}

static np_market_strip_view_t bottom_strip(lv_obj_t *root, uint8_t index,
                                            const char *title, const char *glyph,
                                            lv_color_t badge_color,
                                            lv_color_t glyph_color)
{
    np_market_strip_view_t strip = {0};
    const int32_t width = MARKET_BOTTOM_STRIP_W;
    const int32_t x = 18 + MARKET_FEAR_W + MARKET_GAP +
                      (int32_t)index * (width + MARKET_GAP);
    strip.root = market_card(root, x, MARKET_BOTTOM_Y, width, MARKET_BOTTOM_H);
    (void)np_fill(strip.root, 8, 34, 48, 48, badge_color, LV_OPA_COVER,
                  LV_RADIUS_CIRCLE);
    np_label(strip.root, glyph, NP_FONT_ICON_BADGE, glyph_color, 8, 30, 48,
             LV_TEXT_ALIGN_CENTER);
    np_label(strip.root, title, NP_FONT_MD, np_c_text_2(), 68, 12, width - 78,
             LV_TEXT_ALIGN_LEFT);
    strip.value = np_label(strip.root, "--", NP_FONT_LG, np_c_text(), 68, 42,
                           width - 78, LV_TEXT_ALIGN_LEFT);
    strip.change_icon = np_label(strip.root, "", NP_FONT_ICON, np_c_text_3(),
                                 68, 74, 28, LV_TEXT_ALIGN_LEFT);
    strip.change = np_label(strip.root, "--", NP_FONT_SM, np_c_text_3(),
                            102, 76, width - 112, LV_TEXT_ALIGN_LEFT);
    return strip;
}

static const char *fear_label(uint8_t classification)
{
    static const char *const labels[] = {
        "Medo\nextremo", "Medo", "Neutro", "Ganância", "Ganância\nextrema",
    };
    return classification <= 4U ? labels[classification] : "--";
}

static const uint32_t fear_gradient_stops[5] = {
    0xFF454D, 0xF28A32, 0xF3D13A, 0x8BCB35, 0x08C995,
};

static lv_color_t fear_gradient_color(uint8_t value)
{
    if (value > 100U) value = 100U;
    const uint32_t scaled = (uint32_t)value * 4U;
    const uint8_t stop = (uint8_t)(scaled / 100U);
    if (stop >= 4U) return lv_color_hex(fear_gradient_stops[4]);

    const uint16_t fraction = (uint16_t)(scaled % 100U);
    const uint32_t from = fear_gradient_stops[stop];
    const uint32_t to = fear_gradient_stops[stop + 1U];
    const uint8_t red = (uint8_t)((((from >> 16) & 0xFFU) * (100U - fraction) +
                                   ((to >> 16) & 0xFFU) * fraction + 50U) / 100U);
    const uint8_t green = (uint8_t)((((from >> 8) & 0xFFU) * (100U - fraction) +
                                     ((to >> 8) & 0xFFU) * fraction + 50U) / 100U);
    const uint8_t blue = (uint8_t)(((from & 0xFFU) * (100U - fraction) +
                                    (to & 0xFFU) * fraction + 50U) / 100U);
    return lv_color_make(red, green, blue);
}

static void fear_gauge_draw_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (obj == NULL || layer == NULL) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const lv_point_t center = {
        .x = coords.x1 + MARKET_FEAR_CENTER_X,
        .y = coords.y1 + MARKET_FEAR_CENTER_Y,
    };

    for (uint8_t i = 0; i < MARKET_FEAR_ARC_STEPS; ++i) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.base.layer = layer;
        arc.center = center;
        arc.radius = MARKET_FEAR_RADIUS;
        arc.width = MARKET_FEAR_ARC_WIDTH;
        arc.start_angle = 180 + (180 * i) / MARKET_FEAR_ARC_STEPS;
        arc.end_angle = 180 + (180 * (i + 1U)) / MARKET_FEAR_ARC_STEPS;
        arc.color = fear_gradient_color((uint8_t)((100U * (2U * i + 1U)) /
                                                   (2U * MARKET_FEAR_ARC_STEPS)));
        arc.opa = LV_OPA_COVER;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
    }
}

static void fear_marker_set(lv_obj_t *marker, uint8_t value)
{
    if (value > 100U) value = 100U;
    const float angle = ((float)value / 100.0f) * 3.14159265f;
    const int32_t x = MARKET_FEAR_CENTER_X -
        (int32_t)lroundf((float)MARKET_FEAR_RADIUS * cosf(angle));
    const int32_t y = MARKET_FEAR_CENTER_Y -
        (int32_t)lroundf((float)MARKET_FEAR_RADIUS * sinf(angle));
    const int32_t half = MARKET_FEAR_MARKER_SIZE / 2;
    lv_obj_set_pos(marker, x - half, y - half);
}

np_market_view_t np_market_build_with_header(lv_obj_t *parent,
                                               const np_header_t *header)
{
    np_market_view_t view = {0};
    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    lv_obj_t *btc = market_card(view.root, MARKET_LEFT_X, MARKET_LEFT_Y,
                                MARKET_LEFT_W, MARKET_LEFT_H);
    (void)np_bitcoin_badge(btc, 16, 16, 72);
    np_label(btc, "Bitcoin", NP_FONT_TITLE, np_c_text(), 110, 18, 240,
             LV_TEXT_ALIGN_LEFT);
    np_label(btc, "BTC/USD", NP_FONT_LG, np_c_text_2(), 110, 52, 220,
             LV_TEXT_ALIGN_LEFT);
    view.btc_status = np_status_dot(btc, 512, 29, NP_DATA_UNAVAILABLE);
    view.btc_spark = np_spark(btc, MARKET_BTC_SPARK_X, MARKET_BTC_SPARK_Y,
                              MARKET_BTC_SPARK_W, MARKET_BTC_SPARK_H);

    np_label(btc, "US$", NP_FONT_SM, np_c_text_2(), 20, 183, 48,
             LV_TEXT_ALIGN_LEFT);
    view.btc_price = np_label(btc, "--", NP_FONT_BRAND, np_c_text(), 70, 154,
                              MARKET_LEFT_W - 90, LV_TEXT_ALIGN_LEFT);
    view.btc_change_icon = np_label(btc, NP_ICON_RISE, NP_FONT_ICON,
                                    np_c_text_3(), 20, 238, 30,
                                    LV_TEXT_ALIGN_CENTER);
    view.btc_change = np_label(btc, "--", NP_FONT_LG, np_c_text_3(),
                               56, 235, 260, LV_TEXT_ALIGN_LEFT);
    np_hline(btc, 20, 310, MARKET_LEFT_W - 40);

    static const char *const metric_titles[] = {
        "Máx. 24 h", "Mín. 24 h", "Volume 24 h", "US$/BRL"};
    lv_obj_t **metric_values[] = {
        &view.btc_high, &view.btc_low, &view.btc_volume, &view.btc_exchange};
    const int32_t metric_width = (MARKET_LEFT_W - 40) / 4;
    for (uint8_t i = 0; i < 4; ++i) {
        const int32_t x = 20 + (int32_t)i * metric_width;
        *metric_values[i] = np_label(btc, "--", NP_FONT_MD, np_c_text(), x, 334,
                                     metric_width - 8, LV_TEXT_ALIGN_LEFT);
        np_label(btc, metric_titles[i], NP_FONT_SM, np_c_text_2(), x, 312,
                 metric_width - 12, LV_TEXT_ALIGN_LEFT);
        if (i > 0) np_vline(btc, x - 12, 308, 50);
    }
    const int32_t exchange_x = 20 + 3 * metric_width;
    view.btc_exchange_change_icon = np_label(
        btc, "", NP_FONT_ICON, np_c_text_3(), exchange_x + 48, 334, 24,
        LV_TEXT_ALIGN_LEFT);
    view.btc_exchange_change = np_label(
        btc, "--", NP_FONT_SM, np_c_text_3(), exchange_x + 72, 338,
        metric_width - 72, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(view.btc_exchange, 48);

    static const char *const names[] = {"Ethereum", "Solana", "BNB", "XRP"};
    static const char *const tickers[] = {"ETH/USD", "SOL/USD", "BNB/USD", "XRP/USD"};
    lv_obj_t *altcoin_values[4] = {0};
    lv_obj_t *altcoin_changes[4] = {0};
    lv_obj_t *altcoin_change_icons[4] = {0};
    const int32_t row_h = (MARKET_LEFT_H - MARKET_GAP * 3) / 4;
    for (uint8_t i = 0; i < 4; ++i) {
        const int32_t y = MARKET_LEFT_Y + (int32_t)i * (row_h + MARKET_GAP);
        lv_obj_t *row = market_card(view.root, MARKET_RIGHT_X, y,
                                    MARKET_RIGHT_W, row_h);
        /* Altcoin rows deliberately remain text-only, matching the request. */
        np_label(row, names[i], NP_FONT_LG, np_c_text(), 20, 17, 180,
                 LV_TEXT_ALIGN_LEFT);
        np_label(row, tickers[i], NP_FONT_MD, np_c_text_2(), 20, 48, 170,
                 LV_TEXT_ALIGN_LEFT);
        altcoin_values[i] = np_label(row, "--", NP_FONT_LG, np_c_text(),
                                     MARKET_RIGHT_W - 132, 14, 112,
                                     LV_TEXT_ALIGN_RIGHT);
        altcoin_change_icons[i] = np_label(row, "", NP_FONT_ICON,
                                           np_c_text_3(), MARKET_RIGHT_W - 132,
                                           50, 28, LV_TEXT_ALIGN_LEFT);
        altcoin_changes[i] = np_label(row, "--", NP_FONT_LG, np_c_text_3(),
                                      MARKET_RIGHT_W - 100, 47, 84,
                                      LV_TEXT_ALIGN_RIGHT);
    }

    /* The sample's indicator and US indices have reserved slots until their
     * feeds are integrated. Material Symbols provides their category icons. */
    view.fear_root = market_card(view.root, 18, MARKET_BOTTOM_Y,
                                 MARKET_FEAR_W, MARKET_BOTTOM_H);
    view.fear_gauge = lv_obj_create(view.fear_root);
    lv_obj_remove_style_all(view.fear_gauge);
    lv_obj_set_pos(view.fear_gauge, 0, 0);
    lv_obj_set_size(view.fear_gauge, MARKET_FEAR_W, MARKET_BOTTOM_H);
    lv_obj_set_style_bg_opa(view.fear_gauge, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view.fear_gauge, 0, 0);
    lv_obj_clear_flag(view.fear_gauge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view.fear_gauge, fear_gauge_draw_event,
                        LV_EVENT_DRAW_MAIN, NULL);
    view.fear_marker = lv_obj_create(view.fear_root);
    lv_obj_remove_style_all(view.fear_marker);
    lv_obj_set_size(view.fear_marker, MARKET_FEAR_MARKER_SIZE,
                    MARKET_FEAR_MARKER_SIZE);
    lv_obj_set_style_radius(view.fear_marker, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(view.fear_marker, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(view.fear_marker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view.fear_marker, 2, 0);
    lv_obj_set_style_border_color(view.fear_marker, np_c_surface(), 0);
    lv_obj_clear_flag(view.fear_marker, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view.fear_marker, LV_OBJ_FLAG_HIDDEN);
    np_label(view.fear_root, "Fear & Greed", NP_FONT_LG, np_c_text_2(),
             28, 12, 200, LV_TEXT_ALIGN_LEFT);
    view.fear_value = np_label(view.fear_root, "--", NP_FONT_TITLE, np_c_text(),
                               42, 67, 102, LV_TEXT_ALIGN_CENTER);
    view.fear_classification = np_label(view.fear_root, "--", NP_FONT_SM,
                                        np_c_text_2(), 170, 74, 120,
                                        LV_TEXT_ALIGN_LEFT);
    view.sp500 = bottom_strip(view.root, 0, "S&P 500", NP_ICON_MARKET,
                              np_c_negative_bg(), np_c_negative());
    view.nasdaq = bottom_strip(view.root, 1, "Nasdaq", NP_ICON_MARKET,
                               np_c_accent_bg(), np_c_accent());
    view.ibovespa = bottom_strip(view.root, 2, "Ibovespa", NP_ICON_MARKET,
                                 np_c_positive(), np_c_text_on_accent());

    for (uint8_t i = 0; i < 4; ++i) {
        view.altcoins[i].value = altcoin_values[i];
        view.altcoins[i].change = altcoin_changes[i];
        view.altcoins[i].change_icon = altcoin_change_icons[i];
    }
    return view;
}

void np_market_sync(np_market_view_t *view,
                    const offline_data_snapshot_t *snapshot)
{
    if (view == NULL || snapshot == NULL) return;
    const offline_market_data_t *btc = &snapshot->market;
    const offline_exchange_data_t *fx = &snapshot->exchange;
    const offline_ibovespa_data_t *ibov = &snapshot->ibovespa;
    const bool btc_available = btc->available;
    const bool positive = btc->change_24h_basis_points >= 0;
    np_status_dot_set(&view->btc_status, market_data_state(btc_available, btc->stale));
    if (btc_available) {
        char price[24] = {0}, change[24] = {0}, high[24] = {0}, low[24] = {0};
        char volume[24] = {0};
        format_usd(btc->bitcoin_usd_cents, price, sizeof(price));
        format_change(btc->change_24h_basis_points, change, sizeof(change));
        np_set_text(view->btc_price, price);
        np_set_text(view->btc_change, change);
        const lv_color_t color = positive ? np_c_positive() : np_c_negative();
        np_set_text(view->btc_change_icon,
                    positive ? NP_ICON_RISE : NP_ICON_FALL);
        np_set_text_color(view->btc_change, color);
        np_set_text_color(view->btc_change_icon, color);
        np_set_text(view->btc_high, btc->high_24h_available
            ? (format_usd(btc->high_24h_usd_cents, high, sizeof(high)), high) : "--");
        np_set_text(view->btc_low, btc->low_24h_available
            ? (format_usd(btc->low_24h_usd_cents, low, sizeof(low)), low) : "--");
        np_set_text(view->btc_volume, btc->volume_24h_available
            ? (format_volume(btc->volume_24h_usd_cents, volume, sizeof(volume)), volume) : "--");
    } else {
        np_set_text(view->btc_price, "--"); np_set_text(view->btc_change, "--");
        np_set_text(view->btc_change_icon, "");
        np_set_text(view->btc_high, "--"); np_set_text(view->btc_low, "--");
        np_set_text(view->btc_volume, "--");
    }
    if (fx->available) {
        char value[24] = {0}, change[24] = {0};
        const uint32_t cents = (fx->usd_brl_ten_thousandths + 50U) / 100U;
        (void)snprintf(value, sizeof(value), "%lu,%02lu",
                       (unsigned long)(cents / 100U), (unsigned long)(cents % 100U));
        if (fx->change_available)
            format_change(fx->change_basis_points, change, sizeof(change));
        np_set_text(view->btc_exchange, value);
        np_set_text(view->btc_exchange_change, fx->change_available ? change : "--");
        set_metric_state(view->btc_exchange_change_icon,
                         view->btc_exchange_change,
                         fx->change_available,
                         fx->change_basis_points >= 0);
        np_set_text_color(view->btc_exchange,
                          fx->stale ? np_c_warning() : np_c_text());
    } else {
        np_set_text(view->btc_exchange, "--");
        np_set_text(view->btc_exchange_change, "--");
        set_metric_state(view->btc_exchange_change_icon,
                         view->btc_exchange_change, false, false);
        np_set_text_color(view->btc_exchange, np_c_text_3());
    }
    if (btc_available && btc->history_count >= 2U) {
        int32_t points[OFFLINE_MARKET_HISTORY_MAX] = {0};
        uint8_t count = btc->history_count;
        if (count > OFFLINE_MARKET_HISTORY_MAX) count = OFFLINE_MARKET_HISTORY_MAX;
        for (uint8_t i = 0; i < count; ++i) {
            points[i] = (int32_t)(btc->history_usd_cents[i] / 100U);
        }
        np_spark_set(&view->btc_spark, points, count,
                     MARKET_BTC_SPARK_W, MARKET_BTC_SPARK_H,
                     positive ? np_c_positive() : np_c_negative());
    } else {
        np_spark_set(&view->btc_spark, NULL, 0U,
                     MARKET_BTC_SPARK_W, MARKET_BTC_SPARK_H, np_c_btc());
    }

    if (snapshot->fear_greed.available) {
        np_set_text(view->fear_value, "--");
        char value[8] = {0};
        (void)snprintf(value, sizeof(value), "%u", (unsigned)snapshot->fear_greed.value);
        np_set_text(view->fear_value, value);
        np_set_text(view->fear_classification,
                    fear_label(snapshot->fear_greed.classification));
        fear_marker_set(view->fear_marker, snapshot->fear_greed.value);
        np_set_visible(view->fear_marker, true);
        const lv_color_t fear_color = fear_gradient_color(snapshot->fear_greed.value);
        np_set_text_color(view->fear_value, fear_color);
        np_set_text_color(view->fear_classification, fear_color);
        lv_obj_set_style_text_font(view->fear_classification,
            snapshot->fear_greed.classification > 0U &&
            snapshot->fear_greed.classification < 4U
                ? NP_FONT_MD : NP_FONT_SM, LV_PART_MAIN);
    } else {
        np_set_text(view->fear_value, "--");
        np_set_text(view->fear_classification, "Sem dados");
        lv_obj_add_flag(view->fear_marker, LV_OBJ_FLAG_HIDDEN);
        np_set_text_color(view->fear_value, np_c_text());
        np_set_text_color(view->fear_classification, np_c_text_3());
        lv_obj_set_style_text_font(view->fear_classification,
                                   NP_FONT_SM, LV_PART_MAIN);
    }

    if (ibov->available) {
        char value[24] = {0}, change[24] = {0};
        format_index_value(ibov->value_centi_points, value, sizeof(value));
        if (ibov->change_available)
            format_change(ibov->change_basis_points, change, sizeof(change));
        market_strip_sync(&view->ibovespa, value, change, true,
                          ibov->change_basis_points >= 0);
        if (!ibov->change_available) {
            np_set_text(view->ibovespa.change_icon, "");
            np_set_text_color(view->ibovespa.change, np_c_text_3());
        }
    } else market_strip_sync(&view->ibovespa, "--", "--", false, false);

    const offline_index_data_t *const indices[] = {&snapshot->sp500, &snapshot->nasdaq};
    np_market_strip_view_t *const strips[] = {&view->sp500, &view->nasdaq};
    for (size_t index = 0U; index < 2U; ++index) {
        const offline_index_data_t *const quote = indices[index];
        if (!quote->available) {
            market_strip_sync(strips[index], "--", "--", false, false);
            continue;
        }
        char value[24] = {0}, change[24] = {0};
        format_index_value(quote->value_centi_points, value, sizeof(value));
        if (quote->change_available)
            format_change(quote->change_basis_points, change, sizeof(change));
        market_strip_sync(strips[index], value,
                          quote->change_available ? change : "--", true,
                          quote->change_basis_points >= 0);
        if (!quote->change_available) {
            np_set_text(strips[index]->change_icon, "");
            np_set_text_color(strips[index]->change, np_c_text_3());
        }
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const offline_altcoin_data_t *coin = &snapshot->altcoins[i];
        if (!coin->available) {
            np_set_text(view->altcoins[i].value, "--");
            np_set_text(view->altcoins[i].change, "--");
            set_metric_state(view->altcoins[i].change_icon, view->altcoins[i].change, false, false);
            continue;
        }
        char price[24] = {0}, change[24] = {0};
        const uint64_t whole = coin->usd_micros / UINT64_C(1000000);
        const uint32_t fraction = (uint32_t)(coin->usd_micros % UINT64_C(1000000));
        if (whole >= 1000U) {
            (void)snprintf(price, sizeof(price), "%llu",
                           (unsigned long long)whole);
        } else {
            uint32_t trimmed = fraction;
            uint8_t decimals = 6U;
            while (decimals > 2U && trimmed % 10U == 0U) { trimmed /= 10U; --decimals; }
            (void)snprintf(price, sizeof(price), "%llu,%0*lu",
                (unsigned long long)whole, (int)decimals, (unsigned long)trimmed);
        }
        format_change(coin->change_24h_basis_points, change, sizeof(change));
        np_set_text(view->altcoins[i].value, price);
        np_set_text(view->altcoins[i].change, change);
        set_metric_state(view->altcoins[i].change_icon, view->altcoins[i].change,
                         true, coin->change_24h_basis_points >= 0);
        if (coin->stale) np_set_text_color(view->altcoins[i].change, np_c_warning());
    }
}

lv_obj_t *np_market_create(lv_obj_t *parent)
{
    return np_market_build_with_header(parent, NULL).root;
}
