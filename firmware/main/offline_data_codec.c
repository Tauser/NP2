#include "offline_data_codec.h"

#define SNAPSHOT_FLAG_AVAILABLE UINT8_C(0x01)
#define SNAPSHOT_FLAG_STALE UINT8_C(0x02)
#define SNAPSHOT_VISUAL_AVAILABLE UINT8_C(0x01)
#define SNAPSHOT_VISUAL_DAY UINT8_C(0x02)
#define DETAIL_APPARENT UINT8_C(0x01)
#define DETAIL_WIND UINT8_C(0x02)
#define DETAIL_UV UINT8_C(0x04)
#define DETAIL_HIGH UINT8_C(0x01)
#define DETAIL_LOW UINT8_C(0x02)
#define DETAIL_VOLUME UINT8_C(0x04)
#define DETAIL_CHANGE UINT8_C(0x04)

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

static void put_u64_le(uint8_t *out, uint64_t value)
{
    for (size_t index = 0; index < 8U; ++index) out[index] = (uint8_t)(value >> (8U * index));
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

static uint64_t get_u64_le(const uint8_t *in)
{
    uint64_t value = 0U;
    for (size_t index = 0; index < 8U; ++index) value |= (uint64_t)in[index] << (8U * index);
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

static uint8_t weather_visual_flags_for(const offline_weather_visual_t *visual)
{
    return visual->available
               ? (uint8_t)(SNAPSHOT_VISUAL_AVAILABLE |
                           (visual->is_day ? SNAPSHOT_VISUAL_DAY : 0U))
               : 0U;
}

static bool weather_visual_flags_are_valid(uint8_t flags)
{
    return (flags & ~(SNAPSHOT_VISUAL_AVAILABLE | SNAPSHOT_VISUAL_DAY)) == 0U &&
           ((flags & SNAPSHOT_VISUAL_DAY) == 0U ||
            (flags & SNAPSHOT_VISUAL_AVAILABLE) != 0U);
}

bool offline_data_snapshot_is_valid(const offline_data_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot->schema_version != OFFLINE_DATA_SCHEMA_VERSION ||
        snapshot->origin < OFFLINE_DATA_ORIGIN_CACHE ||
        snapshot->origin > OFFLINE_DATA_ORIGIN_LIVE) {
        return false;
    }
    if ((!snapshot->weather.available && snapshot->weather.stale) ||
        (!snapshot->market.available && snapshot->market.stale) ||
        (!snapshot->exchange.available && snapshot->exchange.stale) ||
        (!snapshot->ibovespa.available && snapshot->ibovespa.stale) ||
        (!snapshot->sp500.available && snapshot->sp500.stale) ||
        (!snapshot->nasdaq.available && snapshot->nasdaq.stale)) {
        return false;
    }
    if (snapshot->weather_visual.available && !snapshot->weather.available) {
        return false;
    }
    if (snapshot->weather.available) {
        if (snapshot->weather.temperature_deci_c < -1000 ||
            snapshot->weather.temperature_deci_c > 1000 ||
            snapshot->weather.relative_humidity_percent > 100U ||
            (snapshot->weather.apparent_temperature_available &&
             (snapshot->weather.apparent_temperature_deci_c < -1000 ||
              snapshot->weather.apparent_temperature_deci_c > 1000)) ||
            (snapshot->weather.wind_speed_available && snapshot->weather.wind_speed_deci_kmh > 2000U) ||
            (snapshot->weather.uv_index_available && snapshot->weather.uv_index_deci > 250U) ||
            snapshot->weather.observed_at_unix_s == 0U) {
            return false;
        }
    } else if (snapshot->weather.temperature_deci_c != 0 ||
               snapshot->weather.relative_humidity_percent != 0U ||
               snapshot->weather.weather_code != 0U ||
               snapshot->weather.observed_at_unix_s != 0U ||
               snapshot->weather.apparent_temperature_available ||
               snapshot->weather.wind_speed_available || snapshot->weather.uv_index_available) {
        return false;
    }
    if ((!snapshot->weather.apparent_temperature_available && snapshot->weather.apparent_temperature_deci_c != 0) ||
        (!snapshot->weather.wind_speed_available && snapshot->weather.wind_speed_deci_kmh != 0U) ||
        (!snapshot->weather.uv_index_available && snapshot->weather.uv_index_deci != 0U)) return false;
    if ((!snapshot->weather.wind_direction_available && snapshot->weather.wind_direction_degrees != 0U) ||
        (snapshot->weather.wind_direction_available && snapshot->weather.wind_direction_degrees > 360U)) return false;
    if (snapshot->weather.forecast_available) {
        if (snapshot->weather.utc_offset_seconds < -50400 ||
            snapshot->weather.utc_offset_seconds > 50400 ||
            snapshot->weather.today_temperature_min_deci_c < -1000 ||
            snapshot->weather.today_temperature_min_deci_c > 1000 ||
            snapshot->weather.today_temperature_max_deci_c < -1000 ||
            snapshot->weather.today_temperature_max_deci_c > 1000 ||
            snapshot->weather.today_temperature_min_deci_c > snapshot->weather.today_temperature_max_deci_c ||
            snapshot->weather.today_precipitation_sum_deci_mm > 10000U ||
            snapshot->weather.sunrise_unix_s == 0U || snapshot->weather.sunset_unix_s == 0U ||
            snapshot->weather.sunrise_unix_s >= snapshot->weather.sunset_unix_s) return false;
        for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
            if (snapshot->weather.hourly[index].time_unix_s == 0U ||
                snapshot->weather.hourly[index].temperature_deci_c < -1000 ||
                snapshot->weather.hourly[index].temperature_deci_c > 1000 ||
                snapshot->weather.hourly[index].precipitation_probability_percent > 100U ||
                (index > 0U && snapshot->weather.hourly[index].time_unix_s <=
                                  snapshot->weather.hourly[index - 1U].time_unix_s)) return false;
        }
        for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
            if (snapshot->weather.daily[index].time_unix_s == 0U ||
                snapshot->weather.daily[index].temperature_min_deci_c < -1000 ||
                snapshot->weather.daily[index].temperature_min_deci_c > 1000 ||
                snapshot->weather.daily[index].temperature_max_deci_c < -1000 ||
                snapshot->weather.daily[index].temperature_max_deci_c > 1000 ||
                snapshot->weather.daily[index].temperature_min_deci_c >
                    snapshot->weather.daily[index].temperature_max_deci_c ||
                (index > 0U && snapshot->weather.daily[index].time_unix_s <=
                                  snapshot->weather.daily[index - 1U].time_unix_s)) return false;
        }
    } else if (snapshot->weather.utc_offset_seconds != 0 ||
               snapshot->weather.today_temperature_max_deci_c != 0 ||
               snapshot->weather.today_temperature_min_deci_c != 0 ||
               snapshot->weather.today_precipitation_sum_deci_mm != 0U ||
               snapshot->weather.sunrise_unix_s != 0U || snapshot->weather.sunset_unix_s != 0U) return false;
    if (snapshot->market.available) {
        if (snapshot->market.observed_at_unix_s == 0U ||
            snapshot->market.history_count > OFFLINE_MARKET_HISTORY_MAX ||
            (snapshot->market.high_24h_available && snapshot->market.high_24h_usd_cents == 0U) ||
            (snapshot->market.low_24h_available && snapshot->market.low_24h_usd_cents == 0U) ||
            (snapshot->market.high_24h_available && snapshot->market.low_24h_available &&
             snapshot->market.high_24h_usd_cents < snapshot->market.low_24h_usd_cents)) {
            return false;
        }
    } else if (snapshot->market.bitcoin_usd_cents != 0U ||
               snapshot->market.change_24h_basis_points != 0 ||
               snapshot->market.observed_at_unix_s != 0U || snapshot->market.high_24h_available ||
               snapshot->market.low_24h_available || snapshot->market.volume_24h_available ||
               snapshot->market.history_count != 0U) {
        return false;
    }
    if ((!snapshot->market.high_24h_available && snapshot->market.high_24h_usd_cents != 0U) ||
        (!snapshot->market.low_24h_available && snapshot->market.low_24h_usd_cents != 0U) ||
        (!snapshot->market.volume_24h_available && snapshot->market.volume_24h_usd_cents != 0U)) return false;
    for (size_t index = 0U; index < OFFLINE_MARKET_HISTORY_MAX; ++index) {
        const bool used = index < snapshot->market.history_count;
        if ((used && snapshot->market.history_usd_cents[index] == 0U) ||
            (!used && snapshot->market.history_usd_cents[index] != 0U)) {
            return false;
        }
    }
    if (snapshot->exchange.available) {
        if (snapshot->exchange.usd_brl_ten_thousandths == 0U ||
            snapshot->exchange.usd_brl_ten_thousandths > UINT32_C(1000000) ||
            snapshot->exchange.observed_at_unix_s == 0U) {
            return false;
        }
    } else if (snapshot->exchange.usd_brl_ten_thousandths != 0U ||
               snapshot->exchange.observed_at_unix_s != 0U || snapshot->exchange.change_available) {
        return false;
    }
    if (!snapshot->exchange.change_available && snapshot->exchange.change_basis_points != 0) return false;
    if (snapshot->ibovespa.available) {
        if (snapshot->ibovespa.value_centi_points == 0U ||
            snapshot->ibovespa.value_centi_points > UINT32_C(1000000000) ||
            snapshot->ibovespa.observed_at_unix_s == 0U) return false;
    } else if (snapshot->ibovespa.value_centi_points != 0U ||
               snapshot->ibovespa.change_available ||
               snapshot->ibovespa.observed_at_unix_s != 0U) return false;
    if (!snapshot->ibovespa.change_available && snapshot->ibovespa.change_basis_points != 0) return false;
    const offline_index_data_t *const indices[] = {&snapshot->sp500, &snapshot->nasdaq};
    for (size_t index = 0U; index < sizeof(indices) / sizeof(indices[0]); ++index) {
        const offline_index_data_t *const quote = indices[index];
        if (quote->available) {
            if (quote->value_centi_points == 0U ||
                quote->value_centi_points > UINT32_C(100000000) ||
                quote->observed_at_unix_s == 0U) return false;
        } else if (quote->value_centi_points != 0U || quote->change_available ||
                   quote->change_basis_points != 0 || quote->observed_at_unix_s != 0U) {
            return false;
        }
        if (!quote->change_available && quote->change_basis_points != 0) return false;
    }
    bool market_extension_available = false;
    for (size_t index = 0U; index < OFFLINE_MARKET_ALTCOIN_COUNT; ++index) {
        const offline_altcoin_data_t *coin = &snapshot->altcoins[index];
        if ((!coin->available && (coin->stale || coin->usd_micros != 0U ||
             coin->change_24h_basis_points != 0 || coin->observed_at_unix_s != 0U)) ||
            (coin->available && (coin->usd_micros == 0U || coin->observed_at_unix_s == 0U)))
            return false;
        market_extension_available |= coin->available;
    }
    if ((!snapshot->fear_greed.available &&
         (snapshot->fear_greed.stale || snapshot->fear_greed.value != 0U ||
          snapshot->fear_greed.classification != 0U ||
          snapshot->fear_greed.observed_at_unix_s != 0U)) ||
        (snapshot->fear_greed.available &&
         (snapshot->fear_greed.value > 100U || snapshot->fear_greed.classification > 4U ||
          snapshot->fear_greed.observed_at_unix_s == 0U))) return false;
    market_extension_available |= snapshot->fear_greed.available;
    return snapshot->weather.available || snapshot->market.available ||
           snapshot->exchange.available || snapshot->ibovespa.available ||
           snapshot->sp500.available || snapshot->nasdaq.available ||
           market_extension_available;
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
    out_bytes[3] = weather_visual_flags_for(&snapshot->weather_visual);
    out_bytes[4] = flags_for(snapshot->weather.available, snapshot->weather.stale);
    put_u16_le(&out_bytes[5], (uint16_t)snapshot->weather.temperature_deci_c);
    out_bytes[7] = snapshot->weather.relative_humidity_percent;
    put_u16_le(&out_bytes[8], snapshot->weather.weather_code);
    put_u32_le(&out_bytes[10], snapshot->weather.observed_at_unix_s);
    out_bytes[14] = flags_for(snapshot->market.available, snapshot->market.stale);
    put_u32_le(&out_bytes[15], snapshot->market.bitcoin_usd_cents);
    put_u16_le(&out_bytes[19], (uint16_t)snapshot->market.change_24h_basis_points);
    put_u32_le(&out_bytes[21], snapshot->market.observed_at_unix_s);
    out_bytes[25] = flags_for(snapshot->exchange.available, snapshot->exchange.stale);
    put_u32_le(&out_bytes[26], snapshot->exchange.usd_brl_ten_thousandths);
    put_u32_le(&out_bytes[30], snapshot->exchange.observed_at_unix_s);
    out_bytes[34] = (snapshot->weather.apparent_temperature_available ? DETAIL_APPARENT : 0U) |
                    (snapshot->weather.wind_speed_available ? DETAIL_WIND : 0U) |
                    (snapshot->weather.uv_index_available ? DETAIL_UV : 0U);
    put_u16_le(&out_bytes[35], (uint16_t)snapshot->weather.apparent_temperature_deci_c);
    put_u16_le(&out_bytes[37], snapshot->weather.wind_speed_deci_kmh);
    put_u16_le(&out_bytes[39], snapshot->weather.uv_index_deci);
    out_bytes[41] = (snapshot->market.high_24h_available ? DETAIL_HIGH : 0U) |
                    (snapshot->market.low_24h_available ? DETAIL_LOW : 0U) |
                    (snapshot->market.volume_24h_available ? DETAIL_VOLUME : 0U);
    put_u32_le(&out_bytes[42], snapshot->market.high_24h_usd_cents);
    put_u32_le(&out_bytes[46], snapshot->market.low_24h_usd_cents);
    put_u64_le(&out_bytes[50], snapshot->market.volume_24h_usd_cents);
    out_bytes[58] = snapshot->market.history_count;
    for (size_t index = 0; index < OFFLINE_MARKET_HISTORY_MAX; ++index)
        put_u32_le(&out_bytes[59U + index * 4U], snapshot->market.history_usd_cents[index]);
    out_bytes[155] = snapshot->exchange.change_available ? DETAIL_CHANGE : 0U;
    put_u16_le(&out_bytes[156], (uint16_t)snapshot->exchange.change_basis_points);
    out_bytes[158] = flags_for(snapshot->ibovespa.available, snapshot->ibovespa.stale) |
                     (snapshot->ibovespa.change_available ? DETAIL_CHANGE : 0U);
    put_u32_le(&out_bytes[159], snapshot->ibovespa.value_centi_points);
    put_u16_le(&out_bytes[163], (uint16_t)snapshot->ibovespa.change_basis_points);
    put_u32_le(&out_bytes[165], snapshot->ibovespa.observed_at_unix_s);
    out_bytes[169] = (snapshot->weather.wind_direction_available ? UINT8_C(0x01) : 0U) |
                     (snapshot->weather.forecast_available ? UINT8_C(0x02) : 0U);
    put_u16_le(&out_bytes[170], snapshot->weather.wind_direction_degrees);
    put_u32_le(&out_bytes[172], (uint32_t)snapshot->weather.utc_offset_seconds);
    put_u16_le(&out_bytes[176], (uint16_t)snapshot->weather.today_temperature_max_deci_c);
    put_u16_le(&out_bytes[178], (uint16_t)snapshot->weather.today_temperature_min_deci_c);
    put_u16_le(&out_bytes[180], snapshot->weather.today_precipitation_sum_deci_mm);
    put_u32_le(&out_bytes[182], snapshot->weather.sunrise_unix_s);
    put_u32_le(&out_bytes[186], snapshot->weather.sunset_unix_s);
    size_t offset = 190U;
    for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
        put_u32_le(&out_bytes[offset], snapshot->weather.hourly[index].time_unix_s);
        put_u16_le(&out_bytes[offset + 4U], (uint16_t)snapshot->weather.hourly[index].temperature_deci_c);
        put_u16_le(&out_bytes[offset + 6U], snapshot->weather.hourly[index].weather_code);
        out_bytes[offset + 8U] = snapshot->weather.hourly[index].precipitation_probability_percent;
        offset += 9U;
    }
    for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
        put_u32_le(&out_bytes[offset], snapshot->weather.daily[index].time_unix_s);
        put_u16_le(&out_bytes[offset + 4U], (uint16_t)snapshot->weather.daily[index].temperature_min_deci_c);
        put_u16_le(&out_bytes[offset + 6U], (uint16_t)snapshot->weather.daily[index].temperature_max_deci_c);
        put_u16_le(&out_bytes[offset + 8U], snapshot->weather.daily[index].weather_code);
        offset += 10U;
    }
    for (size_t index = 0U; index < OFFLINE_MARKET_ALTCOIN_COUNT; ++index) {
        const offline_altcoin_data_t *coin = &snapshot->altcoins[index];
        out_bytes[offset] = flags_for(coin->available, coin->stale);
        put_u64_le(&out_bytes[offset + 1U], coin->usd_micros);
        put_u16_le(&out_bytes[offset + 9U], (uint16_t)coin->change_24h_basis_points);
        put_u32_le(&out_bytes[offset + 11U], coin->observed_at_unix_s);
        offset += 15U;
    }
    out_bytes[offset] = flags_for(snapshot->fear_greed.available, snapshot->fear_greed.stale);
    out_bytes[offset + 1U] = snapshot->fear_greed.value;
    out_bytes[offset + 2U] = snapshot->fear_greed.classification;
    put_u32_le(&out_bytes[offset + 3U], snapshot->fear_greed.observed_at_unix_s);
    offset = OFFLINE_DATA_V6_FIELDS_SIZE;
    const offline_index_data_t *const indices[] = {&snapshot->sp500, &snapshot->nasdaq};
    for (size_t index = 0U; index < sizeof(indices) / sizeof(indices[0]); ++index) {
        const offline_index_data_t *const quote = indices[index];
        out_bytes[offset] = flags_for(quote->available, quote->stale) |
                            (quote->change_available ? DETAIL_CHANGE : 0U);
        put_u32_le(&out_bytes[offset + 1U], quote->value_centi_points);
        put_u16_le(&out_bytes[offset + 5U], (uint16_t)quote->change_basis_points);
        put_u32_le(&out_bytes[offset + 7U], quote->observed_at_unix_s);
        offset += 11U;
    }
    return true;
}

