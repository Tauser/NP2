#include <assert.h>
#include <string.h>

#include "offline_data_provider.h"

int main(void)
{
    static const uint8_t weather[] =
        "{\"current\":{\"temperature_2m\":23.7,\"relative_humidity_2m\":81,\"weather_code\":61}}";
    static const uint8_t market[] =
        "{\"bitcoin\":{\"usd\":112345.67,\"usd_24h_change\":-1.25,\"last_updated_at\":1760000000}}";
    offline_weather_data_t parsed_weather = {0};
    offline_market_data_t parsed_market = {0};
    offline_exchange_data_t parsed_exchange = {0};

    assert(offline_open_meteo_parse_current(weather, strlen((const char *)weather), 1760000010,
                                             &parsed_weather) == OFFLINE_PROVIDER_OK);
    assert(parsed_weather.temperature_deci_c == 237 && parsed_weather.relative_humidity_percent == 81);
    assert(offline_coingecko_parse_bitcoin(market, strlen((const char *)market), &parsed_market) ==
           OFFLINE_PROVIDER_OK);
    assert(parsed_market.bitcoin_usd_cents == 11234567U &&
           parsed_market.change_24h_basis_points == -125);
    static const uint8_t exchange[] = "[{\"data\":\"22/09/2026\",\"valor\":\"5.1161\"}]";
    assert(offline_bcb_parse_usd_brl(exchange, strlen((const char *)exchange), 1760000020,
                                     &parsed_exchange) == OFFLINE_PROVIDER_OK);
    assert(parsed_exchange.usd_brl_ten_thousandths == 51161U &&
           parsed_exchange.observed_at_unix_s == 1760000020U);
    static const uint8_t full_weather[] =
        "{\"current\":{\"temperature_2m\":23.7,\"relative_humidity_2m\":81,"
        "\"weather_code\":61,\"apparent_temperature\":25.4,"
        "\"wind_speed_10m\":11.2,\"uv_index\":6.3}}";
    assert(offline_open_meteo_parse_current(full_weather, strlen((const char *)full_weather),
                                             1760000010, &parsed_weather) == OFFLINE_PROVIDER_OK);
    assert(parsed_weather.apparent_temperature_available &&
           parsed_weather.apparent_temperature_deci_c == 254 &&
           parsed_weather.wind_speed_deci_kmh == 112 && parsed_weather.uv_index_deci == 63);
    static const uint8_t market_details[] =
        "[{\"id\":\"bitcoin\",\"current_price\":112345.67,"
        "\"price_change_percentage_24h\":-1.25,\"high_24h\":113000.12,"
        "\"low_24h\":110000.01,\"total_volume\":20400000000}]";
    assert(offline_coingecko_parse_bitcoin_market(market_details,
                                                  strlen((const char *)market_details),
                                                  1760000010, &parsed_market) == OFFLINE_PROVIDER_OK);
    assert(parsed_market.high_24h_available && parsed_market.low_24h_available &&
           parsed_market.volume_24h_available &&
           parsed_market.high_24h_usd_cents == 11300012U &&
           parsed_market.low_24h_usd_cents == 11000001U &&
           parsed_market.volume_24h_usd_cents == UINT64_C(2040000000000));
    static const uint8_t exchange_pair[] =
        "[{\"data\":\"21/09/2026\",\"valor\":\"5.1000\"},"
        "{\"data\":\"22/09/2026\",\"valor\":\"5.1161\"}]";
    assert(offline_bcb_parse_usd_brl(exchange_pair, strlen((const char *)exchange_pair),
                                     1760000020, &parsed_exchange) == OFFLINE_PROVIDER_OK);
    assert(parsed_exchange.change_available && parsed_exchange.change_basis_points == 32);
    static const uint8_t weather_with_units_first[] =
        "{\"current_units\":{\"temperature_2m\":\"C\"},\"current\":{\"weather_code\":3,"
        "\"relative_humidity_2m\":61,\"temperature_2m\":23.7}}";
    assert(offline_open_meteo_parse_current(weather_with_units_first,
                                             strlen((const char *)weather_with_units_first),
                                             1760000010, &parsed_weather) == OFFLINE_PROVIDER_OK);
    assert(parsed_weather.temperature_deci_c == 237);
    static const uint8_t ethereum_only[] =
        "{\"ethereum\":{\"usd\":2500,\"usd_24h_change\":1.2,\"last_updated_at\":1760000000}}";
    assert(offline_coingecko_parse_bitcoin(ethereum_only, strlen((const char *)ethereum_only),
                                           &parsed_market) == OFFLINE_PROVIDER_MALFORMED_RESPONSE);
    static const uint8_t truncated_weather[] =
        "{\"current\":{\"temperature_2m\":23.7,\"relative_humidity_2m\":81,\"weather_code\":61";
    assert(offline_open_meteo_parse_current(truncated_weather,
                                             strlen((const char *)truncated_weather), 1760000010,
                                             &parsed_weather) == OFFLINE_PROVIDER_MALFORMED_RESPONSE);
    static const uint8_t decimal_humidity[] =
        "{\"current\":{\"temperature_2m\":23.7,\"relative_humidity_2m\":100.9,\"weather_code\":61}}";
    assert(offline_open_meteo_parse_current(decimal_humidity,
                                             strlen((const char *)decimal_humidity), 1760000010,
                                             &parsed_weather) == OFFLINE_PROVIDER_MALFORMED_RESPONSE);
    static const uint8_t duplicate_field[] =
        "{\"bitcoin\":{\"usd\":1,\"usd\":2,\"usd_24h_change\":0,\"last_updated_at\":1760000000}}";
    assert(offline_coingecko_parse_bitcoin(duplicate_field, strlen((const char *)duplicate_field),
                                           &parsed_market) == OFFLINE_PROVIDER_MALFORMED_RESPONSE);
    static const uint8_t escaped_unknown[] =
        "{\"metadata\":{\"label\":\"a\\nvalue\",\"items\":[1,true,null]},"
        "\"bitcoin\":{\"last_updated_at\":1760000000,\"usd_24h_change\":-1.255,\"usd\":1.239}}";
    assert(offline_coingecko_parse_bitcoin(escaped_unknown, strlen((const char *)escaped_unknown),
                                           &parsed_market) == OFFLINE_PROVIDER_OK);
    assert(parsed_market.bitcoin_usd_cents == 124U &&
           parsed_market.change_24h_basis_points == -126);
    assert(offline_open_meteo_parse_current(weather, OFFLINE_WEATHER_MAX_BODY_BYTES + 1U,
                                             1760000010, &parsed_weather) ==
           OFFLINE_PROVIDER_BODY_TOO_LARGE);
    static const uint8_t negative_price[] =
        "{\"bitcoin\":{\"usd\":-1,\"usd_24h_change\":0,\"last_updated_at\":1760000000}}";
    assert(offline_coingecko_parse_bitcoin(negative_price, strlen((const char *)negative_price),
                                           &parsed_market) == OFFLINE_PROVIDER_OUT_OF_RANGE);
    static const uint8_t malformed_exchange[] = "[{\"data\":\"2026-09-22\",\"valor\":\"5.1161\"}]";
    assert(offline_bcb_parse_usd_brl(malformed_exchange,
                                     strlen((const char *)malformed_exchange), 1760000020,
                                     &parsed_exchange) == OFFLINE_PROVIDER_MALFORMED_RESPONSE);
    return 0;
}
