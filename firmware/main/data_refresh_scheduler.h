/* Portable cadence and fairness policy for product data refreshes. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DATA_REFRESH_DOMAIN_BITCOIN = 0,
    DATA_REFRESH_DOMAIN_WEATHER,
    DATA_REFRESH_DOMAIN_USD_BRL,
    DATA_REFRESH_DOMAIN_FEAR_GREED,
    DATA_REFRESH_DOMAIN_MARKET_INDICES,
    DATA_REFRESH_DOMAIN_COUNT,
} data_refresh_domain_t;

typedef struct {
    int64_t next_due_us[DATA_REFRESH_DOMAIN_COUNT];
    data_refresh_domain_t next_domain;
} data_refresh_scheduler_t;

/* Product policy: 60 s BTC, 2 h weather, daily PTAX, and daily market sentiment/indexes. */
#define DATA_REFRESH_BITCOIN_INTERVAL_US (60LL * 1000LL * 1000LL)
#define DATA_REFRESH_WEATHER_INTERVAL_US (2LL * 60LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_USD_BRL_INTERVAL_US (24LL * 60LL * 60LL * 1000LL * 1000LL)

/* Each domain retries independently; no retry starts a parallel request. */
#define DATA_REFRESH_BITCOIN_RETRY_US (2LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_WEATHER_RETRY_US (15LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_USD_BRL_RETRY_US (60LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_FEAR_GREED_INTERVAL_US (24LL * 60LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_FEAR_GREED_RETRY_US (60LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_MARKET_INDICES_INTERVAL_US (24LL * 60LL * 60LL * 1000LL * 1000LL)
#define DATA_REFRESH_MARKET_INDICES_RETRY_US (60LL * 60LL * 1000LL * 1000LL)

void data_refresh_scheduler_init(data_refresh_scheduler_t *scheduler);
void data_refresh_scheduler_mark_all_due(data_refresh_scheduler_t *scheduler);
bool data_refresh_scheduler_take_due(data_refresh_scheduler_t *scheduler, int64_t now_us,
                                     data_refresh_domain_t *out_domain);
void data_refresh_scheduler_note_result(data_refresh_scheduler_t *scheduler,
                                        data_refresh_domain_t domain, int64_t now_us,
                                        bool success);

