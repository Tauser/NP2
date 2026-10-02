#include "np_weather.h"

#include <stdio.h>
#include <time.h>

#include "np_styles.h"
#include "weather_condition.h"
#include "../assets/np_weather_icon_assets.h"
#include "../assets/np_weather_static_icons.h"

#define WEATHER_LEFT_X 18
#define WEATHER_LEFT_W 292
#define WEATHER_CENTER_X 324
#define WEATHER_CENTER_W 382
#define WEATHER_RIGHT_X 720
#define WEATHER_RIGHT_W 286
#define WEATHER_TOP_Y 82
#define WEATHER_CARD_H 478
#define WEATHER_PADDING 12
#define WEATHER_ICON_SIZE 130

static bool weather_local_time(uint32_t unix_s, int32_t utc_offset_seconds, struct tm *out)
{
    if (unix_s == 0U || out == NULL) return false;
    const time_t shifted = (time_t)unix_s + (time_t)utc_offset_seconds;
    return gmtime_r(&shifted, out) != NULL;
}

static void format_temperature(int16_t deci_c, char *out, size_t out_size)
{
    const int16_t whole = deci_c >= 0 ? (int16_t)((deci_c + 5) / 10)
                                      : (int16_t)((deci_c - 5) / 10);
    (void)snprintf(out, out_size, "%d°", (int)whole);
}

static const char *uv_label(uint16_t deci)
{
    if (deci < 30U) return "Baixo";
    if (deci < 60U) return "Moderado";
    if (deci < 80U) return "Alto";
    if (deci < 110U) return "Muito alto";
    return "Extremo";
}

static lv_obj_t *weather_card(lv_obj_t *parent, int32_t x, int32_t width)
{
    lv_obj_t *card = np_surface(parent, x, WEATHER_TOP_Y, width, WEATHER_CARD_H);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    return card;
}

static lv_obj_t *weather_image(lv_obj_t *parent, int32_t x, int32_t y, uint8_t size,
                               const lv_image_dsc_t *source)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_obj_remove_style_all(image);
    lv_obj_set_pos(image, x, y);
    lv_obj_set_size(image, size, size);
    lv_obj_clear_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(image, source);
    return image;
}

