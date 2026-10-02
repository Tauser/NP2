/* Product Clima screen. The layout is fixed; only labels and icon sources are
 * projected into it by product_ui on the LVGL task. */
#pragma once

#include "offline_data_model.h"
#include "np_components.h"

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *condition_icon;
    lv_obj_t *condition_icon_fallback;
    np_status_dot_t status;
    lv_obj_t *temperature;
    lv_obj_t *summary;
    lv_obj_t *range;
    lv_obj_t *feels_like;
    lv_obj_t *sunrise;
    lv_obj_t *sunset;
    lv_obj_t *hour_time[OFFLINE_WEATHER_HOURLY_MAX];
    lv_obj_t *hour_icon[OFFLINE_WEATHER_HOURLY_MAX];
    lv_obj_t *hour_temperature[OFFLINE_WEATHER_HOURLY_MAX];
    lv_obj_t *hour_precipitation[OFFLINE_WEATHER_HOURLY_MAX];
    lv_obj_t *day_name[OFFLINE_WEATHER_DAILY_MAX];
    lv_obj_t *day_icon[OFFLINE_WEATHER_DAILY_MAX];
    lv_obj_t *day_summary[OFFLINE_WEATHER_DAILY_MAX];
    lv_obj_t *day_minimum[OFFLINE_WEATHER_DAILY_MAX];
    lv_obj_t *day_maximum[OFFLINE_WEATHER_DAILY_MAX];
    lv_obj_t *wind;
    lv_obj_t *humidity;
    lv_obj_t *uv;
    lv_obj_t *rain;
    np_status_dot_t source_status;
    lv_obj_t *source;
    lv_obj_t *updated_at;
} np_weather_view_t;

np_weather_view_t np_weather_build_with_header(lv_obj_t *parent, const np_header_t *header);
void np_weather_set_icon_source(np_weather_view_t *view, const void *source);
void np_weather_sync(np_weather_view_t *view, const offline_weather_data_t *weather);
