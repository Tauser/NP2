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
                    .observed_at_unix_s = UINT32_C(1760000000)},
        .market = {.available = true, .stale = true, .bitcoin_usd_cents = UINT32_C(11234567),
                   .change_24h_basis_points = -125, .observed_at_unix_s = UINT32_C(1760000010)},
    };
    uint8_t encoded[OFFLINE_DATA_ENCODED_SIZE] = {0};
    offline_data_snapshot_t decoded = {0};

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    assert(offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    assert(decoded.schema_version == original.schema_version && decoded.origin == original.origin);
    assert(decoded.weather.available && decoded.weather.temperature_deci_c == -37 &&
           decoded.weather.relative_humidity_percent == 84 && decoded.weather.weather_code == 61 &&
           decoded.weather.observed_at_unix_s == UINT32_C(1760000000));
    assert(decoded.market.available && decoded.market.stale &&
           decoded.market.bitcoin_usd_cents == UINT32_C(11234567) &&
           decoded.market.change_24h_basis_points == -125 &&
           decoded.market.observed_at_unix_s == UINT32_C(1760000010));
    encoded[3] = 1U;
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
    invalid.weather.available = false;
    assert(!offline_data_snapshot_encode(&invalid, encoded, sizeof(encoded)));

    assert(offline_data_snapshot_encode(&original, encoded, sizeof(encoded)));
    encoded[5] = UINT8_C(0xd0);
    encoded[6] = UINT8_C(0x07); /* 200.0 C with structurally valid encoding. */
    assert(!offline_data_snapshot_decode(encoded, sizeof(encoded), &decoded));
    return 0;
}
