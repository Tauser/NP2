#include <assert.h>

#include "data_refresh_scheduler.h"

int main(void)
{
    data_refresh_scheduler_t scheduler = {0};
    data_refresh_scheduler_init(&scheduler);
    data_refresh_domain_t domain = DATA_REFRESH_DOMAIN_COUNT;

    /* Cold boot: all are due, but only one is issued each scheduler turn. */
    assert(data_refresh_scheduler_take_due(&scheduler, 1LL, &domain));
    assert(domain == DATA_REFRESH_DOMAIN_BITCOIN);
    data_refresh_scheduler_note_result(&scheduler, domain, 1LL, true);
    assert(data_refresh_scheduler_take_due(&scheduler, 1LL, &domain));
    assert(domain == DATA_REFRESH_DOMAIN_WEATHER);
    data_refresh_scheduler_note_result(&scheduler, domain, 1LL, true);
    assert(data_refresh_scheduler_take_due(&scheduler, 1LL, &domain));
    assert(domain == DATA_REFRESH_DOMAIN_USD_BRL);
    data_refresh_scheduler_note_result(&scheduler, domain, 1LL, true);
    assert(!data_refresh_scheduler_take_due(&scheduler, 2LL, &domain));

    assert(data_refresh_scheduler_take_due(&scheduler,
                                           1LL + DATA_REFRESH_BITCOIN_INTERVAL_US, &domain));
    assert(domain == DATA_REFRESH_DOMAIN_BITCOIN);
    data_refresh_scheduler_note_result(&scheduler, domain,
                                       1LL + DATA_REFRESH_BITCOIN_INTERVAL_US, false);
    assert(!data_refresh_scheduler_take_due(&scheduler,
                                            1LL + DATA_REFRESH_BITCOIN_INTERVAL_US +
                                                DATA_REFRESH_BITCOIN_RETRY_US - 1LL,
                                            &domain));
    assert(data_refresh_scheduler_take_due(&scheduler,
                                           1LL + DATA_REFRESH_BITCOIN_INTERVAL_US +
                                               DATA_REFRESH_BITCOIN_RETRY_US,
                                           &domain));
    assert(domain == DATA_REFRESH_DOMAIN_BITCOIN);
    return 0;
}
