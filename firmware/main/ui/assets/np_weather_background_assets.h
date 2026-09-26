#ifndef NP_WEATHER_BACKGROUND_ASSETS_H
#define NP_WEATHER_BACKGROUND_ASSETS_H

#include <stdbool.h>

#include "weather_condition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Home V2: assets pregerados, sem resize em runtime. */
#define NP_WEATHER_BACKGROUND_WIDTH  560
#define NP_WEATHER_BACKGROUND_HEIGHT 376

const void *np_weather_background_asset_source(bool is_day, weather_condition_t condition);

const char *np_weather_background_asset_file_name(bool is_day,
                                                   weather_condition_t condition);

#ifdef __cplusplus
}
#endif

#endif /* NP_WEATHER_BACKGROUND_ASSETS_H */
