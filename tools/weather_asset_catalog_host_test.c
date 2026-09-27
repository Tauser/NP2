#include <assert.h>
#include <string.h>

#include "np_weather_background_assets.h"

int main(void)
{
    assert(NP_WEATHER_BACKGROUND_WIDTH == 500);
    assert(NP_WEATHER_BACKGROUND_HEIGHT == 462);
    assert(strcmp(np_weather_background_asset_file_name(true, WEATHER_CONDITION_CLEAR),
                  "np_bg_day_clear.bin") == 0);
    assert(strcmp(np_weather_background_asset_file_name(false, WEATHER_CONDITION_RAIN_SHOWERS),
                  "np_bg_night_rain_showers.bin") == 0);
    assert(strcmp(np_weather_background_asset_file_name(true, WEATHER_CONDITION_VARIABLE),
                  "np_bg_day_variable.bin") == 0);
    assert(strcmp(np_weather_background_asset_file_name(false, (weather_condition_t)99),
                  "np_bg_night_variable.bin") == 0);
    return 0;
}
