/* Bounded adapters from provider JSON into the offline product model. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "offline_data_model.h"

#define OFFLINE_WEATHER_MAX_BODY_BYTES 768U
#define OFFLINE_MARKET_MAX_BODY_BYTES 768U

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

/* Maps only the documented CoinGecko simple-price bitcoin fields. */
offline_provider_result_t offline_coingecko_parse_bitcoin(const uint8_t *body, size_t body_size,
                                                          offline_market_data_t *out_market);
