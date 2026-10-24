#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PHOTO_WIDTH 1024U
#define PHOTO_HEIGHT 600U
#define PHOTO_BYTES (PHOTO_WIDTH * PHOTO_HEIGHT * 2U)
#define PHOTO_HEADER_BYTES 24U
#define PHOTO_MAX_FILES 64U
#define PHOTO_PAGE_SIZE 4U
#define PHOTO_IDLE_CHOICE_COUNT 8U
#define PHOTO_INTERVAL_CHOICE_COUNT 10U
#define PHOTO_EXIT_CHOICE_COUNT 9U
#define PHOTO_EXIT_CHOICE_DEFAULT 0U
#define PHOTO_IDLE_CHOICE_DEFAULT 3U
#define PHOTO_INTERVAL_CHOICE_DEFAULT 4U
#define PHOTO_CONFIG_DEFAULT 0x15U /* frame, idle 5 min, swap 2 min */
#define PHOTO_FRAME_ENABLED 1U
/* Bit 1 was the removed market wallpaper. Accept legacy profiles, ignore it. */
#define PHOTO_WIDGET_TOP 1U
#define PHOTO_WIDGET_LARGE 2U
#define PHOTO_WIDGET_VISIBLE 4U
#define PHOTO_BITCOIN_WIDGET_DEFAULT 28U /* bottom, 48 px, visible, 36% */
#define PHOTO_CLOCK_WIDGET_DEFAULT 30U   /* bottom, 64 px, visible, 36% */

static inline bool photo_widget_valid(uint8_t value) { return value < 64U; }
static inline uint8_t photo_widget_opacity(uint8_t value)
{
    return (uint8_t)(((value >> 3U) & 7U) * 12U * 255U / 100U);
}

static inline bool photo_config_valid(uint8_t config) { return config < 64U; }
static inline uint32_t photo_idle_choice_ms(uint8_t choice)
{
    const uint32_t seconds[] = {30, 60, 120, 300, 600, 900, 1800, 3600};
    return seconds[choice < PHOTO_IDLE_CHOICE_COUNT ? choice : PHOTO_IDLE_CHOICE_DEFAULT] * 1000U;
}
static inline uint32_t photo_interval_choice_ms(uint8_t choice)
{
    const uint32_t seconds[] = {10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600};
    return seconds[choice < PHOTO_INTERVAL_CHOICE_COUNT ? choice : PHOTO_INTERVAL_CHOICE_DEFAULT] * 1000U;
}
static inline uint8_t photo_mask_count(uint64_t mask)
{
    uint8_t count = 0;
    while (mask) { count += (uint8_t)(mask & 1U); mask >>= 1U; }
    return count;
}
static inline uint32_t photo_exit_choice_ms(uint8_t choice)
{
    const uint32_t seconds[] = {0, 30, 60, 120, 300, 600, 900, 1800, 3600};
    return seconds[choice < PHOTO_EXIT_CHOICE_COUNT ? choice : PHOTO_EXIT_CHOICE_DEFAULT] * 1000U;
}
/* A session starts on reveal, not on each slideshow image. Unsigned elapsed
 * time also handles the LVGL tick counter wrapping during an open session. */
static inline bool photo_exit_due(uint8_t choice, uint32_t started, uint32_t now)
{
    const uint32_t duration = photo_exit_choice_ms(choice);
    return duration != 0U && (uint32_t)(now - started) >= duration;
}
static inline void photo_page_ids(uint64_t catalog, uint8_t page, uint8_t ids[PHOTO_PAGE_SIZE])
{
    unsigned index = 0, used = 0;
    for (unsigned i = 0; i < PHOTO_PAGE_SIZE; ++i) ids[i] = 0;
    for (unsigned i = 0; i < PHOTO_MAX_FILES && used < PHOTO_PAGE_SIZE; ++i) {
        if (!(catalog & (UINT64_C(1) << i))) continue;
        if (index++ < (unsigned)page * PHOTO_PAGE_SIZE) continue;
        ids[used++] = (uint8_t)(i + 1U);
    }
}
static inline uint32_t photo_idle_ms(uint8_t config)
{
    const uint8_t minutes[] = {1, 5, 10, 15};
    return (uint32_t)minutes[(config >> 2U) & 3U] * 60000U;
}
static inline uint32_t photo_interval_ms(uint8_t config)
{
    const uint8_t minutes[] = {1, 2, 5, 10};
    return (uint32_t)minutes[(config >> 4U) & 3U] * 60000U;
}

uint32_t photo_crc_update(uint32_t crc, const uint8_t *data, size_t size);
bool photo_header_valid(const uint8_t header[PHOTO_HEADER_BYTES], uint32_t *crc);

typedef struct {
    bool ready;
    bool mounted;
    bool pending;
    bool available;
    uint8_t count;
    uint8_t enabled_count;
    uint8_t current;
    uint64_t catalog;
    uint32_t thumbnail_generation;
    uint8_t thumbnail_page;
    bool thumbnail_ready;
    uint32_t generation;
    int32_t result;
} photo_frame_status_t;
