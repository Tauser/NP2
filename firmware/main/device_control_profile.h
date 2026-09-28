#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Independent of the offline-data schema. v1 payloads contain only two bytes. */
typedef struct {
    uint8_t brightness_percent;
    uint8_t volume_percent;
    uint8_t night_mode_enabled;
} device_control_profile_t;

static inline bool device_control_profile_is_valid(const device_control_profile_t *profile)
{
    return profile != NULL && profile->brightness_percent <= 100U &&
           profile->volume_percent <= 100U && profile->night_mode_enabled <= 1U;
}

static inline bool device_control_profile_decode(const uint8_t *bytes, size_t size,
                                                  device_control_profile_t *out)
{
    if (bytes == NULL || out == NULL || (size != 2U && size != 3U)) return false;
    const device_control_profile_t profile = {bytes[0], bytes[1], size == 3U ? bytes[2] : 0U};
    if (!device_control_profile_is_valid(&profile)) return false;
    *out = profile;
    return true;
}
