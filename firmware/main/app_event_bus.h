#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Events are intentionally tiny: credentials, provider bodies and UI models
 * never cross this boundary. */
typedef enum {
    APP_EVENT_REFRESH_PLATFORM = 0,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
} app_event_t;

typedef struct {
    bool ready;
    uint32_t posted_count;
    uint32_t dropped_count;
} app_event_bus_status_t;

esp_err_t app_event_bus_start(void);
esp_err_t app_event_bus_post(const app_event_t *event);
esp_err_t app_event_bus_receive(app_event_t *out_event, uint32_t timeout_ms);
void app_event_bus_get_status(app_event_bus_status_t *out_status);

#ifdef __cplusplus
}
#endif
