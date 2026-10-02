/* NP2 offline product data contract. Independent of ESP-IDF, LVGL and I/O.
 * Schema v7 adds S&P/Nasdaq quotes; older cache records migrate with those
 * optional fields unavailable. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define OFFLINE_DATA_SCHEMA_VERSION UINT16_C(7)

#define OFFLINE_MARKET_HISTORY_MAX UINT8_C(24)
#define OFFLINE_MARKET_ALTCOIN_COUNT UINT8_C(4)
#define OFFLINE_WEATHER_HOURLY_MAX UINT8_C(5)
#define OFFLINE_WEATHER_DAILY_MAX UINT8_C(4)

typedef enum {
    OFFLINE_DATA_ORIGIN_NONE = 0,
    OFFLINE_DATA_ORIGIN_CACHE,
    OFFLINE_DATA_ORIGIN_LIVE,
} offline_data_origin_t;

/* Weather ------------------------------------------------------------------ */

typedef struct {
    bool available;
    bool stale;

    /* Already present in schema v3. */
    int16_t temperature_deci_c;
    uint8_t relative_humidity_percent;
    uint16_t weather_code;

    /* Home V2 detail tiles. Each optional datum has an explicit availability
     * bit so the UI never invents or displays a synthetic value. */
    bool apparent_temperature_available;
    int16_t apparent_temperature_deci_c;

    bool wind_speed_available;
    uint16_t wind_speed_deci_kmh;

    bool uv_index_available;
    uint16_t uv_index_deci;

    /* The full screen needs a bound, coherent forecast response. It contains
     * the current-day summary, five upcoming hours and four following days. */
    bool wind_direction_available;
    uint16_t wind_direction_degrees;

    bool forecast_available;
    int32_t utc_offset_seconds;
    int16_t today_temperature_max_deci_c;
    int16_t today_temperature_min_deci_c;
    uint16_t today_precipitation_sum_deci_mm;
    uint32_t sunrise_unix_s;
    uint32_t sunset_unix_s;

    struct {
        uint32_t time_unix_s;
        int16_t temperature_deci_c;
        uint16_t weather_code;
        uint8_t precipitation_probability_percent;
    } hourly[OFFLINE_WEATHER_HOURLY_MAX];

    struct {
        uint32_t time_unix_s;
        int16_t temperature_min_deci_c;
        int16_t temperature_max_deci_c;
        uint16_t weather_code;
    } daily[OFFLINE_WEATHER_DAILY_MAX];

    uint32_t observed_at_unix_s;
} offline_weather_data_t;

/* Persisted visual selection, not a bitmap. */
typedef struct {
    bool available;
    bool is_day;
} offline_weather_visual_t;

/* Bitcoin ------------------------------------------------------------------ */

typedef struct {
    bool available;
    bool stale;

    /* Already present in schema v3. */
    uint32_t bitcoin_usd_cents;
    int16_t change_24h_basis_points;

    /* Optional 24 h summary shown at the bottom of the BTC card. */
    bool high_24h_available;
    uint32_t high_24h_usd_cents;

    bool low_24h_available;
    uint32_t low_24h_usd_cents;

    bool volume_24h_available;
    uint64_t volume_24h_usd_cents;

    /* Real samples only. The Home hides the sparkline when count < 2.
     * The source may be API history or samples accumulated by the product,
     * but the UI must never synthesize them. */
    uint8_t history_count;
    uint32_t history_usd_cents[OFFLINE_MARKET_HISTORY_MAX];

    uint32_t observed_at_unix_s;
} offline_market_data_t;

/* USD/BRL ------------------------------------------------------------------ */

/* PTAX USD sale rate, fixed-point in BRL ten-thousandths. */
typedef struct {
    bool available;
    bool stale;

    uint32_t usd_brl_ten_thousandths;

    /* Optional because the current provider/contract may not expose a
     * comparable previous value yet. */
    bool change_available;
    int16_t change_basis_points;

    uint32_t observed_at_unix_s;
} offline_exchange_data_t;

/* Ibovespa ----------------------------------------------------------------- */

typedef struct {
    bool available;
    bool stale;

    /* Index points × 100. Example: 134567.25 -> 13,456,725. */
    uint32_t value_centi_points;

    bool change_available;
    int16_t change_basis_points;

    uint32_t observed_at_unix_s;
} offline_ibovespa_data_t;

typedef struct {
    bool available;
    bool stale;
    uint32_t value_centi_points;
    bool change_available;
    int16_t change_basis_points;
    uint32_t observed_at_unix_s;
} offline_index_data_t;

/* Additional screen quotes. Price is USD × 1,000,000 to retain precision for
 * assets whose unit price is below one dollar. Order: ETH, SOL, BNB, XRP. */
typedef struct {
    bool available;
    bool stale;
    uint64_t usd_micros;
    int16_t change_24h_basis_points;
    uint32_t observed_at_unix_s;
} offline_altcoin_data_t;

typedef struct {
    bool available;
    bool stale;
    uint8_t value;
    /* 0=Extreme Fear, 1=Fear, 2=Neutral, 3=Greed, 4=Extreme Greed. */
    uint8_t classification;
    uint32_t observed_at_unix_s;
} offline_fear_greed_data_t;

/* Snapshot ----------------------------------------------------------------- */

typedef struct {
    uint16_t schema_version;
    offline_data_origin_t origin;

    offline_weather_data_t weather;
    offline_weather_visual_t weather_visual;

    offline_market_data_t market;
    offline_exchange_data_t exchange;
    offline_ibovespa_data_t ibovespa;
    offline_index_data_t sp500;
    offline_index_data_t nasdaq;
    offline_altcoin_data_t altcoins[OFFLINE_MARKET_ALTCOIN_COUNT];
    offline_fear_greed_data_t fear_greed;
} offline_data_snapshot_t;
