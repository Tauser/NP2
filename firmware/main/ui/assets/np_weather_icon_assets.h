#pragma once

#include <stdint.h>

#define NP_WEATHER_ICON_LEGACY_SIZE 96U
#define NP_WEATHER_ICON_SIZE 160U
#define NP_WEATHER_ICON_MAX_FRAMES 48U

/* Owned by the SD weather worker. The pointer array and all image descriptors
 * remain stable while this asset is selected by the UI. */
typedef struct {
    const void **frames;
    uint16_t frame_count;
    uint16_t frame_ms;
} np_weather_icon_asset_t;
