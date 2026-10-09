#include <assert.h>
#include <stdio.h>
#include "coingecko_quota.h"

static void exercise_month(uint32_t month, unsigned int expected)
{
    coingecko_quota_t quota;
    assert(coingecko_quota_init(&quota, NULL));
    coingecko_quota_record_t proposal;
    uint32_t now_s = 1791400000U;
    int64_t now_us = 1LL;
    unsigned int attempts = 0U, writes = 0U;
    for (;;) {
        const coingecko_quota_result_t result = coingecko_quota_prepare(
            &quota, month, now_s, now_us, &proposal);
        if (result == COINGECKO_QUOTA_EXHAUSTED) break;
        if (result == COINGECKO_QUOTA_RESERVE) {
            assert(quota.remaining == 0U || month > quota.record.month);
            /* Preparing again before durable ACK never grants a request. */
            coingecko_quota_record_t again;
            assert(coingecko_quota_prepare(&quota, month, now_s, now_us, &again) == result);
            assert(coingecko_quota_commit(&quota, &proposal));
            ++writes;
        } else assert(result == COINGECKO_QUOTA_READY);
        const uint32_t interval = coingecko_quota_interval_s(&quota);
        assert(interval == (quota.record.reserved >= 8000U ? 1200U : 300U));
        assert(coingecko_quota_note_attempt(&quota, now_us));
        ++attempts;
        assert(coingecko_quota_prepare(&quota, month, now_s, now_us, &proposal) == COINGECKO_QUOTA_WAIT);
        assert(!coingecko_quota_note_attempt(&quota, now_us));
        now_s += interval;
        now_us += (int64_t)interval * 1000000LL;
    }
    assert(attempts == expected);
    assert(quota.record.reserved == 10000U);
    assert(writes == (expected + 7U) / 8U);
    assert(!coingecko_quota_note_attempt(&quota, now_us));
    assert(coingecko_quota_prepare(&quota, month - 1U, now_s, now_us, &proposal) == COINGECKO_QUOTA_INVALID);
    /* Exhaustion clears only in a later month, after another durable reservation. */
    assert(coingecko_quota_prepare(&quota, month + 1U, now_s, now_us, &proposal) == COINGECKO_QUOTA_RESERVE);
    assert(proposal.reserved == 8U);
    assert(coingecko_quota_commit(&quota, &proposal));
    assert(coingecko_quota_interval_s(&quota) == 300U);
}

int main(void)
{
    coingecko_quota_t quota, reboot;
    coingecko_quota_record_t proposed;
    assert(coingecko_quota_init(&quota, NULL));
    assert(quota.record.reserved == 8010U);
    assert(coingecko_quota_interval_s(&quota) == 1200U);
    assert(coingecko_quota_prepare(&quota, 202610U, 1791400000U, 1LL, &proposed) == COINGECKO_QUOTA_RESERVE);
    assert(proposed.reserved == 8018U);
    assert(!coingecko_quota_note_attempt(&quota, 1LL));
    assert(coingecko_quota_commit(&quota, &proposed));
    assert(!coingecko_quota_commit(&quota, &proposed));
    assert(coingecko_quota_note_attempt(&quota, 1LL));
    assert(coingecko_quota_init(&reboot, &quota.record));
    assert(reboot.remaining == 0U);
    assert(coingecko_quota_prepare(&reboot, 202610U, 1791400001U, 1LL, &proposed) == COINGECKO_QUOTA_WAIT);
    assert(coingecko_quota_prepare(&reboot, 202610U, quota.record.restart_after_s, 1LL, &proposed) == COINGECKO_QUOTA_RESERVE);
    assert(proposed.reserved == 8026U); /* unused seven credits are never refunded */
    proposed.crc32 ^= 1U;
    assert(!coingecko_quota_record_valid(&proposed));
    assert(!coingecko_quota_init(&reboot, &proposed));
    assert(!coingecko_quota_commit(&quota, &proposed));
    assert(coingecko_quota_prepare(&quota, 202613U, 1U, 1LL, &proposed) == COINGECKO_QUOTA_INVALID);
    assert(coingecko_quota_prepare(&quota, 202610U, 0U, 1LL, &proposed) == COINGECKO_QUOTA_INVALID);
    assert(coingecko_quota_prepare(&quota, 202610U, 1U, -1LL, &proposed) == COINGECKO_QUOTA_INVALID);
    assert(coingecko_quota_prepare(&quota, 202610U, 1U, INT64_MAX, &proposed) == COINGECKO_QUOTA_INVALID);
    assert(coingecko_quota_prepare(NULL, 202610U, 1U, 1LL, &proposed) == COINGECKO_QUOTA_INVALID);
    exercise_month(202610U, 1990U);
    exercise_month(202611U, 10000U);
    /* Year boundary and earlier clocks cannot restore already allocated credits. */
    assert(coingecko_quota_init(&quota, NULL));
    assert(coingecko_quota_prepare(&quota, 202612U, 1791400000U, 1LL, &proposed) == COINGECKO_QUOTA_RESERVE);
    assert(coingecko_quota_commit(&quota, &proposed));
    assert(coingecko_quota_prepare(&quota, 202701U, 1791400000U, 1LL, &proposed) == COINGECKO_QUOTA_RESERVE);
    assert(coingecko_quota_commit(&quota, &proposed));
    assert(coingecko_quota_prepare(&quota, 202612U, 1791400000U, 1LL, &proposed) == COINGECKO_QUOTA_INVALID);
    puts("CoinGecko quota: thresholds, durable ACK, complete budgets, cadence, reboot, rollover and corruption passed");
    return 0;
}
