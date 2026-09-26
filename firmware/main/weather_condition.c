#include "weather_condition.h"

weather_condition_t weather_condition_from_code(uint16_t weather_code)
{
    if (weather_code == 0U) return WEATHER_CONDITION_CLEAR;
    if (weather_code <= 3U) return WEATHER_CONDITION_PARTLY_CLOUDY;
    if (weather_code == 45U || weather_code == 48U) return WEATHER_CONDITION_FOG;
    if (weather_code >= 51U && weather_code <= 57U) return WEATHER_CONDITION_DRIZZLE;
    if (weather_code >= 61U && weather_code <= 67U) return WEATHER_CONDITION_RAIN;
    if (weather_code >= 71U && weather_code <= 77U) return WEATHER_CONDITION_SNOW;
    if (weather_code >= 80U && weather_code <= 82U) return WEATHER_CONDITION_RAIN_SHOWERS;
    if (weather_code >= 85U && weather_code <= 86U) return WEATHER_CONDITION_SNOW_SHOWERS;
    if (weather_code >= 95U) return WEATHER_CONDITION_THUNDERSTORM;
    return WEATHER_CONDITION_VARIABLE;
}

const char *weather_condition_summary(weather_condition_t condition)
{
    switch (condition) {
        case WEATHER_CONDITION_CLEAR: return "Céu limpo";
        case WEATHER_CONDITION_PARTLY_CLOUDY: return "Parcialmente nublado";
        case WEATHER_CONDITION_FOG: return "Neblina";
        case WEATHER_CONDITION_DRIZZLE: return "Garoa";
        case WEATHER_CONDITION_RAIN: return "Chuva";
        case WEATHER_CONDITION_SNOW: return "Neve";
        case WEATHER_CONDITION_RAIN_SHOWERS: return "Pancadas de chuva";
        case WEATHER_CONDITION_SNOW_SHOWERS: return "Pancadas de neve";
        case WEATHER_CONDITION_THUNDERSTORM: return "Trovoadas";
        case WEATHER_CONDITION_VARIABLE: return "Condição variável";
        default: return "Condição variável";
    }
}

bool weather_condition_is_day(uint8_t hour)
{
    return hour >= 6U && hour < 18U;
}
