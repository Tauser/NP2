#include "offline_data_provider.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define JSON_MAX_DEPTH 8U

typedef struct { const uint8_t *cursor; const uint8_t *end; } json_reader_t;
typedef struct { const uint8_t *bytes; size_t length; bool plain; } json_string_t;
typedef struct { const uint8_t *bytes; size_t length; } json_number_t;

static bool is_space(uint8_t value) { return value == ' ' || value == '\t' || value == '\r' || value == '\n'; }
static bool is_digit(uint8_t value) { return value >= '0' && value <= '9'; }
static bool is_hex(uint8_t value) { return is_digit(value) || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F'); }

static void skip_space(json_reader_t *reader)
{
    while (reader->cursor < reader->end && is_space(*reader->cursor)) ++reader->cursor;
}

static bool consume(json_reader_t *reader, uint8_t expected)
{
    skip_space(reader);
    if (reader->cursor >= reader->end || *reader->cursor != expected) return false;
    ++reader->cursor;
    return true;
}

static bool read_string(json_reader_t *reader, json_string_t *out_string)
{
    skip_space(reader);
    if (reader->cursor >= reader->end || *reader->cursor != '"') return false;
    ++reader->cursor;
    const uint8_t *const start = reader->cursor;
    bool plain = true;
    while (reader->cursor < reader->end) {
        const uint8_t value = *reader->cursor++;
        if (value == '"') {
            *out_string = (json_string_t){start, (size_t)((reader->cursor - 1) - start), plain};
            return true;
        }
        if (value < UINT8_C(0x20)) return false;
        if (value != '\\') continue;
        plain = false;
        if (reader->cursor >= reader->end) return false;
        const uint8_t escaped = *reader->cursor++;
        if (escaped == 'u') {
            for (size_t index = 0; index < 4U; ++index) {
                if (reader->cursor >= reader->end || !is_hex(*reader->cursor++)) return false;
            }
        } else if (escaped != '"' && escaped != '\\' && escaped != '/' && escaped != 'b' &&
                   escaped != 'f' && escaped != 'n' && escaped != 'r' && escaped != 't') {
            return false;
        }
    }
    return false;
}

static bool string_equals(const json_string_t *value, const char *expected)
{
    const size_t length = strlen(expected);
    return value->plain && value->length == length && memcmp(value->bytes, expected, length) == 0;
}

static bool read_number(json_reader_t *reader, json_number_t *out_number)
{
    skip_space(reader);
    const uint8_t *const start = reader->cursor;
    if (reader->cursor < reader->end && *reader->cursor == '-') ++reader->cursor;
    if (reader->cursor >= reader->end || !is_digit(*reader->cursor)) return false;
    if (*reader->cursor == '0') {
        ++reader->cursor;
        if (reader->cursor < reader->end && is_digit(*reader->cursor)) return false;
    } else {
        while (reader->cursor < reader->end && is_digit(*reader->cursor)) ++reader->cursor;
    }
    if (reader->cursor < reader->end && *reader->cursor == '.') {
        ++reader->cursor;
        if (reader->cursor >= reader->end || !is_digit(*reader->cursor)) return false;
        while (reader->cursor < reader->end && is_digit(*reader->cursor)) ++reader->cursor;
    }
    if (reader->cursor < reader->end && (*reader->cursor == 'e' || *reader->cursor == 'E')) {
        ++reader->cursor;
        if (reader->cursor < reader->end && (*reader->cursor == '+' || *reader->cursor == '-')) ++reader->cursor;
        if (reader->cursor >= reader->end || !is_digit(*reader->cursor)) return false;
        while (reader->cursor < reader->end && is_digit(*reader->cursor)) ++reader->cursor;
    }
    *out_number = (json_number_t){start, (size_t)(reader->cursor - start)};
    return true;
}

static bool skip_value(json_reader_t *reader, unsigned int depth);

static bool skip_compound(json_reader_t *reader, uint8_t open, uint8_t close, unsigned int depth)
{
    if (depth >= JSON_MAX_DEPTH || !consume(reader, open)) return false;
    skip_space(reader);
    if (reader->cursor < reader->end && *reader->cursor == close) { ++reader->cursor; return true; }
    for (;;) {
        if (open == '{') {
            json_string_t key = {0};
            if (!read_string(reader, &key) || !consume(reader, ':')) return false;
        }
        if (!skip_value(reader, depth + 1U)) return false;
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == close) { ++reader->cursor; return true; }
        if (reader->cursor >= reader->end || *reader->cursor++ != ',') return false;
    }
}

