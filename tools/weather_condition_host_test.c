#include <assert.h>
#include <string.h>

#include "weather_condition.h"

int main(void)
{
    assert(weather_condition_from_code(0U) == WEATHER_CONDITION_CLEAR);
    assert(weather_condition_from_code(3U) == WEATHER_CONDITION_PARTLY_CLOUDY);
    assert(weather_condition_from_code(45U) == WEATHER_CONDITION_FOG);
    assert(weather_condition_from_code(48U) == WEATHER_CONDITION_FOG);
    assert(weather_condition_from_code(51U) == WEATHER_CONDITION_DRIZZLE);
    assert(weather_condition_from_code(57U) == WEATHER_CONDITION_DRIZZLE);
    assert(weather_condition_from_code(61U) == WEATHER_CONDITION_RAIN);
    assert(weather_condition_from_code(67U) == WEATHER_CONDITION_RAIN);
    assert(weather_condition_from_code(71U) == WEATHER_CONDITION_SNOW);
    assert(weather_condition_from_code(77U) == WEATHER_CONDITION_SNOW);
    assert(weather_condition_from_code(80U) == WEATHER_CONDITION_RAIN_SHOWERS);
    assert(weather_condition_from_code(82U) == WEATHER_CONDITION_RAIN_SHOWERS);
    assert(weather_condition_from_code(85U) == WEATHER_CONDITION_SNOW_SHOWERS);
    assert(weather_condition_from_code(86U) == WEATHER_CONDITION_SNOW_SHOWERS);
    assert(weather_condition_from_code(95U) == WEATHER_CONDITION_THUNDERSTORM);
    assert(weather_condition_from_code(4U) == WEATHER_CONDITION_VARIABLE);
    assert(weather_condition_is_day(6U));
    assert(weather_condition_is_day(17U));
    assert(!weather_condition_is_day(18U));
    assert(!weather_condition_is_day(5U));
    assert(strcmp(weather_condition_summary(WEATHER_CONDITION_THUNDERSTORM), "Trovoadas") == 0);
    return 0;
}
