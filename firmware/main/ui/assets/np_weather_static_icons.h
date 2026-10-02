#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "weather_condition.h"

#define NP_WEATHER_STATIC_ICON_SMALL 32U
#define NP_WEATHER_STATIC_ICON_LARGE 48U

typedef enum {
    NP_WEATHER_STATIC_SUNRISE = 0,
    NP_WEATHER_STATIC_SUNSET,
    NP_WEATHER_STATIC_WIND,
    NP_WEATHER_STATIC_HUMIDITY,
    NP_WEATHER_STATIC_UV_INDEX,
    NP_WEATHER_STATIC_RAIN,
} np_weather_static_info_icon_t;

const lv_image_dsc_t *np_weather_static_condition_icon(
    weather_condition_t condition, bool is_day, uint8_t size);
const lv_image_dsc_t *np_weather_static_info_icon(
    np_weather_static_info_icon_t icon, uint8_t size);
