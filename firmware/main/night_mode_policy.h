#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Local schedule: 22:00 inclusive to 06:00 exclusive. Unknown time never dims. */
static inline bool night_mode_active(bool enabled, bool trusted, unsigned hour)
{
    return enabled && trusted && hour < 24U && (hour >= 22U || hour < 6U);
}

static inline uint8_t night_mode_brightness(uint8_t preferred, bool active)
{
    return active && preferred > 15U ? 15U : preferred;
}
