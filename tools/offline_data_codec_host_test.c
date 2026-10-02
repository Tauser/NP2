#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "offline_data_codec.h"

int main(void)
{
    const offline_data_snapshot_t original = {
        .schema_version = OFFLINE_DATA_SCHEMA_VERSION,
        .origin = OFFLINE_DATA_ORIGIN_LIVE,
        .weather = {.available = true, .temperature_deci_c = -37,
                    .relative_humidity_percent = 84, .weather_code = 61,
                    .apparent_temperature_available = true, .apparent_temperature_deci_c = -52,
                    .wind_speed_available = true, .wind_speed_deci_kmh = 114,
                    .uv_index_available = true, .uv_index_deci = 63,
                    .wind_direction_available = true, .wind_direction_degrees = 45,
                    .forecast_available = true, .utc_offset_seconds = -10800,
                    .today_temperature_max_deci_c = 281, .today_temperature_min_deci_c = 174,
                    .today_precipitation_sum_deci_mm = 15,
                    .sunrise_unix_s = UINT32_C(1760001000), .sunset_unix_s = UINT32_C(1760040000),
                    .hourly = {
                        {UINT32_C(1760000400), 237, 3, 0}, {UINT32_C(1760004000), 250, 2, 5},
                        {UINT32_C(1760007600), 240, 3, 30}, {UINT32_C(1760011200), 220, 61, 80},
                        {UINT32_C(1760014800), 200, 61, 20},
                    },
                    .daily = {
                        {UINT32_C(1760054400), 180, 280, 2}, {UINT32_C(1760140800), 170, 240, 61},
                        {UINT32_C(1760227200), 160, 230, 3}, {UINT32_C(1760313600), 170, 260, 0},
                    },
                    .observed_at_unix_s = UINT32_C(1760000000)},
        .weather_visual = {.available = true, .is_day = false},
        .market = {.available = true, .stale = true, .bitcoin_usd_cents = UINT32_C(11234567),
                   .change_24h_basis_points = -125, .high_24h_available = true,
                   .high_24h_usd_cents = UINT32_C(11300000), .low_24h_available = true,
                   .low_24h_usd_cents = UINT32_C(11000000), .volume_24h_available = true,
                   .volume_24h_usd_cents = UINT64_C(2040000000000),
                   .observed_at_unix_s = UINT32_C(1760000010)},
        .exchange = {.available = true, .usd_brl_ten_thousandths = UINT32_C(51161),
                     .change_available = true, .change_basis_points = 21,
                     .observed_at_unix_s = UINT32_C(1760000020)},
        .sp500 = {.available = true, .value_centi_points = UINT32_C(574512),
                  .change_available = true, .change_basis_points = 33,
                  .observed_at_unix_s = UINT32_C(1760000020)},
        .nasdaq = {.available = true, .value_centi_points = UINT32_C(1827942),
                   .change_available = true, .change_basis_points = -28,
                   .observed_at_unix_s = UINT32_C(1760000020)},
        .altcoins = {
            {.available = true, .usd_micros = UINT64_C(3842000000),
             .change_24h_basis_points = 132, .observed_at_unix_s = UINT32_C(1760000030)},
            {.available = true, .usd_micros = UINT64_C(218410000),
             .change_24h_basis_points = 312, .observed_at_unix_s = UINT32_C(1760000030)},
        },
        .fear_greed = {.available = true, .value = 72, .classification = 3,
                       .observed_at_unix_s = UINT32_C(1760000040)},
    };
    uint8_t encoded[OFFLINE_DATA_ENCODED_SIZE] = {0};
    offline_data_snapshot_t decoded = {0};

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    assert(offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.schema_version == original.schema_version && decoded.origin == original.origin);
    assert(decoded.weather.available && decoded.weather.temperature_deci_c == -37 &&
           decoded.weather.relative_humidity_percent == 84 && decoded.weather.weather_code == 61 &&
           decoded.weather.observed_at_unix_s == UINT32_C(1760000000));
    assert(decoded.weather_visual.available && !decoded.weather_visual.is_day);
    assert(decoded.weather.apparent_temperature_deci_c == -52 &&
           decoded.weather.wind_speed_deci_kmh == 114 && decoded.weather.uv_index_deci == 63);
    assert(decoded.weather.forecast_available && decoded.weather.wind_direction_degrees == 45U &&
           decoded.weather.utc_offset_seconds == -10800 &&
           decoded.weather.hourly[3].weather_code == 61U &&
           decoded.weather.daily[2].temperature_max_deci_c == 230);
    assert(decoded.market.available && decoded.market.stale &&
           decoded.market.bitcoin_usd_cents == UINT32_C(11234567) &&
           decoded.market.change_24h_basis_points == -125 &&
           decoded.market.observed_at_unix_s == UINT32_C(1760000010));
    assert(decoded.market.high_24h_usd_cents == UINT32_C(11300000) &&
           decoded.market.low_24h_usd_cents == UINT32_C(11000000) &&
           decoded.market.volume_24h_usd_cents == UINT64_C(2040000000000));
    assert(decoded.exchange.available && !decoded.exchange.stale &&
           decoded.exchange.usd_brl_ten_thousandths == UINT32_C(51161) &&
           decoded.exchange.observed_at_unix_s == UINT32_C(1760000020));
    assert(decoded.exchange.change_available && decoded.exchange.change_basis_points == 21);
    assert(decoded.sp500.available && decoded.sp500.value_centi_points == UINT32_C(574512) &&
           decoded.sp500.change_basis_points == 33 && decoded.nasdaq.available &&
           decoded.nasdaq.value_centi_points == UINT32_C(1827942) &&
           decoded.nasdaq.change_basis_points == -28);
    assert(decoded.altcoins[0].available && decoded.altcoins[0].usd_micros == UINT64_C(3842000000) &&
           decoded.altcoins[0].change_24h_basis_points == 132 &&
           decoded.altcoins[1].available && decoded.altcoins[1].usd_micros == UINT64_C(218410000));
    assert(decoded.fear_greed.available && decoded.fear_greed.value == 72U &&
           decoded.fear_greed.classification == 3U &&
           decoded.fear_greed.observed_at_unix_s == UINT32_C(1760000040));
    encoded[3] = 4U;
    assert(!offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    assert(!offline_data_snapshot_decode(encoded, sizeof(encoded) - 1U, &decoded));
    offline_data_snapshot_t empty = {
        .schema_version = OFFLINE_DATA_SCHEMA_VERSION,
        .origin = OFFLINE_DATA_ORIGIN_CACHE,
    };
    assert(!offline_data_snapshot_encode(&empty, encoded, sizeof(encoded)));
    offline_data_snapshot_t invalid = original;
    invalid.weather.temperature_deci_c = 2000;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.weather.relative_humidity_percent = 255U;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.weather.observed_at_unix_s = 0U;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.market.observed_at_unix_s = 0U;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.exchange.usd_brl_ten_thousandths = 0U;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.weather_visual.available = true;
    invalid.weather.available = false;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));
    invalid = original;
    invalid.weather.available = false;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    uint8_t previous[OFFLINE_DATA_V3_ENCODED_SIZE] = {0};
    for (size_t index = 0; index < sizeof(previous); ++index) previous[index] = encoded[index];
    previous[0] = 3U;
    assert(offline_data_snapshot_decode(previous, sizeof(previous), &decoded));
    assert(decoded.schema_version == OFFLINE_DATA_SCHEMA_VERSION &&
           decoded.weather_visual.available && !decoded.weather.wind_speed_available &&
           !decoded.market.high_24h_available && !decoded.exchange.change_available);

    uint8_t v4[OFFLINE_DATA_V4_ENCODED_SIZE] = {0};
    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    for (size_t index = 0; index < sizeof(v4); ++index) v4[index] = encoded[index];
    v4[0] = 4U;
    assert(offline_data_snapshot_decode(v4, sizeof(v4), &decoded));
    assert(decoded.schema_version == OFFLINE_DATA_SCHEMA_VERSION &&
           decoded.weather.wind_speed_available && !decoded.weather.forecast_available &&
           !decoded.weather.wind_direction_available);

    uint8_t v5[OFFLINE_DATA_V5_ENCODED_SIZE] = {0};
    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    for (size_t index = 0U; index < sizeof(v5); ++index) v5[index] = encoded[index];
    v5[0] = 5U;
    assert(offline_data_snapshot_decode(v5, sizeof(v5), &decoded));
    assert(decoded.schema_version == OFFLINE_DATA_SCHEMA_VERSION &&
           !decoded.altcoins[0].available && !decoded.fear_greed.available);

    uint8_t v6[OFFLINE_DATA_V6_ENCODED_SIZE] = {0};
    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    for (size_t index = 0U; index < sizeof(v6); ++index) v6[index] = encoded[index];
    v6[0] = 6U;
    v6[340] = 0U; /* Reserved byte in v6; v7 starts its index extension here. */
    assert(offline_data_snapshot_decode(v6, sizeof(v6), &decoded));
    assert(decoded.schema_version == OFFLINE_DATA_SCHEMA_VERSION &&
           !decoded.sp500.available && !decoded.nasdaq.available);

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    encoded[5] = UINT8_C(0xd0);
    encoded[6] = UINT8_C(0x07); /* 200.0 C with structurally valid encoding. */
    assert(!offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    return 0;
}
