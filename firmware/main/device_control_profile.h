#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "photo_frame_model.h"

/* Independent of the offline-data schema. v1 payloads contain only two bytes. */
typedef struct {
    uint8_t brightness_percent;
    uint8_t volume_percent;
    uint8_t night_mode_enabled;
    uint8_t photo_config;
    uint8_t photo_bitcoin_widget;
    uint8_t photo_clock_widget;
    uint8_t photo_idle_choice;
    uint8_t photo_interval_choice;
    uint64_t photo_enabled_mask;
    uint8_t photo_exit_choice;
    uint8_t reserved[7]; /* Explicit zero bytes: no padding in persisted CRC. */
} device_control_profile_t;

_Static_assert(sizeof(device_control_profile_t) == 24U, "Controls payload layout");
_Static_assert(offsetof(device_control_profile_t, photo_enabled_mask) == 8U, "Controls mask offset");
_Static_assert(offsetof(device_control_profile_t, photo_exit_choice) == 16U, "Controls exit offset");

static inline bool device_control_profile_is_valid(const device_control_profile_t *profile)
{
    if (profile == NULL) return false;
    for (unsigned i = 0; i < sizeof(profile->reserved); ++i)
        if (profile->reserved[i] != 0U) return false;
    return profile->brightness_percent <= 100U &&
           profile->volume_percent <= 100U && profile->night_mode_enabled <= 1U &&
           photo_config_valid(profile->photo_config) &&
           photo_widget_valid(profile->photo_bitcoin_widget) &&
           photo_widget_valid(profile->photo_clock_widget) &&
           profile->photo_idle_choice < PHOTO_IDLE_CHOICE_COUNT &&
           profile->photo_interval_choice < PHOTO_INTERVAL_CHOICE_COUNT &&
           profile->photo_exit_choice < PHOTO_EXIT_CHOICE_COUNT;
}

static inline bool device_control_profile_decode(const uint8_t *bytes, size_t size,
                                                  device_control_profile_t *out)
{
    if (bytes == NULL || out == NULL ||
        (size != 2U && size != 3U && size != 4U && size != 6U && size != 16U && size != 24U)) return false;
    if (size == 24U)
        for (unsigned i = 17; i < 24; ++i) if (bytes[i] != 0U) return false;
    const uint8_t config = size >= 4U ? bytes[3] : PHOTO_CONFIG_DEFAULT;
    const uint8_t legacy_idle[] = {1, 3, 4, 5};
    const uint8_t legacy_interval[] = {3, 4, 5, 6};
    uint64_t enabled = UINT64_MAX;
    if (size >= 16U) {
        enabled = 0;
        for (unsigned i = 0; i < 8; ++i) enabled |= (uint64_t)bytes[8U+i] << (i*8U);
    }
    const device_control_profile_t profile = {
        bytes[0], bytes[1], size >= 3U ? bytes[2] : 0U,
        config,
        size >= 6U ? bytes[4] : PHOTO_BITCOIN_WIDGET_DEFAULT,
        size >= 6U ? bytes[5] : PHOTO_CLOCK_WIDGET_DEFAULT,
        size >= 16U ? bytes[6] : legacy_idle[(config >> 2U) & 3U],
        size >= 16U ? bytes[7] : legacy_interval[(config >> 4U) & 3U],
        enabled,
        size == 24U ? bytes[16] : PHOTO_EXIT_CHOICE_DEFAULT,
        {0},
    };
    if (!device_control_profile_is_valid(&profile)) return false;
    *out = profile;
    return true;
}
