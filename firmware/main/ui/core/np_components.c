#include "np_components.h"
#include "np_styles.h"

#include <string.h>

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
    np_label(badge, NP_ICON_BITCOIN, NP_FONT_ICON_BADGE, np_c_text(),
             0, (size - NP_FONT_ICON_BADGE->line_height) / 2, size,
             LV_TEXT_ALIGN_CENTER);
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
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, NP_RADIUS_CONTROL, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, primary ? np_c_accent() : np_c_surface(), 0);
    lv_obj_set_style_border_width(obj, primary ? 0 : 1, 0);
    lv_obj_set_style_border_color(obj, np_c_hairline(), 0);

    lv_obj_t *text_label = lv_label_create(obj);
    lv_obj_set_style_text_font(text_label, NP_FONT_MD, 0);
    lv_obj_set_style_text_color(text_label,
                                primary ? np_c_text_on_accent() : np_c_text(), 0);
    lv_label_set_text(text_label, text != NULL ? text : "");
    lv_obj_center(text_label);
    return obj;
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
        default:
            symbol = NP_ICON_CALENDAR;
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

    /* Grupo de ações encostado ao bloco exclusivo do relógio. */
    header.wifi_button =
        np_icon_button(parent, 650, 12, NP_TOUCH_TARGET, NP_ICON_WIFI);
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

    (void)drawer_item(header.drawer, 20,  DRAWER_ICON_HOME, true);
    (void)drawer_item(header.drawer, 88,  DRAWER_ICON_WEATHER, false);
    (void)drawer_item(header.drawer, 156, DRAWER_ICON_MARKET, false);
    (void)drawer_item(header.drawer, 224, DRAWER_ICON_CALENDAR, false);

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

void np_header_set_connections(np_header_t *header, bool wifi_online,
                               bool bluetooth_online, bool has_alert)
{
    if (header == NULL) return;

    np_set_text_color(icon_label(header->wifi_button),
                      wifi_online ? np_c_positive() : np_c_text_3());

    if (header->bluetooth_button != NULL) {
        np_set_text_color(icon_label(header->bluetooth_button),
                          bluetooth_online ? np_c_positive() : np_c_text_disabled());
    }

    np_set_visible(header->alert_dot, has_alert);
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

np_spark_t np_spark(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    np_spark_t spark;
    memset(&spark, 0, sizeof(spark));
    spark.line = lv_line_create(parent);
    lv_obj_set_pos(spark.line, x, y);
    lv_obj_set_size(spark.line, w, h);
    lv_obj_set_style_line_width(spark.line, 2, 0);
    lv_obj_set_style_line_rounded(spark.line, true, 0);
    lv_obj_set_style_line_color(spark.line, np_c_text_3(), 0);
    lv_obj_add_flag(spark.line, LV_OBJ_FLAG_HIDDEN);
    return spark;
}

void np_spark_set(np_spark_t *spark, const int32_t *samples, uint8_t count,
                  int32_t w, int32_t h, lv_color_t color)
{
    if (spark == NULL || spark->line == NULL) return;

    if (samples == NULL || count < 2) {
        np_set_visible(spark->line, false);
        return;
    }

    if (count > 24) count = 24;

    int32_t lo = samples[0];
    int32_t hi = samples[0];
    for (uint8_t i = 1; i < count; ++i) {
        if (samples[i] < lo) lo = samples[i];
        if (samples[i] > hi) hi = samples[i];
    }

    const int32_t span = (hi - lo) > 0 ? (hi - lo) : 1;
    for (uint8_t i = 0; i < count; ++i) {
        spark->pts[i].x = (int32_t)i * (w - 1) / (count - 1);
        spark->pts[i].y =
            (h - 1) - ((samples[i] - lo) * (h - 1) / span);
    }

    lv_line_set_points(spark->line, spark->pts, count);
    lv_obj_set_style_line_color(spark->line, color, 0);
    np_set_visible(spark->line, true);
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
