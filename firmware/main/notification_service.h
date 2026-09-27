#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* A lock-protected snapshot consumed by app_loop. The service owns mutation
 * and persistence coordination; the LVGL task only submits preferences. */
typedef struct {
    bool ready;
    bool general_enabled;
    bool sound_enabled;
    bool system_alerts_enabled;
    bool persistence_pending;
    uint32_t generation;
    uint32_t persisted_generation;
    esp_err_t last_result;
} notification_service_status_t;

esp_err_t notification_service_start(void);
esp_err_t notification_service_set_general_enabled(bool enabled);
esp_err_t notification_service_set_sound_enabled(bool enabled);
esp_err_t notification_service_set_system_alerts_enabled(bool enabled);
void notification_service_get_status(notification_service_status_t *out_status);
