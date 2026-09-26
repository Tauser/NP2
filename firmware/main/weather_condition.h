/* Weather-code classification shared by UI text and visual assets. */
#ifndef WEATHER_CONDITION_H
#define WEATHER_CONDITION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WEATHER_CONDITION_CLEAR = 0,
    WEATHER_CONDITION_PARTLY_CLOUDY,
    WEATHER_CONDITION_FOG,
    WEATHER_CONDITION_DRIZZLE,
    WEATHER_CONDITION_RAIN,
    WEATHER_CONDITION_SNOW,
    WEATHER_CONDITION_RAIN_SHOWERS,
    WEATHER_CONDITION_SNOW_SHOWERS,
    WEATHER_CONDITION_THUNDERSTORM,
    WEATHER_CONDITION_VARIABLE,
} weather_condition_t;

weather_condition_t weather_condition_from_code(uint16_t weather_code);
const char *weather_condition_summary(weather_condition_t condition);

/* Temporary local-time policy. Keeping this outside the Home lets the SD
 * loader and the UI use exactly the same day/night boundary until the weather
 * contract gains sunrise and sunset. */
bool weather_condition_is_day(uint8_t hour);

#ifdef __cplusplus
}
#endif

#endif /* WEATHER_CONDITION_H */
