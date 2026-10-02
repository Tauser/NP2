#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "offline_data_model.h"
#include "user_profile.h"
#include "pomodoro_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Events are intentionally bounded: credentials and provider bodies never
 * cross this boundary. Product snapshots are already validated, fixed-size
 * values safe for the app_loop to project to the UI. */
typedef enum {
    APP_EVENT_REFRESH_PLATFORM = 0,
    APP_EVENT_PRODUCT_DATA_UPDATED,
    APP_EVENT_USER_PROFILE_UPDATED,
    APP_EVENT_POMODORO_COMMAND,
    APP_EVENT_ONVIF_SCAN_REQUEST,
    APP_EVENT_ONVIF_ADDRESS_REQUEST,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    offline_data_snapshot_t offline_data;
    user_profile_t user_profile;
    pomodoro_command_t pomodoro_command;
    uint16_t pomodoro_value;
    char onvif_address[16];
} app_event_t;

typedef struct {
    bool ready;
    uint32_t posted_count;
    uint32_t dropped_count;
} app_event_bus_status_t;

esp_err_t app_event_bus_start(void);
esp_err_t app_event_bus_post(const app_event_t *event);
esp_err_t app_event_bus_post_product_data(const offline_data_snapshot_t *snapshot);
esp_err_t app_event_bus_post_pomodoro(pomodoro_command_t command, uint16_t value);
esp_err_t app_event_bus_post_onvif_scan_request(void);
esp_err_t app_event_bus_post_onvif_address_request(const char *address);
esp_err_t app_event_bus_receive(app_event_t *out_event, uint32_t timeout_ms);
void app_event_bus_get_status(app_event_bus_status_t *out_status);

#ifdef __cplusplus
}
#endif
