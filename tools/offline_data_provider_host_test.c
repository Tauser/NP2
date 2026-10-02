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
    static const uint8_t forecast_weather[] =
        "{\"utc_offset_seconds\":-10800,\"current\":{\"temperature_2m\":23.7,"
        "\"relative_humidity_2m\":81,\"weather_code\":3,\"apparent_temperature\":25.4,"
        "\"wind_speed_10m\":11.2,\"wind_direction_10m\":45,\"uv_index\":6.3},"
        "\"hourly\":{\"time\":[1760000400,1760004000,1760007600,1760011200,1760014800],"
        "\"temperature_2m\":[24,25,24,22,20],\"weather_code\":[3,2,3,61,61],"
        "\"precipitation_probability\":[0,5,30,80,20]},\"daily\":{"
        "\"time\":[1759968000,1760054400,1760140800,1760227200,1760313600],"
        "\"weather_code\":[3,2,61,3,0],\"temperature_2m_max\":[31,28,24,23,26],"
        "\"temperature_2m_min\":[18,18,17,16,17],\"sunrise\":[1759990000,1760076400,1760162800,1760249200,1760335600],"
        "\"sunset\":[1760034000,1760120400,1760206800,1760293200,1760379600],"
        "\"precipitation_sum\":[0,0,1.5,4.2,0]}}";
    assert(offline_open_meteo_parse_forecast(forecast_weather,
                                             strlen((const char *)forecast_weather), 1760000010,
                                             &parsed_weather) == OFFLINE_PROVIDER_OK);
    assert(parsed_weather.forecast_available && parsed_weather.wind_direction_available &&
           parsed_weather.wind_direction_degrees == 45U &&
           parsed_weather.today_temperature_max_deci_c == 310 &&
           parsed_weather.today_temperature_min_deci_c == 180 &&
           parsed_weather.hourly[3].precipitation_probability_percent == 80U &&
           parsed_weather.daily[1].weather_code == 61U &&
           parsed_weather.daily[1].temperature_min_deci_c == 170);
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
    static const uint8_t market_overview[] =
        "[{\"id\":\"ripple\",\"current_price\":0.621,"
        "\"price_change_percentage_24h\":0.46},{\"id\":\"bitcoin\","
        "\"current_price\":103842.42,\"price_change_percentage_24h\":2.41,"
        "\"high_24h\":104521,\"low_24h\":100841,\"total_volume\":28400000000},"
        "{\"id\":\"ethereum\",\"current_price\":3842,\"price_change_percentage_24h\":1.32},"
        "{\"id\":\"binancecoin\",\"current_price\":578.23,\"price_change_percentage_24h\":-0.84},"
        "{\"id\":\"solana\",\"current_price\":218.41,\"price_change_percentage_24h\":3.12}]";
    offline_altcoin_data_t altcoins[OFFLINE_MARKET_ALTCOIN_COUNT] = {0};
    assert(offline_coingecko_parse_market_overview(market_overview,
        strlen((const char *)market_overview), 1760000050, &parsed_market, altcoins) ==
        OFFLINE_PROVIDER_OK);
    assert(parsed_market.bitcoin_usd_cents == UINT32_C(10384242) &&
           parsed_market.high_24h_available && parsed_market.low_24h_available &&
           parsed_market.volume_24h_available);
    assert(altcoins[0].available && altcoins[0].usd_micros == UINT64_C(3842000000) &&
           altcoins[1].available && altcoins[1].usd_micros == UINT64_C(218410000) &&
           altcoins[2].change_24h_basis_points == -84 &&
           altcoins[3].usd_micros == UINT64_C(621000));
    static const uint8_t fear_greed[] =
        "{\"name\":\"Fear and Greed Index\",\"data\":[{\"value\":\"72\","
        "\"value_classification\":\"Greed\",\"timestamp\":\"1760000000\"}],"
        "\"metadata\":{\"error\":null}}";
    offline_fear_greed_data_t parsed_fear = {0};
    assert(offline_alternative_me_parse_fear_greed(fear_greed, strlen((const char *)fear_greed),
        1760000060, &parsed_fear) == OFFLINE_PROVIDER_OK);
    assert(parsed_fear.available && parsed_fear.value == 72U &&
           parsed_fear.classification == 3U && parsed_fear.observed_at_unix_s == 1760000060U);
    static const uint8_t exchange_pair[] =
        "[{\"data\":\"21/09/2026\",\"valor\":\"5.1000\"},"
        "{\"data\":\"22/09/2026\",\"valor\":\"5.1161\"}]";
    assert(offline_bcb_parse_usd_brl(exchange_pair, strlen((const char *)exchange_pair),
                                     1760000020, &parsed_exchange) == OFFLINE_PROVIDER_OK);
    assert(parsed_exchange.change_available && parsed_exchange.change_basis_points == 32);
    static const uint8_t brapi_ibovespa[] =
        "{\"results\":[{\"symbol\":\"^BVSP\",\"regularMarketPrice\":138265.5,"
        "\"regularMarketChangePercent\":-0.48}]}";
    offline_ibovespa_data_t parsed_ibovespa = {0};
    offline_index_data_t parsed_sp500 = {0}, parsed_nasdaq = {0};
    assert(offline_brapi_parse_market_indices(brapi_ibovespa,
        strlen((const char *)brapi_ibovespa), 1760000020, &parsed_ibovespa,
        &parsed_sp500, &parsed_nasdaq) == OFFLINE_PROVIDER_OK);
    assert(parsed_ibovespa.available && parsed_ibovespa.value_centi_points == 13826550U &&
           parsed_ibovespa.change_basis_points == -48 && !parsed_sp500.available &&
           !parsed_nasdaq.available);
    static const uint8_t brapi_sp500[] =
        "{\"results\":[{\"symbol\":\"^GSPC\",\"regularMarketPrice\":5745.12,"
        "\"regularMarketChangePercent\":0.33}]}";
    assert(offline_brapi_parse_market_indices(brapi_sp500,
        strlen((const char *)brapi_sp500), 1760000020, &parsed_ibovespa,
        &parsed_sp500, &parsed_nasdaq) == OFFLINE_PROVIDER_OK);
    assert(!parsed_ibovespa.available && parsed_sp500.available &&
           parsed_sp500.value_centi_points == 574512U &&
           parsed_sp500.change_basis_points == 33 && !parsed_nasdaq.available);
    static const uint8_t brapi_nasdaq[] =
        "{\"results\":[{\"symbol\":\"^IXIC\",\"regularMarketPrice\":18279.42,"
        "\"regularMarketChangePercent\":0.28}]}";
    assert(offline_brapi_parse_market_indices(brapi_nasdaq,
        strlen((const char *)brapi_nasdaq), 1760000020, &parsed_ibovespa,
        &parsed_sp500, &parsed_nasdaq) == OFFLINE_PROVIDER_OK);
    assert(!parsed_ibovespa.available && !parsed_sp500.available &&
           parsed_nasdaq.available && parsed_nasdaq.value_centi_points == 1827942U &&
           parsed_nasdaq.change_basis_points == 28);
    static const uint8_t brapi_partial[] =
        "{\"results\":[{\"symbol\":\"^BVSP\",\"regularMarketPrice\":138265.5}]}";
    assert(offline_brapi_parse_market_indices(brapi_partial,
        strlen((const char *)brapi_partial), 1760000020, &parsed_ibovespa,
        &parsed_sp500, &parsed_nasdaq) == OFFLINE_PROVIDER_OK);
    assert(parsed_ibovespa.available && !parsed_sp500.available && !parsed_nasdaq.available);
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
