#include "np_components.h"
#include "np_styles.h"
#include "../assets/np_btc_tilted_icon.h"

#include <limits.h>
#include <string.h>

/*
 * O preenchimento do sparkline usa o mesmo mecanismo do exemplo oficial
 * "Faded area under line chart" do LVGL 9. O header privado é necessário
 * apenas para acessar a layer do draw task. Em builds onde ele não estiver
 * exposto, o chart continua funcional e simplesmente fica sem o fade.
 */
#define NP_ENABLE_SPARK_FADE 1

#if NP_ENABLE_SPARK_FADE && LV_USE_CHART && LV_DRAW_SW_COMPLEX
#  if defined(__has_include)
#    if __has_include("src/lvgl_private.h")
#      include "src/lvgl_private.h"
#      define NP_HAVE_LVGL_PRIVATE_DRAW 1
#    elif __has_include("lvgl_private.h")
#      include "lvgl_private.h"
#      define NP_HAVE_LVGL_PRIVATE_DRAW 1
#    elif __has_include("lvgl/src/lvgl_private.h")
#      include "lvgl/src/lvgl_private.h"
#      define NP_HAVE_LVGL_PRIVATE_DRAW 1
#    endif
#  endif
#endif

#ifndef NP_HAVE_LVGL_PRIVATE_DRAW
#define NP_HAVE_LVGL_PRIVATE_DRAW 0
#endif

/* ---------------- escrita guardada ---------------- */

void np_set_text(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL) return;
    if (lv_strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void np_set_text_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), color)) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

void np_set_bg_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) return;
    if (!lv_color_eq(lv_obj_get_style_bg_color(obj, LV_PART_MAIN), color)) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
}

void np_set_bg_opa(lv_obj_t *obj, lv_opa_t opa)
{
    if (obj == NULL) return;
    if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) != opa) {
        lv_obj_set_style_bg_opa(obj, opa, 0);
    }
}

void np_set_radius(lv_obj_t *obj, int32_t radius)
{
    if (obj == NULL) return;
    if (lv_obj_get_style_radius(obj, LV_PART_MAIN) != radius) {
        lv_obj_set_style_radius(obj, radius, 0);
    }
}

void np_set_visible(lv_obj_t *obj, bool visible)
{
    if (obj == NULL) return;
    const bool hidden = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
    if (visible && hidden) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else if (!visible && !hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------------- primitivas ---------------- */

static lv_obj_t *bare_box(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t *np_scene(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_add_style(obj, np_st_screen(), 0);
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

lv_obj_t *np_group(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    return bare_box(parent, x, y, w, h);
}

lv_obj_t *np_surface(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, h);
    lv_obj_add_style(obj, np_st_surface(), 0);
    return obj;
}

lv_obj_t *np_raised(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, h);
    lv_obj_add_style(obj, np_st_raised(), 0);
    return obj;
}

lv_obj_t *np_glass(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, np_c_surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_50, 0);
    lv_obj_set_style_border_color(obj, np_c_text_2(), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, NP_RADIUS_SURFACE, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    return obj;
}

lv_obj_t *np_panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, h);
    lv_obj_add_style(obj, np_st_panel(), 0);
    return obj;
}

lv_obj_t *np_fill(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  lv_color_t color, lv_opa_t opa, int32_t radius)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, opa, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

lv_obj_t *np_image(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_obj_remove_style_all(image);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_TOP_LEFT);
    lv_obj_set_pos(image, x, y);
    lv_obj_set_size(image, w, h);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
    return image;
}

void np_image_set_source(lv_obj_t *image, const void *source)
{
    if (image == NULL) return;

    if (source == NULL) {
        np_set_visible(image, false);
        return;
    }

    if (lv_image_get_src(image) != source) {
        lv_image_set_src(image, source);
    }
    np_set_visible(image, true);
    lv_obj_invalidate(image);
}

void np_set_clip_corner(lv_obj_t *obj, bool enabled)
{
    if (obj == NULL) return;
    if (lv_obj_get_style_clip_corner(obj, LV_PART_MAIN) != enabled) {
        lv_obj_set_style_clip_corner(obj, enabled, 0);
    }
}

lv_obj_t *np_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                   lv_color_t color, int32_t x, int32_t y, int32_t w,
                   lv_text_align_t align)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_add_style(obj, np_st_label(), 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, w);
    lv_label_set_text(obj, text != NULL ? text : "");
    return obj;
}

lv_obj_t *np_hline(lv_obj_t *parent, int32_t x, int32_t y, int32_t w)
{
    lv_obj_t *obj = bare_box(parent, x, y, w, 1);
    lv_obj_add_style(obj, np_st_hairline(), 0);
    return obj;
}

lv_obj_t *np_vline(lv_obj_t *parent, int32_t x, int32_t y, int32_t h)
{
    lv_obj_t *obj = bare_box(parent, x, y, 1, h);
    lv_obj_add_style(obj, np_st_hairline(), 0);
    return obj;
}

