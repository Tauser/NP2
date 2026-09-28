#pragma once
#include <stdbool.h>

static inline bool system_restart_allowed(bool ready, bool flash_busy,
                                           bool flash_pending, bool preferences_pending,
                                           bool ota_active, bool boot_pending)
{
    return ready && !flash_busy && !flash_pending && !preferences_pending &&
           !ota_active && !boot_pending;
}
