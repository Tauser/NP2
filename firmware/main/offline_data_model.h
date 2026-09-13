/* Product data contract: no ESP-IDF, LVGL, network or storage dependency. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OFFLINE_DATA_SCHEMA_VERSION UINT16_C(1)

typedef enum {
    OFFLINE_DATA_ORIGIN_NONE = 0,
    OFFLINE_DATA_ORIGIN_CACHE,
    OFFLINE_DATA_ORIGIN_LIVE,
} offline_data_origin_t;

typedef struct {
    bool available;
    bool stale;
    int16_t temperature_deci_c;
    uint8_t relative_humidity_percent;
    uint16_t weather_code;
    uint32_t observed_at_unix_s;
} offline_weather_data_t;

typedef struct {
    bool available;
    bool stale;
    uint32_t bitcoin_usd_cents;
    int16_t change_24h_basis_points;
    uint32_t observed_at_unix_s;
} offline_market_data_t;

typedef struct {
    uint16_t schema_version;
    offline_data_origin_t origin;
    offline_weather_data_t weather;
    offline_market_data_t market;
} offline_data_snapshot_t;
