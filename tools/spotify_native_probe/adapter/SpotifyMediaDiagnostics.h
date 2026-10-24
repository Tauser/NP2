#pragma once
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
inline void spotifyMediaDiagnostic(unsigned stage,long value,unsigned count)
{
#ifdef ESP_PLATFORM
    ESP_LOGI("np2_spotify","media stage=%u value=%ld count=%u",stage,value,count);
#else
    (void)stage;(void)value;(void)count;
#endif
}