static bool skip_value(json_reader_t *reader, unsigned int depth)
{
    skip_space(reader);
    if (reader->cursor >= reader->end) return false;
    if (*reader->cursor == '{') return skip_compound(reader, '{', '}', depth);
    if (*reader->cursor == '[') return skip_compound(reader, '[', ']', depth);
    if (*reader->cursor == '"') { json_string_t value = {0}; return read_string(reader, &value); }
    if (*reader->cursor == '-' || is_digit(*reader->cursor)) { json_number_t value = {0}; return read_number(reader, &value); }
    static const char *const literals[] = {"true", "false", "null"};
    for (size_t index = 0; index < sizeof(literals) / sizeof(literals[0]); ++index) {
        const size_t length = strlen(literals[index]);
        if ((size_t)(reader->end - reader->cursor) >= length && memcmp(reader->cursor, literals[index], length) == 0) {
            reader->cursor += length;
            return true;
        }
    }
    return false;
}

static offline_provider_result_t parse_scaled_number(const json_number_t *number, uint8_t scale,
                                                      int64_t minimum, int64_t maximum,
                                                      int64_t *out_value)
{
    if (number == NULL || out_value == NULL || number->length == 0U) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    const uint8_t *cursor = number->bytes;
    const uint8_t *const end = cursor + number->length;
    bool negative = false;
    if (*cursor == '-') { negative = true; ++cursor; }
    int64_t integral = 0;
    while (cursor < end && is_digit(*cursor)) {
        const int64_t digit = (int64_t)(*cursor - '0');
        if (integral > (INT64_MAX - digit) / 10) return OFFLINE_PROVIDER_OUT_OF_RANGE;
        integral = integral * 10 + digit;
        ++cursor;
    }
    int64_t fraction = 0;
    size_t fraction_digits = 0U;
    bool round_up = false;
    if (cursor < end && *cursor == '.') {
        ++cursor;
        while (cursor < end && is_digit(*cursor)) {
            if (fraction_digits < scale) fraction = fraction * 10 + (int64_t)(*cursor - '0');
            else if (fraction_digits == scale) round_up = *cursor >= '5';
            ++fraction_digits;
            ++cursor;
        }
    }
    if (cursor != end) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    while (fraction_digits < scale) { fraction *= 10; ++fraction_digits; }
    int64_t multiplier = 1;
    for (uint8_t digit = 0U; digit < scale; ++digit) multiplier *= 10;
    if (integral > (INT64_MAX - fraction) / multiplier) return OFFLINE_PROVIDER_OUT_OF_RANGE;
    int64_t scaled = integral * multiplier + fraction;
    if (round_up) {
        if (scaled == INT64_MAX) return OFFLINE_PROVIDER_OUT_OF_RANGE;
        ++scaled;
    }
    const int64_t signed_scaled = negative ? -scaled : scaled;
    if (signed_scaled < minimum || signed_scaled > maximum) return OFFLINE_PROVIDER_OUT_OF_RANGE;
    *out_value = signed_scaled;
    return OFFLINE_PROVIDER_OK;
}

static offline_provider_result_t parse_u32(const json_number_t *number, uint32_t minimum,
                                           uint32_t maximum, uint32_t *out_value)
{
    if (number == NULL || out_value == NULL || number->length == 0U || number->bytes[0] == '-') return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    uint64_t parsed = 0U;
    for (size_t index = 0; index < number->length; ++index) {
        if (!is_digit(number->bytes[index])) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        const uint64_t digit = (uint64_t)(number->bytes[index] - '0');
        if (parsed > (UINT32_MAX - digit) / 10U) return OFFLINE_PROVIDER_OUT_OF_RANGE;
        parsed = parsed * 10U + digit;
    }
    if (parsed < minimum || parsed > maximum) return OFFLINE_PROVIDER_OUT_OF_RANGE;
    *out_value = (uint32_t)parsed;
    return OFFLINE_PROVIDER_OK;
}

