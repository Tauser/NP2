#include "onboarding_service.h"

#include "flash_coordinator.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static onboarding_service_status_t s_status;
static bool s_started;
static bool s_editing;

esp_err_t onboarding_service_start(void)
{
    taskENTER_CRITICAL(&s_lock);
    if (s_started) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    s_status = (onboarding_service_status_t){
        .required = true,
        .stage = ONBOARDING_STAGE_WIFI,
        .last_result = ESP_OK,
    };
    taskEXIT_CRITICAL(&s_lock);
    onboarding_service_refresh();
    return ESP_OK;
}

void onboarding_service_refresh(void)
{
    flash_coordinator_status_t storage = {0};
    flash_coordinator_get_status(&storage);
    taskENTER_CRITICAL(&s_lock);
    if (!s_started) {
        taskEXIT_CRITICAL(&s_lock);
        return;
    }
    if (s_editing && s_status.stage == ONBOARDING_STAGE_SAVING && !storage.pending && !storage.busy) {
        if (storage.onboarding_profile_valid && storage.onboarding_profile.completed) {
            s_status.required = false;
            s_status.completed = true;
            s_status.clock_24h = storage.onboarding_profile.clock_24h;
            s_status.timezone_index = storage.onboarding_profile.timezone_index;
            s_status.stage = ONBOARDING_STAGE_COMPLETE;
            s_status.last_result = ESP_OK;
            s_editing = false;
        } else {
            s_status.stage = ONBOARDING_STAGE_ERROR;
            s_status.last_result = storage.onboarding_profile_result;
        }
    } else if (!s_editing && storage.onboarding_profile_valid && storage.onboarding_profile.completed) {
        s_status.required = false;
        s_status.completed = true;
        s_status.clock_24h = storage.onboarding_profile.clock_24h;
        s_status.timezone_index = storage.onboarding_profile.timezone_index;
        s_status.stage = ONBOARDING_STAGE_COMPLETE;
        s_status.last_result = ESP_OK;
    }
    taskEXIT_CRITICAL(&s_lock);
}

esp_err_t onboarding_service_reopen(void)
{
    taskENTER_CRITICAL(&s_lock);
    if (!s_started) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_editing = true;
    s_status.required = true;
    s_status.completed = false;
    s_status.stage = ONBOARDING_STAGE_WIFI;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onboarding_service_set_stage(onboarding_stage_t stage)
{
    if (stage > ONBOARDING_STAGE_SUMMARY) return ESP_ERR_INVALID_ARG;
    taskENTER_CRITICAL(&s_lock);
    if (!s_started || s_status.completed) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_status.stage = stage;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onboarding_service_set_clock_preferences(uint8_t timezone_index, bool clock_24h)
{
    if (timezone_index > 1U) return ESP_ERR_INVALID_ARG;
    taskENTER_CRITICAL(&s_lock);
    if (!s_started || s_status.completed) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_status.timezone_index = timezone_index;
    s_status.clock_24h = clock_24h;
    s_status.stage = ONBOARDING_STAGE_SUMMARY;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onboarding_service_complete(void)
{
    onboarding_profile_t profile = {0};
    taskENTER_CRITICAL(&s_lock);
    if (!s_started || s_status.completed || s_status.stage != ONBOARDING_STAGE_SUMMARY) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    profile.completed = true;
    profile.clock_24h = s_status.clock_24h;
    profile.timezone_index = s_status.timezone_index;
    taskEXIT_CRITICAL(&s_lock);

    const esp_err_t result = flash_coordinator_request_onboarding_profile_write(&profile);
    taskENTER_CRITICAL(&s_lock);
    s_status.last_result = result;
    if (result == ESP_OK) s_status.stage = ONBOARDING_STAGE_SAVING;
    taskEXIT_CRITICAL(&s_lock);
    return result;
}

void onboarding_service_get_status(onboarding_service_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
