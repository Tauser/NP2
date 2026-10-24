#pragma once

/* Only public normalized data with no ISR/cache-off consumer may use this.
 * IDF places and clears these bounded buffers during PSRAM startup. */
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define NP2_PSRAM_BSS EXT_RAM_BSS_ATTR
#else
#define NP2_PSRAM_BSS
#endif
