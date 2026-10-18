#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SYSTEM_CPU_HISTORY_COUNT 30U

/* Bounded, non-secret diagnostic values. No SDK or LVGL dependency. */
typedef struct {
    bool available;
    bool maintenance;
    bool cpu_available;
    uint8_t cpu_percent;
    uint8_t cpu_core_percent[2];
    uint32_t sequence;
    uint32_t uptime_s;
    uint32_t internal_total_bytes;
    uint32_t internal_free_bytes;
    uint32_t internal_largest_bytes;
    uint32_t psram_total_bytes;
    uint32_t psram_free_bytes;
    bool storage_available;
    uint32_t storage_total_bytes;
    uint32_t storage_used_bytes;
    bool sd_mounted;
    bool sd_available;
    uint64_t sd_total_bytes;
    uint64_t sd_free_bytes;
} system_resource_snapshot_t;

static inline uint8_t system_used_percent(uint64_t total, uint64_t free_bytes)
{
    if (total == 0U || free_bytes > total) return 0U;
    return (uint8_t)(((total - free_bytes) * 100U + total / 2U) / total);
}

/* Unsigned subtraction intentionally handles the 32-bit timer/idle rollover.
 * A delayed/out-of-range sample is unavailable; it never invents a CPU value. */
static inline bool system_cpu_window(uint32_t now, uint32_t prior,
                                     uint32_t idle, uint32_t prior_idle,
                                     uint8_t *out_percent)
{
    const uint32_t elapsed = now - prior;
    if (out_percent == 0 || elapsed < 500000U || elapsed > 30000000U) return false;
    uint32_t idle_delta = idle - prior_idle;
    if (idle_delta > elapsed) idle_delta = elapsed;
    *out_percent = system_used_percent(elapsed, idle_delta);
    return true;
}
