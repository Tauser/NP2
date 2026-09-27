#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct { bool general_enabled; bool sound_enabled; bool system_alerts_enabled; uint32_t generation; esp_err_t last_result; } notification_service_status_t;
esp_err_t notification_service_start(void); esp_err_t notification_service_set_general_enabled(bool); esp_err_t notification_service_set_sound_enabled(bool); esp_err_t notification_service_set_system_alerts_enabled(bool); void notification_service_get_status(notification_service_status_t *);