lv_obj_t *np_dot(lv_obj_t *parent, int32_t x, int32_t y, int32_t size, lv_color_t color)
{
    return np_fill(parent, x, y, size, size, color, LV_OPA_COVER, LV_RADIUS_CIRCLE);
}

lv_obj_t *np_icon_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t size, const char *symbol)
{
    lv_obj_t *button = bare_box(parent, x, y, size, size);
    lv_obj_add_style(button, np_st_icon_button(), 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    const int32_t font_h = NP_FONT_ICON->line_height;
    const int32_t top = (size - font_h) / 2;
    np_label(button, symbol, NP_FONT_ICON, np_c_text_2(),
             0, top, size, LV_TEXT_ALIGN_CENTER);
    return button;
}

const char *np_wifi_signal_icon(int8_t rssi)
{
    if (rssi >= -55) return NP_ICON_WIFI;
    if (rssi >= -67) return NP_ICON_WIFI_MEDIUM;
    return NP_ICON_WIFI_LOW;
}

/* ---------------- compostos legados ---------------- */

np_metric_t np_metric(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                      const char *caption, const char *value, const lv_font_t *value_font)
{
    np_metric_t m;
    m.caption = np_label(parent, caption, NP_FONT_SM, np_c_text_3(),
                         x, y, w, LV_TEXT_ALIGN_LEFT);
    m.value = np_label(parent, value, value_font, np_c_text(),
                       x, y + NP_SP_24, w, LV_TEXT_ALIGN_LEFT);
    return m;
}

np_pill_t np_status_pill(lv_obj_t *parent, int32_t x, int32_t y, np_data_state_t state)
{
    np_pill_t pill;
    const lv_color_t color = np_c_for_state(state);
    pill.dot = np_dot(parent, x, y + 5, 8, color);
    pill.label = np_label(parent, np_label_for_state(state), NP_FONT_SM, color,
                          x + 16, y, 140, LV_TEXT_ALIGN_LEFT);
    return pill;
}

void np_status_pill_set(np_pill_t *pill, np_data_state_t state)
{
    if (pill == NULL) return;
    const lv_color_t color = np_c_for_state(state);
    np_set_bg_color(pill->dot, color);
    np_set_text(pill->label, np_label_for_state(state));
    np_set_text_color(pill->label, color);
}

/* ---------------- Home V2 ---------------- */

np_status_dot_t np_status_dot(lv_obj_t *parent, int32_t x, int32_t y,
                              np_data_state_t state)
{
    np_status_dot_t status = {
        .dot = np_dot(parent, x, y, NP_STATUS_DOT_SIZE, np_c_for_state(state)),
    };
    return status;
}

void np_status_dot_set(np_status_dot_t *status, np_data_state_t state)
{
    if (status == NULL) return;
    np_set_bg_color(status->dot, np_c_for_state(state));
}

static lv_obj_t *metric_icon_create(lv_obj_t *parent, np_metric_icon_t kind,
                                     int32_t x, int32_t y, lv_color_t color)
{
    const char *symbol = NP_ICON_UV;

    switch (kind) {
        case NP_METRIC_ICON_WIND:
            symbol = NP_ICON_WIND;
            break;
        case NP_METRIC_ICON_HUMIDITY:
            symbol = NP_ICON_HUMIDITY;
            break;
        case NP_METRIC_ICON_TEMPERATURE:
            symbol = NP_ICON_TEMPERATURE;
            break;
        case NP_METRIC_ICON_UV:
        default:
            symbol = NP_ICON_UV;
            break;
    }

    lv_obj_t *root = bare_box(parent, x, y, 32, 32);
    np_label(root, symbol, NP_FONT_ICON, color,
             0, (32 - NP_FONT_ICON->line_height) / 2, 32,
             LV_TEXT_ALIGN_CENTER);
    return root;
}

np_metric_tile_t np_metric_tile(lv_obj_t *parent, int32_t x, int32_t y,
                                int32_t w, int32_t h,
                                np_metric_icon_t icon_kind, lv_color_t icon_color,
                                const char *caption, const char *value)
{
    np_metric_tile_t tile = {0};
    tile.root = bare_box(parent, x, y, w, h);
    lv_obj_add_style(tile.root, np_st_metric_tile(), 0);

    tile.icon = metric_icon_create(tile.root, icon_kind, NP_SP_12, NP_SP_8, icon_color);
    tile.caption = np_label(tile.root, caption, NP_FONT_SM, np_c_text_2(),
                            NP_SP_12, 42, w - 2 * NP_SP_12,
                            LV_TEXT_ALIGN_LEFT);
    tile.value = np_label(tile.root, value, NP_FONT_MD, np_c_text(),
                          NP_SP_12, h - 31, w - 2 * NP_SP_12,
                          LV_TEXT_ALIGN_LEFT);
    return tile;
}

lv_obj_t *np_location_icon(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t size, lv_color_t color)
{
    lv_obj_t *root = bare_box(parent, x, y, size, size);
    np_label(root, NP_ICON_LOCATION, NP_FONT_ICON, color,
             0, (size - NP_FONT_ICON->line_height) / 2, size,
             LV_TEXT_ALIGN_CENTER);
    return root;
}

lv_obj_t *np_bitcoin_badge(lv_obj_t *parent, int32_t x, int32_t y, int32_t size)
{
    lv_obj_t *badge = np_fill(parent, x, y, size, size,
                              np_c_btc(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    /* A bitmap cache generated from the Material Bitcoin glyph at 20 degrees
     * avoids LVGL's per-frame transform layer and its temporary draw buffer. */
    lv_obj_t *icon = lv_image_create(badge);
    lv_image_set_src(icon, &np_btc_tilted_icon);
    lv_obj_set_size(icon, NP_BTC_TILTED_ICON_SIZE, NP_BTC_TILTED_ICON_SIZE);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_image_recolor(icon, np_c_text(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    return badge;
}

static lv_obj_t *market_badge_create(lv_obj_t *parent, int32_t x, int32_t y,
                                     int32_t size, np_market_icon_t kind)
{
    const lv_color_t bg = kind == NP_MARKET_ICON_DOLLAR
                              ? np_c_positive()
                              : np_c_market_blue();
    const char *symbol = kind == NP_MARKET_ICON_DOLLAR
                             ? NP_ICON_DOLLAR
                             : NP_ICON_MARKET;

    lv_obj_t *badge = np_fill(parent, x, y, size, size,
                              bg, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(badge, symbol, NP_FONT_ICON_BADGE, np_c_text(),
             0, (size - NP_FONT_ICON_BADGE->line_height) / 2, size,
             LV_TEXT_ALIGN_CENTER);
    return badge;
}

np_market_strip_t np_market_strip(lv_obj_t *parent, int32_t x, int32_t y,
                                  int32_t w, int32_t h,
                                  np_market_icon_t icon_kind,
                                  const char *title, const char *ticker)
{
    np_market_strip_t strip = {0};
    strip.root = np_panel(parent, x, y, w, h);

    strip.lead = market_badge_create(strip.root, 18, (h - 52) / 2, 52, icon_kind);

    strip.title = np_label(strip.root, title, NP_FONT_MD, np_c_text(),
                           86, 15, 188, LV_TEXT_ALIGN_LEFT);
    strip.ticker = np_label(strip.root, ticker, NP_FONT_SM, np_c_text_3(),
                            86, 46, 160, LV_TEXT_ALIGN_LEFT);

    strip.value = np_label(strip.root, "--", NP_FONT_LG, np_c_text(),
                           245, 14, w - 269, LV_TEXT_ALIGN_RIGHT);
    strip.change_icon = np_label(strip.root, "", NP_FONT_ICON, np_c_text_3(),
                                 w - 146, 54, 24, LV_TEXT_ALIGN_LEFT);
    strip.change = np_label(strip.root, "--", NP_FONT_SM, np_c_text_3(),
                            w - 122, 55, 98, LV_TEXT_ALIGN_RIGHT);

    /* Os strips inferiores seguem o mockup: sem status dot próprio. O handle
     * continua existindo para manter o contrato, porém fica invisível. */
    strip.status = np_status_dot(strip.root, w - 18, 12, NP_DATA_UNAVAILABLE);
    np_set_visible(strip.status.dot, false);
    return strip;
}

/* ---------------- relogio ---------------- */

np_clock_t np_clock(lv_obj_t *parent, int32_t x, int32_t y, const lv_font_t *font,
                    lv_color_t color)
{
    np_clock_t clock;
    const int32_t digit_w = font->line_height * 3 / 5;
    const int32_t colon_w = digit_w / 2;
    const int32_t width[5] = {digit_w, digit_w, colon_w, digit_w, digit_w};
    int32_t cursor = x;

    for (int i = 0; i < 5; ++i) {
        clock.slot[i] = np_label(parent, i == 2 ? ":" : "-", font, color,
                                 cursor, y, width[i], LV_TEXT_ALIGN_CENTER);
        cursor += width[i];
    }
    return clock;
}

void np_clock_set(np_clock_t *clock, const char *hhmm)
{
    if (clock == NULL || hhmm == NULL || strlen(hhmm) != 5) return;

    char one[2] = {0, 0};
    for (int i = 0; i < 5; ++i) {
        one[0] = hhmm[i];
        np_set_text(clock->slot[i], one);
    }
}

/* ---------------- botao/header ---------------- */

lv_obj_t *np_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                    const char *text, bool primary)
{
    return np_form_button(parent, x, y, w, h, text,
                          primary ? NP_FORM_BUTTON_PRIMARY
                                  : NP_FORM_BUTTON_SECONDARY);
}

static lv_obj_t *icon_label(lv_obj_t *button)
{
    return button != NULL ? lv_obj_get_child(button, 0) : NULL;
}

static lv_obj_t *menu_grid_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t size)
{
    lv_obj_t *button = bare_box(parent, x, y, size, size);
    lv_obj_add_style(button, np_st_icon_button(), 0);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);

    const int32_t cell = 9;
    const int32_t gap = 4;
    const int32_t total = cell * 2 + gap;
    const int32_t ox = (size - total) / 2;
    const int32_t oy = (size - total) / 2;

    np_fill(button, ox, oy, cell, cell, np_c_accent(), LV_OPA_COVER, 2);
    np_fill(button, ox + cell + gap, oy, cell, cell, np_c_text(), LV_OPA_COVER, 2);
    np_fill(button, ox, oy + cell + gap, cell, cell, np_c_text(), LV_OPA_COVER, 2);
    np_fill(button, ox + cell + gap, oy + cell + gap, cell, cell,
            np_c_accent(), LV_OPA_COVER, 2);
    return button;
}

static int32_t s_drawer_touch_start_x;

static void drawer_set_visible(lv_obj_t *overlay, bool visible)
{
    if (overlay == NULL) return;
    if (visible) {
        lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(overlay);
    } else {
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static int32_t drawer_pointer_x(void)
{
    lv_indev_t *indev = lv_indev_active();
    lv_point_t point = {0};
    if (indev != NULL) lv_indev_get_point(indev, &point);
    return point.x;
}

static void drawer_overlay_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *overlay = lv_event_get_user_data(event);
    if (overlay == NULL) overlay = lv_event_get_target(event);

    if (code == LV_EVENT_PRESSED) {
        s_drawer_touch_start_x = drawer_pointer_x();
    } else if (code == LV_EVENT_RELEASED) {
        if (drawer_pointer_x() - s_drawer_touch_start_x <= -48) {
            drawer_set_visible(overlay, false);
        }
    } else if (code == LV_EVENT_CLICKED && lv_event_get_target(event) == overlay) {
        drawer_set_visible(overlay, false);
    }
}

static void drawer_edge_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *overlay = lv_event_get_user_data(event);

    if (code == LV_EVENT_PRESSED) {
        s_drawer_touch_start_x = drawer_pointer_x();
    } else if (code == LV_EVENT_RELEASED) {
        if (drawer_pointer_x() - s_drawer_touch_start_x >= 48) {
            drawer_set_visible(overlay, true);
        }
    }
}

static void drawer_toggle_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    lv_obj_t *overlay = lv_event_get_user_data(event);
    drawer_set_visible(overlay, lv_obj_has_flag(overlay, LV_OBJ_FLAG_HIDDEN));
}

typedef enum {
    DRAWER_ICON_HOME = 0,
    DRAWER_ICON_WEATHER,
    DRAWER_ICON_MARKET,
    DRAWER_ICON_CALENDAR,
    DRAWER_ICON_SETTINGS,
} drawer_icon_t;

static void drawer_shape(lv_obj_t *item, drawer_icon_t kind, lv_color_t color,
                         lv_color_t item_bg)
{
    (void)item_bg;

    const char *symbol = NP_ICON_SETTINGS;

    switch (kind) {
        case DRAWER_ICON_HOME:
            symbol = NP_ICON_HOME;
            break;
        case DRAWER_ICON_WEATHER:
            symbol = NP_ICON_WEATHER;
            break;
        case DRAWER_ICON_MARKET:
            symbol = NP_ICON_MARKET;
            break;
        case DRAWER_ICON_CALENDAR:
            symbol = NP_ICON_CALENDAR;
            break;
        case DRAWER_ICON_SETTINGS:
        default:
            symbol = NP_ICON_SETTINGS;
            break;
    }

    np_label(item, symbol, NP_FONT_ICON, color,
             0, (52 - NP_FONT_ICON->line_height) / 2, 52,
             LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *drawer_item(lv_obj_t *drawer, int32_t y, drawer_icon_t kind, bool active)
{
    const lv_color_t bg = active ? np_c_accent() : np_c_surface();
    const lv_color_t fg = active ? np_c_text() : np_c_text_2();

    lv_obj_t *item = np_fill(drawer, 16, y, 52, 52,
                             bg, LV_OPA_COVER, NP_RADIUS_TILE);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    drawer_shape(item, kind, fg, bg);
    return item;
}

np_header_t np_header(lv_obj_t *parent)
{
    np_header_t header = {0};

    /* Marca/menu: quatro quadrados, como no protótipo aprovado. */
    header.menu_button = menu_grid_button(parent, 24, 12, NP_TOUCH_TARGET);
    header.brand_nova = np_label(parent, "Nova", NP_FONT_LG, np_c_text(),
                                 88, 19, 64, LV_TEXT_ALIGN_LEFT);
    header.brand_panel = np_label(parent, "Panel", NP_FONT_LG, np_c_accent(),
                                  146, 19, 76, LV_TEXT_ALIGN_LEFT);
    header.user_greeting = np_label(parent, "", NP_FONT_MD, np_c_text_2(),
                                    250, 21, 380, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(header.user_greeting, LV_LABEL_LONG_DOT);
    lv_obj_add_flag(header.user_greeting, LV_OBJ_FLAG_HIDDEN);

    /* Grupo de ações encostado ao bloco exclusivo do relógio. */
    header.wifi_button =
        np_icon_button(parent, 650, 12, NP_TOUCH_TARGET, NP_ICON_WIFI_OFF);
    header.notifications_button =
        np_icon_button(parent, 706, 12, NP_TOUCH_TARGET, NP_ICON_NOTIFICATIONS);
    header.settings_button =
        np_icon_button(parent, 762, 12, NP_TOUCH_TARGET, NP_ICON_SETTINGS);
    header.bluetooth_button = NULL;

    header.alert_dot = np_dot(parent, 742, 12, 7, np_c_accent());
    np_set_visible(header.alert_dot, false);

    header.divider = np_vline(parent, 826, 12, 48);

    /* Sem data no header. O horário usa todo o bloco 844..1000 com
     * tipografia maior, como no layout anterior aprovado. */
    header.clock = np_clock(parent, 844, 3, NP_FONT_DISPLAY, np_c_text());
    header.date = NULL;

    /* Drawer retraído: rail visual do mockup, sem texto e sem ocupar largura
     * quando fechado. O overlay é transparente para não escurecer a Home. */
    header.drawer_scrim = np_fill(parent, 0, 0, NP_SCREEN_W, NP_SCREEN_H,
                                  np_c_bg(), LV_OPA_TRANSP, 0);
    lv_obj_add_flag(header.drawer_scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header.drawer_scrim, drawer_overlay_event_cb,
                        LV_EVENT_ALL, NULL);

    header.drawer = bare_box(header.drawer_scrim, 16, 88,
                             NP_DRAWER_W, NP_SCREEN_H - 112);
    lv_obj_add_style(header.drawer, np_st_drawer(), 0);
    lv_obj_add_flag(header.drawer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(header.drawer, LV_OBJ_FLAG_EVENT_BUBBLE);

    header.drawer_home_button =
        drawer_item(header.drawer, 20, DRAWER_ICON_HOME, true);
    (void)drawer_item(header.drawer, 88,  DRAWER_ICON_WEATHER, false);
    (void)drawer_item(header.drawer, 156, DRAWER_ICON_MARKET, false);
    (void)drawer_item(header.drawer, 224, DRAWER_ICON_CALENDAR, false);
    header.drawer_settings_button =
        drawer_item(header.drawer, 292, DRAWER_ICON_SETTINGS, false);

    np_set_visible(header.drawer_scrim, false);

    lv_obj_t *edge = np_fill(parent, 0, NP_HEADER_H, 18,
                             NP_SCREEN_H - NP_HEADER_H,
                             np_c_bg(), LV_OPA_TRANSP, 0);
    lv_obj_add_flag(edge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(edge, drawer_edge_event_cb, LV_EVENT_ALL,
                        header.drawer_scrim);

    lv_obj_add_event_cb(header.menu_button, drawer_toggle_event_cb,
                        LV_EVENT_CLICKED, header.drawer_scrim);
    return header;
}

void np_header_set_drawer_active(np_header_t *header, bool settings_active)
{
    if (header == NULL) return;

    np_set_bg_color(header->drawer_home_button,
                    settings_active ? np_c_surface() : np_c_accent());
    np_set_bg_color(header->drawer_settings_button,
                    settings_active ? np_c_accent() : np_c_surface());
    np_set_text_color(icon_label(header->drawer_home_button),
                      settings_active ? np_c_text_2() : np_c_text());
    np_set_text_color(icon_label(header->drawer_settings_button),
                      settings_active ? np_c_text() : np_c_text_2());
}

void np_header_set_connections(np_header_t *header, bool wifi_online,
                               int8_t wifi_rssi, bool wifi_rssi_measured,
                               bool bluetooth_online, bool has_alert)
{
    if (header == NULL) return;

    const bool has_measured_strength = wifi_online && wifi_rssi_measured;
    np_set_text(icon_label(header->wifi_button),
                wifi_online ? (has_measured_strength ? np_wifi_signal_icon(wifi_rssi) : NP_ICON_WIFI)
                            : NP_ICON_WIFI_OFF);
    np_set_text_color(icon_label(header->wifi_button),
                      wifi_online ? np_c_positive() : np_c_text_3());

    if (header->bluetooth_button != NULL) {
        np_set_text_color(icon_label(header->bluetooth_button),
                          bluetooth_online ? np_c_positive() : np_c_text_disabled());
    }

    np_set_visible(header->alert_dot, has_alert);
}

void np_header_set_notifications_enabled(np_header_t *header, bool enabled)
{
    if (header == NULL || header->notifications_button == NULL) return;
    np_set_text_color(icon_label(header->notifications_button),
                      enabled ? np_c_text_2() : np_c_text_3());
}

/* ---------------- componentes existentes ---------------- */

np_row_t np_row(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    np_row_t row;
    row.root = bare_box(parent, x, y, w, h);
    lv_obj_add_style(row.root, np_st_row(), 0);

    row.accent = np_fill(row.root, 0, 0, 3, h, np_c_accent(), LV_OPA_COVER, 0);
    row.lead = np_label(row.root, "", NP_FONT_LG, np_c_accent(),
                        NP_SP_16, NP_SP_12, 96, LV_TEXT_ALIGN_LEFT);
    row.title = np_label(row.root, "", NP_FONT_MD, np_c_text(),
                         128, NP_SP_12, w - 128 - NP_SP_16, LV_TEXT_ALIGN_LEFT);
    row.meta = np_label(row.root, "", NP_FONT_SM, np_c_text_3(),
                        128, h - NP_SP_24 - NP_SP_4,
                        w - 128 - NP_SP_16, LV_TEXT_ALIGN_LEFT);
    return row;
}

void np_row_set(np_row_t *row, const char *lead, const char *title, const char *meta,
                lv_color_t accent_color, bool visible)
{
    if (row == NULL) return;
    np_set_visible(row->root, visible);
    if (!visible) return;

    np_set_text(row->lead, lead);
    np_set_text_color(row->lead, accent_color);
    np_set_text(row->title, title);
    np_set_text(row->meta, meta);
    np_set_bg_color(row->accent, accent_color);
}

np_tile_t np_tile(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h,
                  const char *icon, const char *title, const char *state)
{
    np_tile_t tile;
    tile.root = bare_box(parent, x, y, w, h);
    lv_obj_add_style(tile.root, np_st_tile(), 0);

    tile.icon = np_label(tile.root, icon, NP_FONT_TITLE, np_c_text_2(),
                         0, 0, w - 2 * NP_SP_16, LV_TEXT_ALIGN_LEFT);
    tile.title = np_label(tile.root, title, NP_FONT_MD, np_c_text(),
                          0, h - 2 * NP_SP_16 - 44,
                          w - 2 * NP_SP_16, LV_TEXT_ALIGN_LEFT);
    tile.state = np_label(tile.root, state, NP_FONT_SM, np_c_text_3(),
                          0, h - 2 * NP_SP_16 - 20,
                          w - 2 * NP_SP_16, LV_TEXT_ALIGN_LEFT);
    return tile;
}

void np_tile_set_on(np_tile_t *tile, bool on, const char *state_text)
{
    if (tile == NULL) return;

    lv_obj_remove_style(tile->root, on ? np_st_tile() : np_st_tile_on(), 0);
    lv_obj_add_style(tile->root, on ? np_st_tile_on() : np_st_tile(), 0);
    np_set_text_color(tile->icon, on ? np_c_accent() : np_c_text_2());
    np_set_text(tile->state, state_text);
    np_set_text_color(tile->state, on ? np_c_accent() : np_c_text_3());
}

#define NP_SPARK_GRADIENT_STRENGTH_PERCENT 26U
/* Mantem toda a linha na faixa superior do chart. O restante da altura fica
 * reservado para o preco/variacao, enquanto o degradê continua descendo ate
 * a divisoria inferior do card. */
#define NP_SPARK_LINE_ZONE_PERCENT         30U

#if LV_USE_CHART && LV_DRAW_SW_COMPLEX && NP_HAVE_LVGL_PRIVATE_DRAW
static lv_opa_t spark_gradient_opa_for_y(int32_t y, const lv_area_t *coords)
{
    const int32_t full_h = coords->y2 - coords->y1 + 1;
    if (full_h <= 1) return 0U;

    int32_t fract = (y - coords->y1) * 255 / full_h;
    if (fract < 0) fract = 0;
    if (fract > 255) fract = 255;

    /* Smoothstep mantém o início e o fim do fade contínuos, evitando uma
     * queda visual abrupta no RGB565. A geometria ainda nasce da curva real
     * do chart; apenas a opacidade vertical fica mais gradual. */
    const uint32_t remaining = (uint32_t)(255 - fract);
    const uint32_t eased_opa =
        (remaining * remaining * (765U - 2U * remaining) + 32512U) / 65025U;
    return (lv_opa_t)((eased_opa * NP_SPARK_GRADIENT_STRENGTH_PERCENT) / 100U);
}

static void spark_add_faded_area(lv_event_t *event)
{
    lv_obj_t *const chart = lv_event_get_target_obj(event);
    if (chart == NULL) return;

    lv_draw_task_t *const draw_task = lv_event_get_draw_task(event);
    if (draw_task == NULL || lv_draw_task_get_type(draw_task) != LV_DRAW_TASK_TYPE_LINE) {
        return;
    }

    lv_draw_dsc_base_t *const base_dsc =
        (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    lv_draw_line_dsc_t *const line_dsc = lv_draw_task_get_line_dsc(draw_task);
    if (base_dsc == NULL || base_dsc->layer == NULL || line_dsc == NULL ||
        line_dsc->points == NULL || line_dsc->point_cnt < 2) {
        return;
    }

    lv_chart_series_t *const series = lv_chart_get_series_next(chart, NULL);
    if (series == NULL) return;

    const lv_color_t color = lv_chart_get_series_color(chart, series);

    lv_area_t coords;
    lv_obj_get_coords(chart, &coords);

    for (int32_t i = 0; i < line_dsc->point_cnt - 1; ++i) {
        const lv_point_precise_t p1 = line_dsc->points[i];
        const lv_point_precise_t p2 = line_dsc->points[i + 1];

        if (p1.x == LV_DRAW_LINE_POINT_NONE || p1.y == LV_DRAW_LINE_POINT_NONE ||
            p2.x == LV_DRAW_LINE_POINT_NONE || p2.y == LV_DRAW_LINE_POINT_NONE) {
            continue;
        }

        /* Fecha geometricamente o espaço imediatamente abaixo do segmento. */
        lv_draw_triangle_dsc_t tri_dsc;
        lv_draw_triangle_dsc_init(&tri_dsc);
        tri_dsc.p[0] = p1;
        tri_dsc.p[1] = p2;
        tri_dsc.p[2].x = p1.y < p2.y ? p1.x : p2.x;
        tri_dsc.p[2].y = LV_MAX(p1.y, p2.y);
        tri_dsc.grad.dir = LV_GRAD_DIR_VER;
        tri_dsc.grad.stops[0].color = color;
        tri_dsc.grad.stops[0].opa =
            spark_gradient_opa_for_y(LV_MIN(p1.y, p2.y), &coords);
        tri_dsc.grad.stops[0].frac = 0U;
        tri_dsc.grad.stops[1].color = color;
        tri_dsc.grad.stops[1].opa =
            spark_gradient_opa_for_y(LV_MAX(p1.y, p2.y), &coords);
        tri_dsc.grad.stops[1].frac = 255U;
        lv_draw_triangle(base_dsc->layer, &tri_dsc);

        /* O retângulo completa o fade até o fundo do chart/divisória. */
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
        rect_dsc.bg_grad.stops[0].color = color;
        rect_dsc.bg_grad.stops[0].frac = 0U;
        rect_dsc.bg_grad.stops[0].opa =
            spark_gradient_opa_for_y(LV_MAX(p1.y, p2.y), &coords);
        rect_dsc.bg_grad.stops[1].color = color;
        rect_dsc.bg_grad.stops[1].frac = 255U;
        rect_dsc.bg_grad.stops[1].opa = 0U;

        lv_area_t rect_area = {
            .x1 = (int32_t)p1.x,
            .x2 = (int32_t)p2.x - 1,
            .y1 = (int32_t)LV_MAX(p1.y, p2.y),
            .y2 = coords.y2,
        };
        if (rect_area.x2 >= rect_area.x1 && rect_area.y2 >= rect_area.y1) {
            lv_draw_rect(base_dsc->layer, &rect_dsc, &rect_area);
        }
    }
}

static void spark_draw_event_cb(lv_event_t *event)
{
    lv_draw_task_t *const draw_task = lv_event_get_draw_task(event);
    if (draw_task == NULL || lv_draw_task_get_type(draw_task) != LV_DRAW_TASK_TYPE_LINE) {
        return;
    }

    lv_draw_dsc_base_t *const base_dsc =
        (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc != NULL && base_dsc->part == LV_PART_ITEMS) {
        spark_add_faded_area(event);
    }
}
#endif

np_spark_t np_spark(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    np_spark_t spark = {0};

    spark.chart = lv_chart_create(parent);
    lv_obj_remove_style_all(spark.chart);
    lv_obj_set_pos(spark.chart, x, y);
    lv_obj_set_size(spark.chart, w, h);
    lv_obj_set_style_pad_all(spark.chart, 0, 0);
    lv_obj_set_style_bg_opa(spark.chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(spark.chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(spark.chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(spark.chart, true, LV_PART_ITEMS);

    /* Sem marcadores nos pontos: somente a linha e o degradê ficam visíveis. */
    lv_obj_set_style_size(spark.chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(spark.chart, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_clear_flag(spark.chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(spark.chart, LV_OBJ_FLAG_SCROLLABLE);

    lv_chart_set_type(spark.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(spark.chart, 0, 0);
    lv_chart_set_point_count(spark.chart, 2U);
    spark.series = lv_chart_add_series(spark.chart, np_c_positive(),
                                       LV_CHART_AXIS_PRIMARY_Y);

#if LV_USE_CHART && LV_DRAW_SW_COMPLEX && NP_HAVE_LVGL_PRIVATE_DRAW
    /* O gradiente é inserido diretamente no draw task da linha do chart. */
    lv_obj_add_event_cb(spark.chart, spark_draw_event_cb,
                        LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(spark.chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
#endif

    lv_obj_add_flag(spark.chart, LV_OBJ_FLAG_HIDDEN);
    return spark;
}

void np_spark_set(np_spark_t *spark, const int32_t *samples, uint8_t count,
                  int32_t w, int32_t h, lv_color_t color)
{
    if (spark == NULL || spark->chart == NULL || spark->series == NULL) return;

    if (samples == NULL || count < 2U || w < 48 || h < 80) {
        np_set_visible(spark->chart, false);
        return;
    }

    if (count > NP_SPARK_MAX_POINTS) count = NP_SPARK_MAX_POINTS;

    int32_t lo = samples[0];
    int32_t hi = samples[0];
    for (uint8_t i = 1U; i < count; ++i) {
        if (samples[i] < lo) lo = samples[i];
        if (samples[i] > hi) hi = samples[i];
    }

    /*
     * Reserva explicitamente a parte inferior do chart para as labels do BTC.
     * Em vez de apenas adicionar um padding arbitrario abaixo da serie,
     * calculamos o range Y para que ate o MENOR valor permaneça dentro dos
     * primeiros NP_SPARK_LINE_ZONE_PERCENT da altura. Assim a linha não chega
     * por trás do preço, mas o degradê ainda pode ocupar toda a
     * altura ate a divisoria.
     */
    const int64_t raw_span = (int64_t)hi - (int64_t)lo;
    int64_t upper_pad;
    if (raw_span > 0) {
        upper_pad = raw_span / 8LL + 1LL; /* pequeno respiro acima do pico */
    } else {
        const int64_t magnitude =
            samples[0] >= 0 ? (int64_t)samples[0] : -(int64_t)samples[0];
        upper_pad = magnitude / 1000LL + 1LL;
    }

    int64_t range_max = (int64_t)hi + upper_pad;
    if (range_max > INT32_MAX) range_max = INT32_MAX;

    const int64_t plot_h = (int64_t)h - 1LL;
    int64_t line_bottom_y =
        (plot_h * (int64_t)NP_SPARK_LINE_ZONE_PERCENT) / 100LL;
    if (line_bottom_y < 1LL) line_bottom_y = 1LL;
    if (line_bottom_y > plot_h) line_bottom_y = plot_h;

    const int64_t distance_to_low = range_max - (int64_t)lo;
    int64_t required_range;
    if (raw_span > 0) {
        /* ceil(distance_to_low * plot_h / line_bottom_y) */
        required_range =
            (distance_to_low * plot_h + line_bottom_y - 1LL) / line_bottom_y;
    } else {
        /* Serie plana: posiciona a linha aproximadamente no meio da faixa
         * segura, evitando que fique colada no limite inferior dela. */
        int64_t flat_y = line_bottom_y / 2LL;
        if (flat_y < 1LL) flat_y = 1LL;
        required_range =
            (distance_to_low * plot_h + flat_y - 1LL) / flat_y;
    }
    if (required_range < 1LL) required_range = 1LL;

    int64_t range_min = range_max - required_range;
    if (range_min < INT32_MIN) range_min = INT32_MIN;
    if (range_max <= range_min) range_max = range_min + 1LL;

    lv_obj_set_size(spark->chart, w, h);
    lv_chart_set_point_count(spark->chart, count);
    lv_chart_set_axis_range(spark->chart, LV_CHART_AXIS_PRIMARY_Y,
                            (int32_t)range_min, (int32_t)range_max);
    lv_chart_set_series_color(spark->chart, spark->series, color);

    /* Recarrega toda a pequena série (máx. 24 pontos) de forma determinística. */
    lv_chart_set_all_values(spark->chart, spark->series, LV_CHART_POINT_NONE);
    for (uint8_t i = 0U; i < count; ++i) {
        lv_chart_set_next_value(spark->chart, spark->series, samples[i]);
    }

    lv_chart_refresh(spark->chart);
    np_set_visible(spark->chart, true);
}

np_segbar_t np_segbar(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, uint8_t count)
{
    np_segbar_t bar;
    memset(&bar, 0, sizeof(bar));
    if (count > 6) count = 6;
    bar.count = count;

    const int32_t gap = NP_SP_8;
    const int32_t seg_w = (w - gap * (count - 1)) / count;
    for (uint8_t i = 0; i < count; ++i) {
        bar.seg[i] = np_fill(parent, x + i * (seg_w + gap), y, seg_w, 4,
                             np_c_hairline(), LV_OPA_COVER, 2);
    }
    return bar;
}

void np_segbar_set(np_segbar_t *bar, uint8_t done, lv_color_t color)
{
    if (bar == NULL) return;
    for (uint8_t i = 0; i < bar->count; ++i) {
        np_set_bg_color(bar->seg[i], i < done ? color : np_c_hairline());
    }
}

lv_obj_t *np_dots(lv_obj_t *parent, int32_t y, uint8_t count, uint8_t active)
{
    const int32_t dot = 6;
    const int32_t wide = 20;
    const int32_t gap = NP_SP_8;
    int32_t total = 0;

    for (uint8_t i = 0; i < count; ++i) {
        total += (i == active ? wide : dot) + (i + 1 < count ? gap : 0);
    }

    lv_obj_t *root = np_group(parent, (NP_SCREEN_W - total) / 2, y, total, dot);
    int32_t cursor = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const int32_t w = i == active ? wide : dot;
        np_fill(root, cursor, 0, w, dot,
                i == active ? np_c_accent() : np_c_hairline(),
                LV_OPA_COVER, dot / 2);
        cursor += w + gap;
    }
    return root;
}
