#include "data_refresh_scheduler.h"

#include <stddef.h>

static int64_t interval_for(data_refresh_domain_t domain, bool success)
{
    switch (domain) {
    case DATA_REFRESH_DOMAIN_BITCOIN:
        return success ? DATA_REFRESH_BITCOIN_INTERVAL_US : DATA_REFRESH_BITCOIN_RETRY_US;
    case DATA_REFRESH_DOMAIN_WEATHER:
        return success ? DATA_REFRESH_WEATHER_INTERVAL_US : DATA_REFRESH_WEATHER_RETRY_US;
    case DATA_REFRESH_DOMAIN_USD_BRL:
        return success ? DATA_REFRESH_USD_BRL_INTERVAL_US : DATA_REFRESH_USD_BRL_RETRY_US;
    case DATA_REFRESH_DOMAIN_FEAR_GREED:
        return success ? DATA_REFRESH_FEAR_GREED_INTERVAL_US : DATA_REFRESH_FEAR_GREED_RETRY_US;
    case DATA_REFRESH_DOMAIN_MARKET_INDICES:
        return success ? DATA_REFRESH_MARKET_INDICES_INTERVAL_US : DATA_REFRESH_MARKET_INDICES_RETRY_US;
    case DATA_REFRESH_DOMAIN_COUNT:
    default:
        return 0LL;
    }
}

void data_refresh_scheduler_init(data_refresh_scheduler_t *scheduler)
{
    if (scheduler == NULL) return;
    for (size_t index = 0U; index < DATA_REFRESH_DOMAIN_COUNT; ++index) {
        scheduler->next_due_us[index] = 0LL;
    }
    /* Bitcoin is checked first at a cold start, then fairness rotates. */
    scheduler->next_domain = DATA_REFRESH_DOMAIN_BITCOIN;
}

void data_refresh_scheduler_mark_all_due(data_refresh_scheduler_t *scheduler)
{
    if (scheduler == NULL) return;
    for (size_t index = 0U; index < DATA_REFRESH_DOMAIN_COUNT; ++index) {
        scheduler->next_due_us[index] = 0LL;
    }
}

bool data_refresh_scheduler_take_due(data_refresh_scheduler_t *scheduler, int64_t now_us,
                                     data_refresh_domain_t *out_domain)
{
    if (scheduler == NULL || out_domain == NULL || now_us < 0LL) return false;
    for (size_t offset = 0U; offset < DATA_REFRESH_DOMAIN_COUNT; ++offset) {
        const data_refresh_domain_t domain =
            (data_refresh_domain_t)((scheduler->next_domain + offset) % DATA_REFRESH_DOMAIN_COUNT);
        if (scheduler->next_due_us[domain] > now_us) continue;
        *out_domain = domain;
        scheduler->next_domain =
            (data_refresh_domain_t)((domain + 1U) % DATA_REFRESH_DOMAIN_COUNT);
        return true;
    }
    return false;
}

void data_refresh_scheduler_note_result(data_refresh_scheduler_t *scheduler,
                                        data_refresh_domain_t domain, int64_t now_us,
                                        bool success)
{
    if (scheduler == NULL || domain >= DATA_REFRESH_DOMAIN_COUNT || now_us < 0LL) return;
    scheduler->next_due_us[domain] = now_us + interval_for(domain, success);
}
