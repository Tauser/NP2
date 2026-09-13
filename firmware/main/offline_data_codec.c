#include "offline_data_codec.h"

#define SNAPSHOT_FLAG_AVAILABLE UINT8_C(0x01)
#define SNAPSHOT_FLAG_STALE UINT8_C(0x02)

static void put_u16_le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8U);
}

static void put_u32_le(uint8_t *out, uint32_t value)
{
    for (size_t index = 0; index < 4U; ++index) {
        out[index] = (uint8_t)(value >> (8U * index));
    }
}

static uint16_t get_u16_le(const uint8_t *in)
{
    return (uint16_t)in[0] | ((uint16_t)in[1] << 8U);
}

static uint32_t get_u32_le(const uint8_t *in)
{
    uint32_t value = 0U;
    for (size_t index = 0; index < 4U; ++index) {
        value |= (uint32_t)in[index] << (8U * index);
    }
    return value;
}

static uint8_t flags_for(bool available, bool stale)
{
    return (available ? SNAPSHOT_FLAG_AVAILABLE : 0U) |
           (stale ? SNAPSHOT_FLAG_STALE : 0U);
}

static bool flags_are_valid(uint8_t flags)
{
    return (flags & ~(SNAPSHOT_FLAG_AVAILABLE | SNAPSHOT_FLAG_STALE)) == 0U &&
           ((flags & SNAPSHOT_FLAG_STALE) == 0U || (flags & SNAPSHOT_FLAG_AVAILABLE) != 0U);
}

bool offline_data_snapshot_is_valid(const offline_data_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot->schema_version != OFFLINE_DATA_SCHEMA_VERSION ||
        snapshot->origin < OFFLINE_DATA_ORIGIN_CACHE ||
        snapshot->origin > OFFLINE_DATA_ORIGIN_LIVE) {
        return false;
    }
    if ((!snapshot->weather.available && snapshot->weather.stale) ||
        (!snapshot->market.available && snapshot->market.stale)) {
        return false;
    }
    if (snapshot->weather.available) {
        if (snapshot->weather.temperature_deci_c < -1000 ||
            snapshot->weather.temperature_deci_c > 1000 ||
            snapshot->weather.relative_humidity_percent > 100U ||
            snapshot->weather.observed_at_unix_s == 0U) {
            return false;
        }
    } else if (snapshot->weather.temperature_deci_c != 0 ||
               snapshot->weather.relative_humidity_percent != 0U ||
               snapshot->weather.weather_code != 0U ||
               snapshot->weather.observed_at_unix_s != 0U) {
        return false;
    }
    if (snapshot->market.available) {
        if (snapshot->market.observed_at_unix_s == 0U) {
            return false;
        }
    } else if (snapshot->market.bitcoin_usd_cents != 0U ||
               snapshot->market.change_24h_basis_points != 0 ||
               snapshot->market.observed_at_unix_s != 0U) {
        return false;
    }
    return snapshot->weather.available || snapshot->market.available;
}

bool offline_data_snapshot_encode(const offline_data_snapshot_t *snapshot, uint8_t *out_bytes,
                                  size_t out_size)
{
    if (!offline_data_snapshot_is_valid(snapshot) || out_bytes == NULL ||
        out_size != OFFLINE_DATA_ENCODED_SIZE) {
        return false;
    }

    put_u16_le(&out_bytes[0], snapshot->schema_version);
    out_bytes[2] = (uint8_t)snapshot->origin;
    out_bytes[3] = 0U;
    out_bytes[4] = flags_for(snapshot->weather.available, snapshot->weather.stale);
    put_u16_le(&out_bytes[5], (uint16_t)snapshot->weather.temperature_deci_c);
    out_bytes[7] = snapshot->weather.relative_humidity_percent;
    put_u16_le(&out_bytes[8], snapshot->weather.weather_code);
    put_u32_le(&out_bytes[10], snapshot->weather.observed_at_unix_s);
    out_bytes[14] = flags_for(snapshot->market.available, snapshot->market.stale);
    put_u32_le(&out_bytes[15], snapshot->market.bitcoin_usd_cents);
    put_u16_le(&out_bytes[19], (uint16_t)snapshot->market.change_24h_basis_points);
    put_u32_le(&out_bytes[21], snapshot->market.observed_at_unix_s);
    return true;
}

bool offline_data_snapshot_decode(const uint8_t *bytes, size_t size,
                                  offline_data_snapshot_t *out_snapshot)
{
    if (bytes == NULL || out_snapshot == NULL || size != OFFLINE_DATA_ENCODED_SIZE ||
        bytes[3] != 0U || !flags_are_valid(bytes[4]) || !flags_are_valid(bytes[14])) {
        return false;
    }

    offline_data_snapshot_t candidate = {
        .schema_version = get_u16_le(&bytes[0]),
        .origin = (offline_data_origin_t)bytes[2],
        .weather = {
            .available = (bytes[4] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
            .stale = (bytes[4] & SNAPSHOT_FLAG_STALE) != 0U,
            .temperature_deci_c = (int16_t)get_u16_le(&bytes[5]),
            .relative_humidity_percent = bytes[7],
            .weather_code = get_u16_le(&bytes[8]),
            .observed_at_unix_s = get_u32_le(&bytes[10]),
        },
        .market = {
            .available = (bytes[14] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
            .stale = (bytes[14] & SNAPSHOT_FLAG_STALE) != 0U,
            .bitcoin_usd_cents = get_u32_le(&bytes[15]),
            .change_24h_basis_points = (int16_t)get_u16_le(&bytes[19]),
            .observed_at_unix_s = get_u32_le(&bytes[21]),
        },
    };
    if (!offline_data_snapshot_is_valid(&candidate)) {
        return false;
    }
    *out_snapshot = candidate;
    return true;
}
