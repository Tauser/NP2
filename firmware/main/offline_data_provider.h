/* Bounded adapters from provider JSON into the offline product model. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "offline_data_model.h"

#define OFFLINE_WEATHER_MAX_BODY_BYTES 2048U
#define OFFLINE_MARKET_MAX_BODY_BYTES 4096U
#define OFFLINE_EXCHANGE_MAX_BODY_BYTES 512U

typedef enum {
    OFFLINE_PROVIDER_OK = 0,
    OFFLINE_PROVIDER_INVALID_ARGUMENT,
    OFFLINE_PROVIDER_BODY_TOO_LARGE,
    OFFLINE_PROVIDER_MALFORMED_RESPONSE,
    OFFLINE_PROVIDER_OUT_OF_RANGE,
} offline_provider_result_t;

/* Maps only the documented Open-Meteo `current` fields. observed_at_unix_s is
 * supplied by the request owner after time has been validated. */
offline_provider_result_t offline_open_meteo_parse_current(const uint8_t *body, size_t body_size,
                                                            uint32_t observed_at_unix_s,
                                                            offline_weather_data_t *out_weather);

/* Parses the bounded current/hourly/daily forecast that supplies every field
 * of the Clima screen. The caller provides the trusted fetch time. */
offline_provider_result_t offline_open_meteo_parse_forecast(const uint8_t *body, size_t body_size,
                                                             uint32_t observed_at_unix_s,
                                                             offline_weather_data_t *out_weather);

/* Maps only the documented CoinGecko simple-price bitcoin fields. */
offline_provider_result_t offline_coingecko_parse_bitcoin(const uint8_t *body, size_t body_size,
                                                          offline_market_data_t *out_market);

/* CoinGecko /coins/markets single-Bitcoin result, including the 24 h range and
 * USD trading volume. Timestamp is the trusted local fetch time. */
offline_provider_result_t offline_coingecko_parse_bitcoin_market(const uint8_t *body,
                                                                 size_t body_size,
                                                                 uint32_t observed_at_unix_s,
                                                                 offline_market_data_t *out_market);

/* Parses one bounded CoinGecko market list and maps BTC plus the four screen
 * altcoins by their documented ids, independent of response order. */
offline_provider_result_t offline_coingecko_parse_market_overview(
    const uint8_t *body, size_t body_size, uint32_t observed_at_unix_s,
    offline_market_data_t *out_bitcoin,
    offline_altcoin_data_t out_altcoins[OFFLINE_MARKET_ALTCOIN_COUNT]);

/* Alternative.me Fear & Greed endpoint, classified and timestamped locally. */
offline_provider_result_t offline_alternative_me_parse_fear_greed(
    const uint8_t *body, size_t body_size, uint32_t observed_at_unix_s,
    offline_fear_greed_data_t *out_data);

/* Parses the latest BCB SGS series 1 (USD sale/PTAX) entry. The source date
 * is validated; observed_at_unix_s is the trusted local fetch time. */
offline_provider_result_t offline_bcb_parse_usd_brl(const uint8_t *body, size_t body_size,
                                                     uint32_t observed_at_unix_s,
                                                     offline_exchange_data_t *out_exchange);

/* Maps only matching symbols and documented Brapi quote fields. Missing or
 * unsupported symbols remain unavailable; timestamps come from the caller. */
offline_provider_result_t offline_brapi_parse_market_indices(
    const uint8_t *body, size_t body_size, uint32_t observed_at_unix_s,
    offline_ibovespa_data_t *out_ibovespa, offline_index_data_t *out_sp500,
    offline_index_data_t *out_nasdaq);