static offline_provider_result_t parse_weather_object(json_reader_t *reader, offline_weather_data_t *out)
{
    if (!consume(reader, '{')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    bool have_temperature = false, have_humidity = false, have_code = false;
    bool have_apparent = false, have_wind = false, have_uv = false;
    int64_t temperature = 0, apparent = 0, wind = 0, uv = 0;
    uint32_t humidity = 0U, code = 0U;
    for (;;) {
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') { ++reader->cursor; break; }
        json_string_t key = {0};
        if (!read_string(reader, &key) || !consume(reader, ':')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        offline_provider_result_t result = OFFLINE_PROVIDER_OK;
        json_number_t number = {0};
        if (string_equals(&key, "temperature_2m")) {
            if (have_temperature || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_temperature = true; result = parse_scaled_number(&number, 1U, -1000, 1000, &temperature);
        } else if (string_equals(&key, "relative_humidity_2m")) {
            if (have_humidity || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_humidity = true; result = parse_u32(&number, 0U, 100U, &humidity);
        } else if (string_equals(&key, "weather_code")) {
            if (have_code || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_code = true; result = parse_u32(&number, 0U, UINT16_MAX, &code);
        } else if (string_equals(&key, "apparent_temperature")) {
            if (have_apparent || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_apparent = true; result = parse_scaled_number(&number, 1U, -1000, 1000, &apparent);
        } else if (string_equals(&key, "wind_speed_10m")) {
            if (have_wind || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_wind = true; result = parse_scaled_number(&number, 1U, 0, 2000, &wind);
        } else if (string_equals(&key, "uv_index")) {
            if (have_uv || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_uv = true; result = parse_scaled_number(&number, 1U, 0, 250, &uv);
        } else if (!skip_value(reader, 1U)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        if (result != OFFLINE_PROVIDER_OK) return result;
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') { ++reader->cursor; break; }
        if (reader->cursor >= reader->end || *reader->cursor++ != ',') return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    if (!have_temperature || !have_humidity || !have_code) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    out->temperature_deci_c = (int16_t)temperature;
    out->relative_humidity_percent = (uint8_t)humidity;
    out->weather_code = (uint16_t)code;
    out->apparent_temperature_available = have_apparent;
    out->apparent_temperature_deci_c = (int16_t)apparent;
    out->wind_speed_available = have_wind;
    out->wind_speed_deci_kmh = (uint16_t)wind;
    out->uv_index_available = have_uv;
    out->uv_index_deci = (uint16_t)uv;
    return OFFLINE_PROVIDER_OK;
}

static offline_provider_result_t parse_market_object(json_reader_t *reader, offline_market_data_t *out)
{
    if (!consume(reader, '{')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    bool have_usd = false, have_change = false, have_timestamp = false;
    int64_t usd = 0, change = 0; uint32_t timestamp = 0U;
    for (;;) {
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') { ++reader->cursor; break; }
        json_string_t key = {0};
        if (!read_string(reader, &key) || !consume(reader, ':')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        offline_provider_result_t result = OFFLINE_PROVIDER_OK;
        json_number_t number = {0};
        if (string_equals(&key, "usd")) {
            if (have_usd || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_usd = true; result = parse_scaled_number(&number, 2U, 0, UINT32_MAX, &usd);
        } else if (string_equals(&key, "usd_24h_change")) {
            if (have_change || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_change = true; result = parse_scaled_number(&number, 2U, INT16_MIN, INT16_MAX, &change);
        } else if (string_equals(&key, "last_updated_at")) {
            if (have_timestamp || !read_number(reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_timestamp = true; result = parse_u32(&number, UINT32_C(1735689600), UINT32_MAX, &timestamp);
        } else if (!skip_value(reader, 1U)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        if (result != OFFLINE_PROVIDER_OK) return result;
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') { ++reader->cursor; break; }
        if (reader->cursor >= reader->end || *reader->cursor++ != ',') return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    if (!have_usd || !have_change || !have_timestamp) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    out->bitcoin_usd_cents = (uint32_t)usd;
    out->change_24h_basis_points = (int16_t)change;
    out->observed_at_unix_s = timestamp;
    return OFFLINE_PROVIDER_OK;
}

static offline_provider_result_t parse_root(const uint8_t *body, size_t size, const char *target,
                                            bool weather, offline_weather_data_t *out_weather,
                                            offline_market_data_t *out_market)
{
    json_reader_t reader = {body, body + size};
    if (!consume(&reader, '{')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    bool found = false;
    for (;;) {
        skip_space(&reader);
        if (reader.cursor < reader.end && *reader.cursor == '}') { ++reader.cursor; break; }
        json_string_t key = {0};
        if (!read_string(&reader, &key) || !consume(&reader, ':')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        offline_provider_result_t result = OFFLINE_PROVIDER_OK;
        if (string_equals(&key, target)) {
            if (found) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            found = true;
            result = weather ? parse_weather_object(&reader, out_weather) : parse_market_object(&reader, out_market);
        } else if (!skip_value(&reader, 1U)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        if (result != OFFLINE_PROVIDER_OK) return result;
        skip_space(&reader);
        if (reader.cursor < reader.end && *reader.cursor == '}') { ++reader.cursor; break; }
        if (reader.cursor >= reader.end || *reader.cursor++ != ',') return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    skip_space(&reader);
    return found && reader.cursor == reader.end ? OFFLINE_PROVIDER_OK : OFFLINE_PROVIDER_MALFORMED_RESPONSE;
}

offline_provider_result_t offline_open_meteo_parse_current(const uint8_t *body, size_t body_size,
                                                            uint32_t observed_at_unix_s,
                                                            offline_weather_data_t *out_weather)
{
    if (body == NULL || out_weather == NULL || observed_at_unix_s == 0U) return OFFLINE_PROVIDER_INVALID_ARGUMENT;
    if (body_size == 0U || body_size > OFFLINE_WEATHER_MAX_BODY_BYTES) return OFFLINE_PROVIDER_BODY_TOO_LARGE;
    offline_weather_data_t candidate = {.available = true, .observed_at_unix_s = observed_at_unix_s};
    const offline_provider_result_t result = parse_root(body, body_size, "current", true, &candidate, NULL);
    if (result == OFFLINE_PROVIDER_OK) *out_weather = candidate;
    return result;
}

offline_provider_result_t offline_coingecko_parse_bitcoin(const uint8_t *body, size_t body_size,
                                                          offline_market_data_t *out_market)
{
    if (body == NULL || out_market == NULL) return OFFLINE_PROVIDER_INVALID_ARGUMENT;
    if (body_size == 0U || body_size > OFFLINE_MARKET_MAX_BODY_BYTES) return OFFLINE_PROVIDER_BODY_TOO_LARGE;
    offline_market_data_t candidate = {.available = true};
    const offline_provider_result_t result = parse_root(body, body_size, "bitcoin", false, NULL, &candidate);
    if (result == OFFLINE_PROVIDER_OK) *out_market = candidate;
    return result;
}

offline_provider_result_t offline_coingecko_parse_bitcoin_market(const uint8_t *body,
                                                                 size_t body_size,
                                                                 uint32_t observed_at_unix_s,
                                                                 offline_market_data_t *out_market)
{
    if (body == NULL || out_market == NULL || observed_at_unix_s == 0U)
        return OFFLINE_PROVIDER_INVALID_ARGUMENT;
    if (body_size == 0U || body_size > OFFLINE_MARKET_MAX_BODY_BYTES)
        return OFFLINE_PROVIDER_BODY_TOO_LARGE;
    json_reader_t reader = {body, body + body_size};
    if (!consume(&reader, '[') || !consume(&reader, '{'))
        return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    offline_market_data_t candidate = {.available = true, .observed_at_unix_s = observed_at_unix_s};
    bool id_seen = false, price_seen = false, change_seen = false;
    bool high_seen = false, low_seen = false, volume_seen = false;
    for (;;) {
        skip_space(&reader);
        if (reader.cursor < reader.end && *reader.cursor == '}') { ++reader.cursor; break; }
        json_string_t key = {0};
        if (!read_string(&reader, &key) || !consume(&reader, ':'))
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        json_number_t number = {0};
        int64_t scaled = 0;
        offline_provider_result_t result = OFFLINE_PROVIDER_OK;
        if (string_equals(&key, "id")) {
            json_string_t id = {0};
            if (id_seen || !read_string(&reader, &id) || !string_equals(&id, "bitcoin"))
                return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            id_seen = true;
        } else if (string_equals(&key, "current_price")) {
            if (price_seen || !read_number(&reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            price_seen = true;
            result = parse_scaled_number(&number, 2U, 1, UINT32_MAX, &scaled);
            candidate.bitcoin_usd_cents = (uint32_t)scaled;
        } else if (string_equals(&key, "price_change_percentage_24h")) {
            if (change_seen || !read_number(&reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            change_seen = true;
            result = parse_scaled_number(&number, 2U, INT16_MIN, INT16_MAX, &scaled);
            candidate.change_24h_basis_points = (int16_t)scaled;
        } else if (string_equals(&key, "high_24h")) {
            if (high_seen || !read_number(&reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            high_seen = true;
            result = parse_scaled_number(&number, 2U, 1, UINT32_MAX, &scaled);
            candidate.high_24h_available = true;
            candidate.high_24h_usd_cents = (uint32_t)scaled;
        } else if (string_equals(&key, "low_24h")) {
            if (low_seen || !read_number(&reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            low_seen = true;
            result = parse_scaled_number(&number, 2U, 1, UINT32_MAX, &scaled);
            candidate.low_24h_available = true;
            candidate.low_24h_usd_cents = (uint32_t)scaled;
        } else if (string_equals(&key, "total_volume")) {
            if (volume_seen || !read_number(&reader, &number)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            volume_seen = true;
            result = parse_scaled_number(&number, 2U, 0, INT64_MAX, &scaled);
            candidate.volume_24h_available = true;
            candidate.volume_24h_usd_cents = (uint64_t)scaled;
        } else if (!skip_value(&reader, 1U)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        if (result != OFFLINE_PROVIDER_OK) return result;
        skip_space(&reader);
        if (reader.cursor < reader.end && *reader.cursor == '}') { ++reader.cursor; break; }
        if (reader.cursor >= reader.end || *reader.cursor++ != ',')
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    if (!id_seen || !price_seen || !change_seen || !consume(&reader, ']'))
        return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    skip_space(&reader);
    if (reader.cursor != reader.end ||
        (high_seen && low_seen && candidate.high_24h_usd_cents < candidate.low_24h_usd_cents))
        return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    *out_market = candidate;
    return OFFLINE_PROVIDER_OK;
}

static bool bcb_date_is_valid(const json_string_t *date)
{
    if (date == NULL || !date->plain || date->length != 10U ||
        date->bytes[2] != '/' || date->bytes[5] != '/') {
        return false;
    }
    for (size_t index = 0U; index < date->length; ++index) {
        if (index != 2U && index != 5U && !is_digit(date->bytes[index])) return false;
    }
    const unsigned int day = (unsigned int)(date->bytes[0] - '0') * 10U +
                             (unsigned int)(date->bytes[1] - '0');
    const unsigned int month = (unsigned int)(date->bytes[3] - '0') * 10U +
                               (unsigned int)(date->bytes[4] - '0');
    const unsigned int year = (unsigned int)(date->bytes[6] - '0') * 1000U +
                              (unsigned int)(date->bytes[7] - '0') * 100U +
                              (unsigned int)(date->bytes[8] - '0') * 10U +
                              (unsigned int)(date->bytes[9] - '0');
    return day >= 1U && day <= 31U && month >= 1U && month <= 12U && year >= 2025U;
}

static offline_provider_result_t parse_bcb_record(json_reader_t *reader,
                                                   offline_exchange_data_t *out_exchange,
                                                   uint32_t *out_date_key)
{
    if (!consume(reader, '{')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    bool have_date = false, have_value = false;
    json_string_t date = {0}, value = {0};
    for (;;) {
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') {
            ++reader->cursor;
            break;
        }
        json_string_t key = {0};
        if (!read_string(reader, &key) || !consume(reader, ':')) {
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        }
        if (string_equals(&key, "data")) {
            if (have_date || !read_string(reader, &date)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_date = true;
        } else if (string_equals(&key, "valor")) {
            if (have_value || !read_string(reader, &value)) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
            have_value = true;
        } else if (!skip_value(reader, 1U)) {
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        }
        skip_space(reader);
        if (reader->cursor < reader->end && *reader->cursor == '}') {
            ++reader->cursor;
            break;
        }
        if (reader->cursor >= reader->end || *reader->cursor++ != ',') {
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        }
    }
    if (!have_date || !have_value || !bcb_date_is_valid(&date) || !value.plain) {
        return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    const json_number_t number = {.bytes = value.bytes, .length = value.length};
    int64_t scaled = 0;
    const offline_provider_result_t result =
        parse_scaled_number(&number, 4U, 1, UINT32_C(1000000), &scaled);
    if (result != OFFLINE_PROVIDER_OK) return result;
    out_exchange->usd_brl_ten_thousandths = (uint32_t)scaled;
    if (out_date_key != NULL) {
        *out_date_key = (unsigned int)(date.bytes[6] - '0') * 10000000U +
                        (unsigned int)(date.bytes[7] - '0') * 1000000U +
                        (unsigned int)(date.bytes[8] - '0') * 100000U +
                        (unsigned int)(date.bytes[9] - '0') * 10000U +
                        (unsigned int)(date.bytes[3] - '0') * 1000U +
                        (unsigned int)(date.bytes[4] - '0') * 100U +
                        (unsigned int)(date.bytes[0] - '0') * 10U +
                        (unsigned int)(date.bytes[1] - '0');
    }
    return OFFLINE_PROVIDER_OK;
}

offline_provider_result_t offline_bcb_parse_usd_brl(const uint8_t *body, size_t body_size,
                                                     uint32_t observed_at_unix_s,
                                                     offline_exchange_data_t *out_exchange)
{
    if (body == NULL || out_exchange == NULL || observed_at_unix_s == 0U) {
        return OFFLINE_PROVIDER_INVALID_ARGUMENT;
    }
    if (body_size == 0U || body_size > OFFLINE_EXCHANGE_MAX_BODY_BYTES) {
        return OFFLINE_PROVIDER_BODY_TOO_LARGE;
    }
    json_reader_t reader = {body, body + body_size};
    if (!consume(&reader, '[')) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    offline_exchange_data_t candidate = {
        .available = true,
        .observed_at_unix_s = observed_at_unix_s,
    };
    uint32_t first_date = 0U;
    offline_provider_result_t result = parse_bcb_record(&reader, &candidate, &first_date);
    if (result != OFFLINE_PROVIDER_OK) return result;
    skip_space(&reader);
    if (reader.cursor < reader.end && *reader.cursor == ',') {
        ++reader.cursor;
        offline_exchange_data_t second = {0};
        uint32_t second_date = 0U;
        result = parse_bcb_record(&reader, &second, &second_date);
        if (result != OFFLINE_PROVIDER_OK || second_date == first_date)
            return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
        const uint32_t latest = second_date > first_date
                                    ? second.usd_brl_ten_thousandths
                                    : candidate.usd_brl_ten_thousandths;
        const uint32_t previous = second_date > first_date
                                      ? candidate.usd_brl_ten_thousandths
                                      : second.usd_brl_ten_thousandths;
        const int64_t numerator = ((int64_t)latest - (int64_t)previous) * 10000LL;
        const int64_t rounded = numerator >= 0
                                    ? (numerator + (int64_t)previous / 2LL) / previous
                                    : (numerator - (int64_t)previous / 2LL) / previous;
        if (rounded < INT16_MIN || rounded > INT16_MAX) return OFFLINE_PROVIDER_OUT_OF_RANGE;
        candidate.usd_brl_ten_thousandths = latest;
        candidate.change_available = true;
        candidate.change_basis_points = (int16_t)rounded;
        skip_space(&reader);
    }
    if (reader.cursor >= reader.end || *reader.cursor++ != ']') {
        return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    }
    skip_space(&reader);
    if (reader.cursor != reader.end) return OFFLINE_PROVIDER_MALFORMED_RESPONSE;
    *out_exchange = candidate;
    return OFFLINE_PROVIDER_OK;
}
