#include "np_weather_background_assets.h"

#include <stddef.h>
#include <stdint.h>

#if NP2_WEATHER_BACKGROUNDS_EMBEDDED

#define NP_WEATHER_BACKGROUND(name) \
    extern const uint8_t name##_bin_start[] asm("_binary_" #name "_bin_start"); \
    const lv_image_dsc_t name = { \
        .header = { \
            .magic = LV_IMAGE_HEADER_MAGIC, \
            .cf = LV_COLOR_FORMAT_RGB565, \
            .flags = 0, \
            .w = NP_WEATHER_BACKGROUND_WIDTH, \
            .h = NP_WEATHER_BACKGROUND_HEIGHT, \
            .stride = NP_WEATHER_BACKGROUND_WIDTH * 2, \
            .reserved_2 = 0, \
        }, \
        .data_size = NP_WEATHER_BACKGROUND_WIDTH * NP_WEATHER_BACKGROUND_HEIGHT * 2U, \
        .data = name##_bin_start, \
        .reserved = NULL, \
        .reserved_2 = NULL, \
    }

NP_WEATHER_BACKGROUND(np_bg_day_clear);
NP_WEATHER_BACKGROUND(np_bg_day_partly_cloudy);
NP_WEATHER_BACKGROUND(np_bg_day_fog);
NP_WEATHER_BACKGROUND(np_bg_day_drizzle);
NP_WEATHER_BACKGROUND(np_bg_day_rain);
NP_WEATHER_BACKGROUND(np_bg_day_snow);
NP_WEATHER_BACKGROUND(np_bg_day_rain_showers);
NP_WEATHER_BACKGROUND(np_bg_day_snow_showers);
NP_WEATHER_BACKGROUND(np_bg_day_thunderstorm);
NP_WEATHER_BACKGROUND(np_bg_day_variable);
NP_WEATHER_BACKGROUND(np_bg_night_clear);
NP_WEATHER_BACKGROUND(np_bg_night_partly_cloudy);
NP_WEATHER_BACKGROUND(np_bg_night_fog);
NP_WEATHER_BACKGROUND(np_bg_night_drizzle);
NP_WEATHER_BACKGROUND(np_bg_night_rain);
NP_WEATHER_BACKGROUND(np_bg_night_snow);
NP_WEATHER_BACKGROUND(np_bg_night_rain_showers);
NP_WEATHER_BACKGROUND(np_bg_night_snow_showers);
NP_WEATHER_BACKGROUND(np_bg_night_thunderstorm);
NP_WEATHER_BACKGROUND(np_bg_night_variable);

#define NP_WEATHER_SOURCE(period, suffix) (&np_bg_##period##_##suffix)

const void *np_weather_background_asset_source(bool is_day, weather_condition_t condition)
{
    switch (condition) {
        case WEATHER_CONDITION_CLEAR:
            return is_day ? NP_WEATHER_SOURCE(day, clear) : NP_WEATHER_SOURCE(night, clear);
        case WEATHER_CONDITION_PARTLY_CLOUDY:
            return is_day ? NP_WEATHER_SOURCE(day, partly_cloudy) : NP_WEATHER_SOURCE(night, partly_cloudy);
        case WEATHER_CONDITION_FOG:
            return is_day ? NP_WEATHER_SOURCE(day, fog) : NP_WEATHER_SOURCE(night, fog);
        case WEATHER_CONDITION_DRIZZLE:
            return is_day ? NP_WEATHER_SOURCE(day, drizzle) : NP_WEATHER_SOURCE(night, drizzle);
        case WEATHER_CONDITION_RAIN:
            return is_day ? NP_WEATHER_SOURCE(day, rain) : NP_WEATHER_SOURCE(night, rain);
        case WEATHER_CONDITION_SNOW:
            return is_day ? NP_WEATHER_SOURCE(day, snow) : NP_WEATHER_SOURCE(night, snow);
        case WEATHER_CONDITION_RAIN_SHOWERS:
            return is_day ? NP_WEATHER_SOURCE(day, rain_showers) : NP_WEATHER_SOURCE(night, rain_showers);
        case WEATHER_CONDITION_SNOW_SHOWERS:
            return is_day ? NP_WEATHER_SOURCE(day, snow_showers) : NP_WEATHER_SOURCE(night, snow_showers);
        case WEATHER_CONDITION_THUNDERSTORM:
            return is_day ? NP_WEATHER_SOURCE(day, thunderstorm) : NP_WEATHER_SOURCE(night, thunderstorm);
        case WEATHER_CONDITION_VARIABLE:
        default:
            return is_day ? NP_WEATHER_SOURCE(day, variable) : NP_WEATHER_SOURCE(night, variable);
    }
}

#else

const void *np_weather_background_asset_source(bool is_day, weather_condition_t condition)
{
    (void)is_day;
    (void)condition;
    return NULL;
}

#endif /* NP2_WEATHER_BACKGROUNDS_EMBEDDED */

const char *np_weather_background_asset_file_name(bool is_day, weather_condition_t condition)
{
    static const char *const day_files[WEATHER_CONDITION_VARIABLE + 1] = {
        [WEATHER_CONDITION_CLEAR] = "np_bg_day_clear.bin",
        [WEATHER_CONDITION_PARTLY_CLOUDY] = "np_bg_day_partly_cloudy.bin",
        [WEATHER_CONDITION_FOG] = "np_bg_day_fog.bin",
        [WEATHER_CONDITION_DRIZZLE] = "np_bg_day_drizzle.bin",
        [WEATHER_CONDITION_RAIN] = "np_bg_day_rain.bin",
        [WEATHER_CONDITION_SNOW] = "np_bg_day_snow.bin",
        [WEATHER_CONDITION_RAIN_SHOWERS] = "np_bg_day_rain_showers.bin",
        [WEATHER_CONDITION_SNOW_SHOWERS] = "np_bg_day_snow_showers.bin",
        [WEATHER_CONDITION_THUNDERSTORM] = "np_bg_day_thunderstorm.bin",
        [WEATHER_CONDITION_VARIABLE] = "np_bg_day_variable.bin",
    };
    static const char *const night_files[WEATHER_CONDITION_VARIABLE + 1] = {
        [WEATHER_CONDITION_CLEAR] = "np_bg_night_clear.bin",
        [WEATHER_CONDITION_PARTLY_CLOUDY] = "np_bg_night_partly_cloudy.bin",
        [WEATHER_CONDITION_FOG] = "np_bg_night_fog.bin",
        [WEATHER_CONDITION_DRIZZLE] = "np_bg_night_drizzle.bin",
        [WEATHER_CONDITION_RAIN] = "np_bg_night_rain.bin",
        [WEATHER_CONDITION_SNOW] = "np_bg_night_snow.bin",
        [WEATHER_CONDITION_RAIN_SHOWERS] = "np_bg_night_rain_showers.bin",
        [WEATHER_CONDITION_SNOW_SHOWERS] = "np_bg_night_snow_showers.bin",
        [WEATHER_CONDITION_THUNDERSTORM] = "np_bg_night_thunderstorm.bin",
        [WEATHER_CONDITION_VARIABLE] = "np_bg_night_variable.bin",
    };

    if (condition > WEATHER_CONDITION_VARIABLE) {
        condition = WEATHER_CONDITION_VARIABLE;
    }
    return (is_day ? day_files : night_files)[condition];
}
