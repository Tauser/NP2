/* Conservative local budget, independent of ESP-IDF and credentials. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define COINGECKO_QUOTA_LIMIT 10000U
#define COINGECKO_QUOTA_WARNING 8000U
#define COINGECKO_QUOTA_BLOCK 8U
#define COINGECKO_QUOTA_NORMAL_S 300U
#define COINGECKO_QUOTA_REDUCED_S 1200U
/* Account dashboard supplied by the owner in October 2026. */
#define COINGECKO_QUOTA_SEED_MONTH 202610U
#define COINGECKO_QUOTA_SEED_USED 8010U
typedef struct {
    uint32_t magic, generation, month, reserved, restart_after_s, crc32;
} coingecko_quota_record_t;
typedef struct {
    coingecko_quota_record_t record;
    uint32_t remaining;
    int64_t next_attempt_us;
} coingecko_quota_t;
typedef enum {
    COINGECKO_QUOTA_INVALID, COINGECKO_QUOTA_WAIT, COINGECKO_QUOTA_EXHAUSTED,
    COINGECKO_QUOTA_RESERVE, COINGECKO_QUOTA_READY,
} coingecko_quota_result_t;
bool coingecko_quota_record_valid(const coingecko_quota_record_t *record);
bool coingecko_quota_init(coingecko_quota_t *quota, const coingecko_quota_record_t *persisted);
uint32_t coingecko_quota_interval_s(const coingecko_quota_t *quota);
/* Grants nothing until the flash owner commits and verifies the proposal. */
coingecko_quota_result_t coingecko_quota_prepare(const coingecko_quota_t *quota,
    uint32_t trusted_month, uint32_t now_s, int64_t now_us, coingecko_quota_record_t *proposal);
bool coingecko_quota_commit(coingecko_quota_t *quota, const coingecko_quota_record_t *verified);
bool coingecko_quota_note_attempt(coingecko_quota_t *quota, int64_t now_us);
