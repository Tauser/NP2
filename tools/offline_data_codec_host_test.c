#include <assert.h>
#include <stdint.h>

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

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    encoded[5] = UINT8_C(0xd0);
    encoded[6] = UINT8_C(0x07); /* 200.0 C with structurally valid encoding. */
    assert(!offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    return 0;
}
