#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PROVISIONING_TOUCH_STAGE_IDLE = 0,
    PROVISIONING_TOUCH_STAGE_SSID,
    PROVISIONING_TOUCH_STAGE_PASSWORD,
} provisioning_touch_stage_t;

typedef struct {
    bool armed;
    uint32_t remaining_seconds;
    esp_err_t last_result;
    bool touch_active;
    provisioning_touch_stage_t touch_stage;
    uint8_t touch_ssid_length;
    uint8_t touch_password_length;
} provisioning_service_status_t;

/* Starts the physical USB maintenance service in its disarmed state. */
esp_err_t provisioning_service_start(void);

/* Arms one RAM-only open-network request for a short physical-maintenance window. */
esp_err_t provisioning_service_arm_open_network(void);

/*
 * Product-facing WPA2 input session. The service owns both input buffers in
 * internal RAM. LVGL receives only length/status snapshots and must never
 * retain the password text in a widget, log, state model or event payload.
 */
esp_err_t provisioning_service_touch_begin(void);
esp_err_t provisioning_service_touch_append_ssid(char character);
esp_err_t provisioning_service_touch_backspace_ssid(void);
esp_err_t provisioning_service_touch_begin_password(void);
esp_err_t provisioning_service_touch_append_password(char character);
esp_err_t provisioning_service_touch_backspace_password(void);
esp_err_t provisioning_service_touch_submit(void);
void provisioning_service_touch_cancel(void);

/* Copies the non-secret SSID being edited for the local view only. */
esp_err_t provisioning_service_touch_copy_ssid(char *out_ssid, size_t out_size);

/* Reads a lock-protected status snapshot; no credential material is exposed. */
void provisioning_service_get_status(provisioning_service_status_t *out_status);

#ifdef __cplusplus
}
#endif