bool offline_data_snapshot_decode(const uint8_t *bytes, size_t size,
                                  offline_data_snapshot_t *out_snapshot)
{
    if (bytes == NULL || out_snapshot == NULL ||
        (size != OFFLINE_DATA_V1_ENCODED_SIZE && size != OFFLINE_DATA_V2_ENCODED_SIZE &&
         size != OFFLINE_DATA_V4_ENCODED_SIZE && size != OFFLINE_DATA_V5_ENCODED_SIZE &&
         size != OFFLINE_DATA_V6_ENCODED_SIZE && size != OFFLINE_DATA_ENCODED_SIZE)) {
        return false;
    }
    const uint16_t schema_version = get_u16_le(&bytes[0]);
    const bool v4_details_invalid = size >= OFFLINE_DATA_V4_ENCODED_SIZE &&
        ((bytes[34] & ~UINT8_C(7)) != 0U || (bytes[41] & ~UINT8_C(7)) != 0U ||
         (bytes[155] & ~DETAIL_CHANGE) != 0U || (bytes[158] & ~UINT8_C(7)) != 0U ||
         !flags_are_valid(bytes[158] & UINT8_C(3)));
    if ((size == OFFLINE_DATA_V1_ENCODED_SIZE && schema_version != UINT16_C(1)) ||
        (size == OFFLINE_DATA_V2_ENCODED_SIZE &&
         (schema_version != UINT16_C(2) && schema_version != UINT16_C(3))) ||
        (size == OFFLINE_DATA_V4_ENCODED_SIZE && schema_version != UINT16_C(4)) ||
        (size == OFFLINE_DATA_V5_ENCODED_SIZE && schema_version != UINT16_C(5)) ||
        (size == OFFLINE_DATA_V6_ENCODED_SIZE && schema_version != UINT16_C(6)) ||
        (size == OFFLINE_DATA_ENCODED_SIZE && schema_version != OFFLINE_DATA_SCHEMA_VERSION) ||
        ((schema_version == UINT16_C(1) || schema_version == UINT16_C(2)) &&
         bytes[3] != 0U) ||
        (schema_version >= UINT16_C(3) &&
         !weather_visual_flags_are_valid(bytes[3])) ||
        !flags_are_valid(bytes[4]) || !flags_are_valid(bytes[14]) ||
        (size >= OFFLINE_DATA_V2_ENCODED_SIZE && !flags_are_valid(bytes[25])) ||
        v4_details_invalid ||
        (size >= OFFLINE_DATA_V6_ENCODED_SIZE && (bytes[169] & ~UINT8_C(3)) != 0U)) {
        return false;
    }

    offline_data_snapshot_t candidate = {
        .schema_version = OFFLINE_DATA_SCHEMA_VERSION,
        .origin = (offline_data_origin_t)bytes[2],
        .weather = {
            .available = (bytes[4] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
            .stale = (bytes[4] & SNAPSHOT_FLAG_STALE) != 0U,
            .temperature_deci_c = (int16_t)get_u16_le(&bytes[5]),
            .relative_humidity_percent = bytes[7],
            .weather_code = get_u16_le(&bytes[8]),
            .observed_at_unix_s = get_u32_le(&bytes[10]),
        },
        .weather_visual = size >= OFFLINE_DATA_V2_ENCODED_SIZE &&
                                  get_u16_le(&bytes[0]) >= UINT16_C(3)
                              ? (offline_weather_visual_t){
                                    .available = (bytes[3] & SNAPSHOT_VISUAL_AVAILABLE) != 0U,
                                    .is_day = (bytes[3] & SNAPSHOT_VISUAL_DAY) != 0U,
                                }
                              : (offline_weather_visual_t){0},
        .market = {
            .available = (bytes[14] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
            .stale = (bytes[14] & SNAPSHOT_FLAG_STALE) != 0U,
            .bitcoin_usd_cents = get_u32_le(&bytes[15]),
            .change_24h_basis_points = (int16_t)get_u16_le(&bytes[19]),
            .observed_at_unix_s = get_u32_le(&bytes[21]),
        },
        .exchange = size >= OFFLINE_DATA_V2_ENCODED_SIZE
                        ? (offline_exchange_data_t){
                              .available = (bytes[25] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
                              .stale = (bytes[25] & SNAPSHOT_FLAG_STALE) != 0U,
                              .usd_brl_ten_thousandths = get_u32_le(&bytes[26]),
                              .observed_at_unix_s = get_u32_le(&bytes[30]),
                          }
                        : (offline_exchange_data_t){0},
    };
    if (size >= OFFLINE_DATA_V4_ENCODED_SIZE) {
        candidate.weather.apparent_temperature_available = (bytes[34] & DETAIL_APPARENT) != 0U;
        candidate.weather.apparent_temperature_deci_c = (int16_t)get_u16_le(&bytes[35]);
        candidate.weather.wind_speed_available = (bytes[34] & DETAIL_WIND) != 0U;
        candidate.weather.wind_speed_deci_kmh = get_u16_le(&bytes[37]);
        candidate.weather.uv_index_available = (bytes[34] & DETAIL_UV) != 0U;
        candidate.weather.uv_index_deci = get_u16_le(&bytes[39]);
        candidate.market.high_24h_available = (bytes[41] & DETAIL_HIGH) != 0U;
        candidate.market.high_24h_usd_cents = get_u32_le(&bytes[42]);
        candidate.market.low_24h_available = (bytes[41] & DETAIL_LOW) != 0U;
        candidate.market.low_24h_usd_cents = get_u32_le(&bytes[46]);
        candidate.market.volume_24h_available = (bytes[41] & DETAIL_VOLUME) != 0U;
        candidate.market.volume_24h_usd_cents = get_u64_le(&bytes[50]);
        candidate.market.history_count = bytes[58];
        for (size_t index = 0; index < OFFLINE_MARKET_HISTORY_MAX; ++index)
            candidate.market.history_usd_cents[index] = get_u32_le(&bytes[59U + index * 4U]);
        candidate.exchange.change_available = (bytes[155] & DETAIL_CHANGE) != 0U;
        candidate.exchange.change_basis_points = (int16_t)get_u16_le(&bytes[156]);
        candidate.ibovespa.available = (bytes[158] & SNAPSHOT_FLAG_AVAILABLE) != 0U;
        candidate.ibovespa.stale = (bytes[158] & SNAPSHOT_FLAG_STALE) != 0U;
        candidate.ibovespa.change_available = (bytes[158] & DETAIL_CHANGE) != 0U;
        candidate.ibovespa.value_centi_points = get_u32_le(&bytes[159]);
        candidate.ibovespa.change_basis_points = (int16_t)get_u16_le(&bytes[163]);
        candidate.ibovespa.observed_at_unix_s = get_u32_le(&bytes[165]);
    }
    if (size >= OFFLINE_DATA_V5_ENCODED_SIZE) {
        candidate.weather.wind_direction_available = (bytes[169] & UINT8_C(0x01)) != 0U;
        candidate.weather.forecast_available = (bytes[169] & UINT8_C(0x02)) != 0U;
        candidate.weather.wind_direction_degrees = get_u16_le(&bytes[170]);
        candidate.weather.utc_offset_seconds = (int32_t)get_u32_le(&bytes[172]);
        candidate.weather.today_temperature_max_deci_c = (int16_t)get_u16_le(&bytes[176]);
        candidate.weather.today_temperature_min_deci_c = (int16_t)get_u16_le(&bytes[178]);
        candidate.weather.today_precipitation_sum_deci_mm = get_u16_le(&bytes[180]);
        candidate.weather.sunrise_unix_s = get_u32_le(&bytes[182]);
        candidate.weather.sunset_unix_s = get_u32_le(&bytes[186]);
        size_t offset = 190U;
        for (size_t index = 0U; index < OFFLINE_WEATHER_HOURLY_MAX; ++index) {
            candidate.weather.hourly[index].time_unix_s = get_u32_le(&bytes[offset]);
            candidate.weather.hourly[index].temperature_deci_c =
                (int16_t)get_u16_le(&bytes[offset + 4U]);
            candidate.weather.hourly[index].weather_code = get_u16_le(&bytes[offset + 6U]);
            candidate.weather.hourly[index].precipitation_probability_percent = bytes[offset + 8U];
            offset += 9U;
        }
        for (size_t index = 0U; index < OFFLINE_WEATHER_DAILY_MAX; ++index) {
            candidate.weather.daily[index].time_unix_s = get_u32_le(&bytes[offset]);
            candidate.weather.daily[index].temperature_min_deci_c =
                (int16_t)get_u16_le(&bytes[offset + 4U]);
            candidate.weather.daily[index].temperature_max_deci_c =
                (int16_t)get_u16_le(&bytes[offset + 6U]);
            candidate.weather.daily[index].weather_code = get_u16_le(&bytes[offset + 8U]);
            offset += 10U;
        }
    }
    if (size >= OFFLINE_DATA_V6_ENCODED_SIZE) {
        size_t offset = OFFLINE_DATA_V5_ENCODED_SIZE;
        for (size_t index = 0U; index < OFFLINE_MARKET_ALTCOIN_COUNT; ++index) {
            candidate.altcoins[index] = (offline_altcoin_data_t){
                .available = (bytes[offset] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
                .stale = (bytes[offset] & SNAPSHOT_FLAG_STALE) != 0U,
                .usd_micros = get_u64_le(&bytes[offset + 1U]),
                .change_24h_basis_points = (int16_t)get_u16_le(&bytes[offset + 9U]),
                .observed_at_unix_s = get_u32_le(&bytes[offset + 11U]),
            };
            if (!flags_are_valid(bytes[offset])) return false;
            offset += 15U;
        }
        if (!flags_are_valid(bytes[offset])) return false;
        candidate.fear_greed = (offline_fear_greed_data_t){
            .available = (bytes[offset] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
            .stale = (bytes[offset] & SNAPSHOT_FLAG_STALE) != 0U,
            .value = bytes[offset + 1U],
            .classification = bytes[offset + 2U],
            .observed_at_unix_s = size == OFFLINE_DATA_V6_ENCODED_SIZE
                ? (uint32_t)bytes[offset + 3U] |
                  ((uint32_t)bytes[offset + 4U] << 8U) |
                  ((uint32_t)bytes[offset + 5U] << 16U)
                : get_u32_le(&bytes[offset + 3U]),
        };
        if (size == OFFLINE_DATA_ENCODED_SIZE) {
            offset = OFFLINE_DATA_V6_FIELDS_SIZE;
            offline_index_data_t *const indices[] = {&candidate.sp500, &candidate.nasdaq};
            for (size_t index = 0U; index < sizeof(indices) / sizeof(indices[0]); ++index) {
                if (!flags_are_valid(bytes[offset] & UINT8_C(3)) ||
                    (bytes[offset] & ~UINT8_C(7)) != 0U) return false;
                *indices[index] = (offline_index_data_t){
                    .available = (bytes[offset] & SNAPSHOT_FLAG_AVAILABLE) != 0U,
                    .stale = (bytes[offset] & SNAPSHOT_FLAG_STALE) != 0U,
                    .change_available = (bytes[offset] & DETAIL_CHANGE) != 0U,
                    .value_centi_points = get_u32_le(&bytes[offset + 1U]),
                    .change_basis_points = (int16_t)get_u16_le(&bytes[offset + 5U]),
                    .observed_at_unix_s = get_u32_le(&bytes[offset + 7U]),
                };
                offset += 11U;
            }
        }
    }
    if (!offline_data_snapshot_is_valid(&candidate)) {
        return false;
    }
    *out_snapshot = candidate;
    return true;
}
