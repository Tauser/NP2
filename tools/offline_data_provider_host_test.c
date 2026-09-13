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

    assert(offline_open_meteo_parse_current(weather, strlen((const char *)weather), 1760000010,
                                             &parsed_weather) == OFFLINE_PROVIDER_OK);
    assert(parsed_weather.temperature_deci_c == 237 && parsed_weather.relative_humidity_percent == 81);
    assert(offline_coingecko_parse_bitcoin(market, strlen((const char *)market), &parsed_market) ==
           OFFLINE_PROVIDER_OK);
    assert(parsed_market.bitcoin_usd_cents == 11234567U &&
           parsed_market.change_24h_basis_points == -125);
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
    return 0;
}
