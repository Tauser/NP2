#include "coingecko_quota.h"
#include "cache_record.h"
#include <stddef.h>
#include <string.h>
#define QUOTA_MAGIC UINT32_C(0x43475131)
_Static_assert(sizeof(coingecko_quota_record_t) == 24U, "Quota record layout changed");
static bool month_valid(uint32_t month)
{
    return month >= COINGECKO_QUOTA_SEED_MONTH && month <= 210612U &&
           month % 100U >= 1U && month % 100U <= 12U;
}
static void seal(coingecko_quota_record_t *record)
{
    record->crc32 = cache_record_crc32((const uint8_t *)record,
                                      offsetof(coingecko_quota_record_t, crc32));
}
bool coingecko_quota_record_valid(const coingecko_quota_record_t *record)
{
    return record != NULL && record->magic == QUOTA_MAGIC && month_valid(record->month) &&
           record->reserved <= COINGECKO_QUOTA_LIMIT &&
           (record->month != COINGECKO_QUOTA_SEED_MONTH || record->reserved >= COINGECKO_QUOTA_SEED_USED) &&
           record->crc32 == cache_record_crc32((const uint8_t *)record,
                                              offsetof(coingecko_quota_record_t, crc32));
}
bool coingecko_quota_init(coingecko_quota_t *quota, const coingecko_quota_record_t *persisted)
{
    if (quota == NULL || (persisted != NULL && !coingecko_quota_record_valid(persisted))) return false;
    memset(quota, 0, sizeof(*quota));
    if (persisted != NULL) quota->record = *persisted;
    else {
        quota->record = (coingecko_quota_record_t){.magic = QUOTA_MAGIC,
            .month = COINGECKO_QUOTA_SEED_MONTH, .reserved = COINGECKO_QUOTA_SEED_USED};
        seal(&quota->record);
    }
    /* Preserve all charged credits, including unused legacy reservations.
     * Their wall-clock window is replaced by one full monotonic interval. */
    quota->recovery_pending = persisted != NULL;
    return true;
}
uint32_t coingecko_quota_interval_s(const coingecko_quota_t *quota)
{
    return quota == NULL || quota->record.reserved >= COINGECKO_QUOTA_WARNING
               ? COINGECKO_QUOTA_REDUCED_S : COINGECKO_QUOTA_NORMAL_S;
}
coingecko_quota_result_t coingecko_quota_prepare(coingecko_quota_t *quota,
    uint32_t trusted_month, uint32_t now_s, int64_t now_us, coingecko_quota_record_t *proposal)
{
    if (quota == NULL || proposal == NULL || !coingecko_quota_record_valid(&quota->record) ||
        !month_valid(trusted_month) || trusted_month < quota->record.month || now_s == 0U ||
        now_us < 0LL || now_us > INT64_MAX - (int64_t)COINGECKO_QUOTA_REDUCED_S * 1000000LL)
        return COINGECKO_QUOTA_INVALID;
    if (now_us < quota->next_attempt_us) return COINGECKO_QUOTA_WAIT;
    const bool rollover = trusted_month > quota->record.month;
    if (!rollover && quota->remaining > 0U) return COINGECKO_QUOTA_READY;
    const uint32_t used = rollover ? 0U : quota->record.reserved;
    if (used >= COINGECKO_QUOTA_LIMIT) return COINGECKO_QUOTA_EXHAUSTED;
    if (quota->recovery_pending) {
        quota->recovery_pending = false;
        quota->next_attempt_us = now_us + (int64_t)coingecko_quota_interval_s(quota) * 1000000LL;
        return COINGECKO_QUOTA_WAIT;
    }
    if (quota->record.generation == UINT32_MAX) return COINGECKO_QUOTA_INVALID;
    uint32_t count = COINGECKO_QUOTA_LIMIT - used;
    if (count > COINGECKO_QUOTA_BLOCK) count = COINGECKO_QUOTA_BLOCK;
    const uint32_t interval = used + count >= COINGECKO_QUOTA_WARNING
                                 ? COINGECKO_QUOTA_REDUCED_S : COINGECKO_QUOTA_NORMAL_S;
    if (now_s > UINT32_MAX - count * interval) return COINGECKO_QUOTA_INVALID;
    *proposal = (coingecko_quota_record_t){.magic = QUOTA_MAGIC,
        .generation = quota->record.generation + 1U, .month = trusted_month,
        .reserved = used + count, .restart_after_s = now_s + count * interval};
    seal(proposal);
    return COINGECKO_QUOTA_RESERVE;
}
bool coingecko_quota_commit(coingecko_quota_t *quota, const coingecko_quota_record_t *verified)
{
    if (quota == NULL || !coingecko_quota_record_valid(verified) ||
        quota->record.generation == UINT32_MAX || verified->generation != quota->record.generation + 1U ||
        verified->month < quota->record.month) return false;
    const uint32_t previous = verified->month > quota->record.month ? 0U : quota->record.reserved;
    if (verified->reserved <= previous || verified->reserved - previous > COINGECKO_QUOTA_BLOCK) return false;
    quota->remaining = verified->reserved - previous;
    quota->record = *verified;
    quota->recovery_pending = false;
    return true;
}
bool coingecko_quota_note_attempt(coingecko_quota_t *quota, int64_t now_us)
{
    if (quota == NULL || quota->remaining == 0U || now_us < quota->next_attempt_us ||
        now_us < 0LL || now_us > INT64_MAX - (int64_t)COINGECKO_QUOTA_REDUCED_S * 1000000LL) return false;
    --quota->remaining;
    quota->next_attempt_us = now_us + (int64_t)coingecko_quota_interval_s(quota) * 1000000LL;
    return true;
}
