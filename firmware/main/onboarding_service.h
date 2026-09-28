#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ONBOARDING_STAGE_WIFI = 0,
    ONBOARDING_STAGE_PASSWORD,
    ONBOARDING_STAGE_TIME,
    ONBOARDING_STAGE_SUMMARY,
    ONBOARDING_STAGE_SAVING,
    ONBOARDING_STAGE_COMPLETE,
    ONBOARDING_STAGE_ERROR,
} onboarding_stage_t;

typedef struct {
    bool required;
    bool completed;
    bool clock_24h;
    uint8_t timezone_index;
    bool timezone_persistence_pending;
    onboarding_stage_t stage;
    esp_err_t last_result;
} onboarding_service_status_t;

/* Owns only onboarding flow and non-secret preferences; it never owns Wi-Fi credentials. */
esp_err_t onboarding_service_start(void);
void onboarding_service_refresh(void);
/* Reopens the setup from Settings without deleting the existing profile first. */
esp_err_t onboarding_service_reopen(void);
esp_err_t onboarding_service_set_stage(onboarding_stage_t stage);
esp_err_t onboarding_service_set_clock_preferences(uint8_t timezone_index, bool clock_24h);
esp_err_t onboarding_service_complete(void);
/* Queues a timezone-only update for a completed profile. app_loop submits the
 * flash request, so a Settings callback never starts an NVS operation. The
 * status exposes timezone_persistence_pending until the profile is confirmed. */
esp_err_t onboarding_service_request_timezone_update(uint8_t timezone_index);
void onboarding_service_get_status(onboarding_service_status_t *out_status);

#ifdef __cplusplus
}
#endif
