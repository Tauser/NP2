#include "np_photo_frame.h"
#include <stdio.h>
#include <time.h>

static void pass_touch_to_photo(lv_obj_t *obj)
{
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        pass_touch_to_photo(lv_obj_get_child(obj, i));
}

static lv_obj_t *widget(lv_obj_t *parent, int32_t x, int32_t width)
{
    lv_obj_t *obj = np_fill(parent, x, 456, width, 120, np_c_bg(), 92, 20);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_white(), 0);
    lv_obj_set_style_border_opa(obj, 30, 0);
    return obj;
}

np_photo_frame_view_t np_photo_frame_build(lv_obj_t *parent)
{
    np_photo_frame_view_t view = {0};
    view.root = np_scene(parent);
    lv_obj_add_flag(view.root, LV_OBJ_FLAG_CLICKABLE);
    view.image = lv_image_create(view.root);
    lv_obj_set_pos(view.image, 0, 0);
    view.widgets[0] = widget(view.root, 24, 440);
    view.widgets[1] = widget(view.root, 600, 400);
    lv_obj_t *coin = np_fill(view.widgets[0], 14, 42, 36, 36,
                              lv_color_hex(0xE5B87B), 28, LV_RADIUS_CIRCLE);
    np_label(coin, NP_ICON_BITCOIN, NP_FONT_ICON, lv_color_hex(0xE5B87B),
              0, 6, 36, LV_TEXT_ALIGN_CENTER);
    view.bitcoin_label = np_label(view.widgets[0], "Bitcoin", NP_FONT_SM,
        lv_color_hex(0xCBD2D9), 64, 18, 260, LV_TEXT_ALIGN_LEFT);
    np_label(view.widgets[0], "US$", NP_FONT_SM, lv_color_hex(0xCBD2D9),
              64, 74, 34, LV_TEXT_ALIGN_LEFT);
    view.bitcoin = np_label(view.widgets[0], "--", NP_FONT_BRAND, lv_color_white(),
                             102, 32, 320, LV_TEXT_ALIGN_LEFT);
    view.clock = np_label(view.widgets[1], "--:--", NP_FONT_BRAND,
        lv_color_white(), 14, 20, 216, LV_TEXT_ALIGN_CENTER);
    (void)np_fill(view.widgets[1], 236, 38, 1, 44, lv_color_white(), 46, 0);
    view.weekday = np_label(view.widgets[1], "", NP_FONT_SM,
        lv_color_hex(0xCBD2D9), 252, 40, 132, LV_TEXT_ALIGN_LEFT);
    view.date = np_label(view.widgets[1], "", NP_FONT_SM, lv_color_white(),
        252, 68, 132, LV_TEXT_ALIGN_LEFT);
    pass_touch_to_photo(view.image);
    pass_touch_to_photo(view.widgets[0]);
    pass_touch_to_photo(view.widgets[1]);
    np_set_visible(view.root, false);
    return view;
}

void np_photo_frame_set_source(np_photo_frame_view_t *view, const void *source)
{
    if (view && view->image && lv_image_get_src(view->image) != source)
        lv_image_set_src(view->image, source);
}

void np_photo_frame_sync(np_photo_frame_view_t *view, uint32_t unix_s, bool trusted,
                         const offline_market_data_t *market,
                         uint8_t bitcoin_widget, uint8_t clock_widget)
{
    if (!view || !view->root || !market) return;
    /* Match the clock even for an already-saved profile with a smaller BTC
     * size. Keep position, visibility and background independent. */
    const uint8_t configs[] = {
        (bitcoin_widget & (uint8_t)~PHOTO_WIDGET_LARGE) | (clock_widget & PHOTO_WIDGET_LARGE),
        clock_widget
    };
    for (unsigned i = 0; i < 2; ++i) {
        if (view->configured && view->widget_config[i] == configs[i]) continue;
        view->widget_config[i] = configs[i];
        const uint8_t config = configs[i];
        np_set_visible(view->widgets[i], (config & PHOTO_WIDGET_VISIBLE) != 0);
        np_set_bg_opa(view->widgets[i], photo_widget_opacity(config));
        lv_obj_set_y(view->widgets[i], (config & PHOTO_WIDGET_TOP) ? 24 : 456);
        const bool large = (config & PHOTO_WIDGET_LARGE) != 0;
        if (i == 0) {
            lv_obj_set_width(view->widgets[i], large ? 440 : 360);
            lv_obj_set_width(view->bitcoin, large ? 320 : 240);
            lv_obj_set_style_text_font(view->bitcoin, large ? NP_FONT_BRAND : NP_FONT_DISPLAY, 0);
            lv_obj_set_y(view->bitcoin, large ? 32 : 46);
        } else {
            lv_obj_set_style_text_font(view->clock, large ? NP_FONT_BRAND : NP_FONT_DISPLAY, 0);
            lv_obj_set_y(view->clock, large ? 20 : 30);
        }
    }
    view->configured = true;
    if (market->available) {
        const uint32_t dollars = market->bitcoin_usd_cents / 100U;
        char price[24];
        if (dollars >= 1000000U)
            snprintf(price, sizeof(price), "%lu,%02lu mi", (unsigned long)(dollars/1000000U),
                (unsigned long)((dollars%1000000U)/10000U));
        else if (dollars >= 1000U)
            snprintf(price, sizeof(price), "%lu.%03lu", (unsigned long)(dollars/1000U),
                       (unsigned long)(dollars%1000U));
        else snprintf(price, sizeof(price), "%lu", (unsigned long)dollars);
        np_set_text(view->bitcoin, price);
        np_set_text(view->bitcoin_label, market->stale ? "Bitcoin · salvo" : "Bitcoin");
    } else {
        np_set_text(view->bitcoin, "--");
        np_set_text(view->bitcoin_label, "Bitcoin · sem dados");
    }
    char clock[8] = "--:--", date[24] = "Sem horario";
    const char *weekday = "Aguardando";
    const time_t now = unix_s;
    struct tm local = {0};
    bool local_available = false;
    if (trusted) {
#if defined(_WIN32) && !defined(ESP_PLATFORM)
        local_available = localtime_s(&local, &now) == 0;
#else
        local_available = localtime_r(&now, &local) != NULL;
#endif
    }
    if (local_available) {
        static const char *const weekdays[] = {"Domingo", "Segunda-feira", "Terça-feira",
            "Quarta-feira", "Quinta-feira", "Sexta-feira", "Sábado"};
        static const char *const months[] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho",
            "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
        if (local.tm_wday >= 0 && local.tm_wday < 7 && local.tm_mon >= 0 && local.tm_mon < 12) {
            snprintf(clock, sizeof(clock), "%02u:%02u", (unsigned)local.tm_hour, (unsigned)local.tm_min);
            snprintf(date, sizeof(date), "%d %s", local.tm_mday, months[local.tm_mon]);
            weekday = weekdays[local.tm_wday];
        }
    }
    np_set_text(view->clock, clock);
    np_set_text(view->weekday, weekday);
    np_set_text(view->date, date);
}