np_weather_view_t np_weather_build_with_header(lv_obj_t *parent, const np_header_t *header)
{
    np_weather_view_t view = {0};
    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    lv_obj_t *left = weather_card(view.root, WEATHER_LEFT_X, WEATHER_LEFT_W);
    (void)np_location_icon(left, 24, WEATHER_PADDING + 2, 26, np_c_text_2());
    np_label(left, "Brasília, DF", NP_FONT_LG, np_c_text(), 62, WEATHER_PADDING,
             WEATHER_LEFT_W - 92, LV_TEXT_ALIGN_LEFT);
    view.status = np_status_dot(left, WEATHER_LEFT_W - WEATHER_PADDING - NP_STATUS_DOT_SIZE,
                                WEATHER_PADDING + 8, NP_DATA_UNAVAILABLE);
    view.condition_icon = lv_animimg_create(left);
    lv_obj_remove_style_all(view.condition_icon);
    lv_obj_set_pos(view.condition_icon, (WEATHER_LEFT_W - WEATHER_ICON_SIZE) / 2, 76);
    lv_obj_set_size(view.condition_icon, WEATHER_ICON_SIZE, WEATHER_ICON_SIZE);
    lv_image_set_inner_align(view.condition_icon, LV_IMAGE_ALIGN_CENTER);
    lv_obj_clear_flag(view.condition_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(view.condition_icon, LV_OBJ_FLAG_HIDDEN);
    view.condition_icon_fallback = weather_image(
        left, (WEATHER_LEFT_W - (int32_t)NP_WEATHER_STATIC_ICON_LARGE) / 2, 118,
        NP_WEATHER_STATIC_ICON_LARGE,
        np_weather_static_condition_icon(WEATHER_CONDITION_VARIABLE, true,
                                         NP_WEATHER_STATIC_ICON_LARGE));
    view.temperature = np_label(left, "--°", NP_FONT_HERO, np_c_text(), 22, 222,
                                WEATHER_LEFT_W - 34, LV_TEXT_ALIGN_LEFT);
    view.summary = np_label(left, "Sem dados de clima", NP_FONT_LG, np_c_text(), 22,
                            330, WEATHER_LEFT_W - 34, LV_TEXT_ALIGN_LEFT);
    view.range = np_label(left, "Máx. --° · Mín. --°", NP_FONT_MD, np_c_text_2(), 22,
                          366, WEATHER_LEFT_W - 34, LV_TEXT_ALIGN_LEFT);
    view.feels_like = np_label(left, "Sensação --°", NP_FONT_MD, np_c_text_2(), 22,
                               394, WEATHER_LEFT_W - 34, LV_TEXT_ALIGN_LEFT);
    np_hline(left, WEATHER_PADDING, 426, WEATHER_LEFT_W - 2 * WEATHER_PADDING);
    (void)weather_image(left, WEATHER_PADDING, 440, NP_WEATHER_STATIC_ICON_SMALL,
                        np_weather_static_info_icon(NP_WEATHER_STATIC_SUNRISE,
                                                    NP_WEATHER_STATIC_ICON_SMALL));
    view.sunrise = np_label(left, "--:--", NP_FONT_MD, np_c_text_2(), 52, 438, 74, LV_TEXT_ALIGN_LEFT);
    np_vline(left, 136, 438, 28);
    (void)weather_image(left, 152, 440, NP_WEATHER_STATIC_ICON_SMALL,
                        np_weather_static_info_icon(NP_WEATHER_STATIC_SUNSET,
                                                    NP_WEATHER_STATIC_ICON_SMALL));
    view.sunset = np_label(left, "--:--", NP_FONT_MD, np_c_text_2(), 186, 438, 78, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *center = weather_card(view.root, WEATHER_CENTER_X, WEATHER_CENTER_W);
    np_label(center, "Próximas horas", NP_FONT_LG, np_c_text_2(), WEATHER_PADDING, WEATHER_PADDING,
             WEATHER_CENTER_W - 2 * WEATHER_PADDING, LV_TEXT_ALIGN_LEFT);
    const int32_t hour_w = (WEATHER_CENTER_W - 2 * WEATHER_PADDING) / OFFLINE_WEATHER_HOURLY_MAX;
    for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
        const int32_t x = WEATHER_PADDING + (int32_t)index * hour_w;
        view.hour_time[index] = np_label(center, "--h", NP_FONT_SM, np_c_text_2(), x, 70,
                                         hour_w, LV_TEXT_ALIGN_CENTER);
        view.hour_icon[index] = weather_image(
            center, x + (hour_w - (int32_t)NP_WEATHER_STATIC_ICON_SMALL) / 2, 104,
            NP_WEATHER_STATIC_ICON_SMALL,
            np_weather_static_condition_icon(WEATHER_CONDITION_VARIABLE, true,
                                             NP_WEATHER_STATIC_ICON_SMALL));
        view.hour_temperature[index] = np_label(center, "--°", NP_FONT_LG, np_c_text(), x, 148,
                                                hour_w, LV_TEXT_ALIGN_CENTER);
        view.hour_precipitation[index] = np_label(center, "--%", NP_FONT_MD, np_c_accent(), x, 190,
                                                  hour_w, LV_TEXT_ALIGN_CENTER);
        if (index + 1U < OFFLINE_WEATHER_HOURLY_MAX)
            np_vline(center, x + hour_w, 66, 150);
    }
    np_hline(center, WEATHER_PADDING, 236, WEATHER_CENTER_W - 2 * WEATHER_PADDING);
    np_label(center, "Próximos dias", NP_FONT_LG, np_c_text_2(), WEATHER_PADDING, 248,
             WEATHER_CENTER_W - 2 * WEATHER_PADDING, LV_TEXT_ALIGN_LEFT);
    for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
        const int32_t y = 300 + (int32_t)index * 42;
        view.day_name[index] = np_label(center, "---", NP_FONT_MD, np_c_text_2(), 24, y,
                                        40, LV_TEXT_ALIGN_LEFT);
        view.day_icon[index] = weather_image(
            center, 68, y - 2, NP_WEATHER_STATIC_ICON_SMALL,
            np_weather_static_condition_icon(WEATHER_CONDITION_VARIABLE, true,
                                             NP_WEATHER_STATIC_ICON_SMALL));
        view.day_summary[index] = np_label(center, "Sem previsão", NP_FONT_SM, np_c_text(), 104, y,
                                           164, LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(view.day_summary[index], LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_height(view.day_summary[index], 42);
        view.day_minimum[index] = np_label(center, "--°", NP_FONT_MD, np_c_text_2(), 266, y,
                                           42, LV_TEXT_ALIGN_RIGHT);
        view.day_maximum[index] = np_label(center, "--°", NP_FONT_MD, np_c_text(), 316, y,
                                           42, LV_TEXT_ALIGN_RIGHT);
        if (index + 1U < OFFLINE_WEATHER_DAILY_MAX)
            np_hline(center, WEATHER_PADDING, y + 39, WEATHER_CENTER_W - 2 * WEATHER_PADDING);
    }

    lv_obj_t *right = weather_card(view.root, WEATHER_RIGHT_X, WEATHER_RIGHT_W);
    /* Four detail rows share the available height evenly; the card has no title. */
    static const char *const captions[] = {"Vento", "Umidade", "Índice UV", "Chuva hoje"};
    static const np_weather_static_info_icon_t icons[] = {
        NP_WEATHER_STATIC_WIND, NP_WEATHER_STATIC_HUMIDITY,
        NP_WEATHER_STATIC_UV_INDEX, NP_WEATHER_STATIC_RAIN,
    };
    lv_obj_t **values[] = {&view.wind, &view.humidity, &view.uv, &view.rain};
    for (size_t index = 0U; index < 4U; ++index) {
        const int32_t y = 42 + (int32_t)index * 86;
        (void)weather_image(right, 18, y + 6, NP_WEATHER_STATIC_ICON_LARGE,
                            np_weather_static_info_icon(icons[index],
                                                        NP_WEATHER_STATIC_ICON_LARGE));
        np_label(right, captions[index], NP_FONT_MD, np_c_text_2(), 76, y, 190, LV_TEXT_ALIGN_LEFT);
        *values[index] = np_label(right, "--", NP_FONT_LG, np_c_text(), 76, y + 30, 190,
                                   LV_TEXT_ALIGN_LEFT);
        if (index < 3U) np_hline(right, WEATHER_PADDING, y + 72, WEATHER_RIGHT_W - 2 * WEATHER_PADDING);
    }
    view.source_status = np_status_dot(right, 20, 405, NP_DATA_UNAVAILABLE);
    view.source = np_label(right, "Open-Meteo · indisponível", NP_FONT_SM, np_c_text_2(), 76, 396,
                           WEATHER_RIGHT_W - 88, LV_TEXT_ALIGN_LEFT);
    view.updated_at = np_label(right, "Atualizado --:--", NP_FONT_MD, np_c_text_2(), 76, 432,
                               WEATHER_RIGHT_W - 88, LV_TEXT_ALIGN_LEFT);
    return view;
}

void np_weather_set_icon_source(np_weather_view_t *view, const void *source)
{
    if (view == NULL || view->condition_icon == NULL) return;
    const np_weather_icon_asset_t *asset = source;
    if (asset == NULL || asset->frames == NULL || asset->frame_count == 0U ||
        asset->frame_count > NP_WEATHER_ICON_MAX_FRAMES || asset->frame_ms != 125U) {
        (void)lv_animimg_delete(view->condition_icon);
        np_set_visible(view->condition_icon, false);
        np_set_visible(view->condition_icon_fallback, true);
        return;
    }
    if (lv_animimg_get_src(view->condition_icon) == asset->frames &&
        !lv_obj_has_flag(view->condition_icon, LV_OBJ_FLAG_HIDDEN)) return;
    (void)lv_animimg_delete(view->condition_icon);
    lv_animimg_set_src(view->condition_icon, asset->frames, asset->frame_count);
    lv_animimg_set_duration(view->condition_icon,
                            (uint32_t)asset->frame_count * asset->frame_ms);
    lv_animimg_set_repeat_count(view->condition_icon, LV_ANIM_REPEAT_INFINITE);
    lv_image_set_src(view->condition_icon, asset->frames[0]);
    np_set_visible(view->condition_icon_fallback, false);
    np_set_visible(view->condition_icon, true);
    lv_animimg_start(view->condition_icon);
}

void np_weather_sync(np_weather_view_t *view, const offline_weather_data_t *weather)
{
    if (view == NULL || weather == NULL) return;
    np_status_dot_set(&view->status, weather->available
        ? (weather->stale ? NP_DATA_STALE : NP_DATA_LIVE) : NP_DATA_UNAVAILABLE);
    np_status_dot_set(&view->source_status, weather->available
        ? (weather->stale ? NP_DATA_STALE : NP_DATA_LIVE) : NP_DATA_UNAVAILABLE);
    if (!weather->available) {
        np_set_text(view->temperature, "--°");
        np_set_text(view->summary, "Sem dados de clima");
        np_set_text(view->range, "Máx. --° · Mín. --°");
        np_set_text(view->feels_like, "Sensação --°");
        np_set_text(view->sunrise, "--:--"); np_set_text(view->sunset, "--:--");
        np_set_text(view->wind, "--"); np_set_text(view->humidity, "--");
        np_set_text(view->uv, "--"); np_set_text(view->rain, "--");
        np_set_text(view->source, "Open-Meteo · indisponível");
        np_set_text(view->updated_at, "Atualizado --:--");
        for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
            lv_image_set_src(view->hour_icon[index], np_weather_static_condition_icon(
                WEATHER_CONDITION_VARIABLE, true, NP_WEATHER_STATIC_ICON_SMALL));
            np_set_text(view->hour_time[index], "--h");
            np_set_text(view->hour_temperature[index], "--°");
            np_set_text(view->hour_precipitation[index], "--%");
        }
        for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
            lv_image_set_src(view->day_icon[index], np_weather_static_condition_icon(
                WEATHER_CONDITION_VARIABLE, true, NP_WEATHER_STATIC_ICON_SMALL));
            np_set_text(view->day_name[index], "---");
            np_set_text(view->day_summary[index], "Sem previsão");
            np_set_text(view->day_minimum[index], "--°");
            np_set_text(view->day_maximum[index], "--°");
        }
        return;
    }

    char value[40] = {0};
    format_temperature(weather->temperature_deci_c, value, sizeof(value));
    np_set_text(view->temperature, value);
    np_set_text(view->summary, weather_condition_summary(weather_condition_from_code(weather->weather_code)));
    if (weather->forecast_available) {
        char max[12] = {0}, min[12] = {0};
        format_temperature(weather->today_temperature_max_deci_c, max, sizeof(max));
        format_temperature(weather->today_temperature_min_deci_c, min, sizeof(min));
        (void)snprintf(value, sizeof(value), "Máx. %s · Mín. %s", max, min);
        np_set_text(view->range, value);
    } else np_set_text(view->range, "Máx. --° · Mín. --°");
    if (weather->apparent_temperature_available) {
        format_temperature(weather->apparent_temperature_deci_c, value, sizeof(value));
        char feels[64] = {0};
        (void)snprintf(feels, sizeof(feels), "Sensação %s", value);
        np_set_text(view->feels_like, feels);
    } else np_set_text(view->feels_like, "Sensação --°");

    struct tm local = {0};
    if (weather->forecast_available && weather_local_time(weather->sunrise_unix_s,
        weather->utc_offset_seconds, &local)) {
        (void)snprintf(value, sizeof(value), "%02d:%02d", local.tm_hour, local.tm_min);
        np_set_text(view->sunrise, value);
    } else np_set_text(view->sunrise, "--:--");
    if (weather->forecast_available && weather_local_time(weather->sunset_unix_s,
        weather->utc_offset_seconds, &local)) {
        (void)snprintf(value, sizeof(value), "%02d:%02d", local.tm_hour, local.tm_min);
        np_set_text(view->sunset, value);
    } else np_set_text(view->sunset, "--:--");

    for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
        if (weather->forecast_available && weather_local_time(weather->hourly[index].time_unix_s,
            weather->utc_offset_seconds, &local)) {
            (void)snprintf(value, sizeof(value), "%02dh", local.tm_hour);
            np_set_text(view->hour_time[index], value);
            format_temperature(weather->hourly[index].temperature_deci_c, value, sizeof(value));
            np_set_text(view->hour_temperature[index], value);
            (void)snprintf(value, sizeof(value), "%u%%",
                           (unsigned)weather->hourly[index].precipitation_probability_percent);
            np_set_text(view->hour_precipitation[index], value);
            lv_image_set_src(view->hour_icon[index], np_weather_static_condition_icon(
                weather_condition_from_code(weather->hourly[index].weather_code),
                weather_condition_is_day((uint8_t)local.tm_hour),
                NP_WEATHER_STATIC_ICON_SMALL));
        } else {
            np_set_text(view->hour_time[index], "--h"); np_set_text(view->hour_temperature[index], "--°");
            np_set_text(view->hour_precipitation[index], "--%");
            lv_image_set_src(view->hour_icon[index], np_weather_static_condition_icon(
                WEATHER_CONDITION_VARIABLE, true, NP_WEATHER_STATIC_ICON_SMALL));
        }
    }
    static const char *const weekdays[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
    for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
        if (weather->forecast_available && weather_local_time(weather->daily[index].time_unix_s,
            weather->utc_offset_seconds, &local)) {
            np_set_text(view->day_name[index], weekdays[local.tm_wday]);
            lv_image_set_src(view->day_icon[index], np_weather_static_condition_icon(
                weather_condition_from_code(weather->daily[index].weather_code), true,
                NP_WEATHER_STATIC_ICON_SMALL));
            np_set_text(view->day_summary[index], weather_condition_summary(
                weather_condition_from_code(weather->daily[index].weather_code)));
            format_temperature(weather->daily[index].temperature_min_deci_c, value, sizeof(value));
            np_set_text(view->day_minimum[index], value);
            format_temperature(weather->daily[index].temperature_max_deci_c, value, sizeof(value));
            np_set_text(view->day_maximum[index], value);
        } else {
            np_set_text(view->day_name[index], "---");
            lv_image_set_src(view->day_icon[index], np_weather_static_condition_icon(
                WEATHER_CONDITION_VARIABLE, true, NP_WEATHER_STATIC_ICON_SMALL));
            np_set_text(view->day_summary[index], "Sem previsão");
            np_set_text(view->day_minimum[index], "--°"); np_set_text(view->day_maximum[index], "--°");
        }
    }
    if (weather->wind_speed_available) {
        const unsigned speed = (unsigned)((weather->wind_speed_deci_kmh + 5U) / 10U);
        static const char *const directions[] = {"N", "NE", "L", "SE", "S", "SO", "O", "NO"};
        const char *direction = weather->wind_direction_available
            ? directions[((weather->wind_direction_degrees + 22U) % 360U) / 45U] : "--";
        (void)snprintf(value, sizeof(value), "%u km/h %s", speed, direction);
        np_set_text(view->wind, value);
    } else np_set_text(view->wind, "--");
    (void)snprintf(value, sizeof(value), "%u%%", (unsigned)weather->relative_humidity_percent);
    np_set_text(view->humidity, value);
    if (weather->uv_index_available) {
        const unsigned whole = (unsigned)(weather->uv_index_deci / 10U);
        (void)snprintf(value, sizeof(value), "%u · %s", whole, uv_label(weather->uv_index_deci));
        np_set_text(view->uv, value);
    } else np_set_text(view->uv, "--");
    if (weather->forecast_available) {
        const unsigned whole = (unsigned)(weather->today_precipitation_sum_deci_mm / 10U);
        const unsigned decimal = (unsigned)(weather->today_precipitation_sum_deci_mm % 10U);
        (void)snprintf(value, sizeof(value), "%u,%u mm", whole, decimal);
        np_set_text(view->rain, value);
    } else np_set_text(view->rain, "--");
    np_set_text(view->source, weather->stale ? "Open-Meteo · cache" : "Open-Meteo · ao vivo");
    if (weather_local_time(weather->observed_at_unix_s, weather->utc_offset_seconds, &local)) {
        (void)snprintf(value, sizeof(value), "Atualizado %02d:%02d", local.tm_hour, local.tm_min);
        np_set_text(view->updated_at, value);
    } else np_set_text(view->updated_at, "Atualizado --:--");
}
