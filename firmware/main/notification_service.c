#include "notification_service.h"

#include "flash_coordinator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* FlashCoordinator request submission carries the largest generic request
 * envelope on the caller stack. Keep this worker sized for that fixed
 * contract instead of allowing a preference change to overflow its stack. */
#define NOTIFICATION_TASK_STACK_BYTES 8192U
#define NOTIFICATION_TASK_PRIORITY 2U
#define NOTIFICATION_PERSIST_DEBOUNCE_MS 500U
#define NOTIFICATION_SERVICE_POLL_MS 200U
#define NOTIFICATION_RETRY_DELAY_MS 5000U

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static bool s_started;
static bool s_restore_applied;
static bool s_save_enqueued;
static TickType_t s_next_save_attempt_tick;
static notification_service_status_t s_status = {
    .general_enabled = true,
    .sound_enabled = true,
    .system_alerts_enabled = true,
    .last_result = ESP_OK,
};

static bool profile_matches_status(const notification_profile_t *profile,
                                   const notification_service_status_t *status)
{
    return profile != NULL && status != NULL &&
           profile->general_enabled == status->general_enabled &&
           profile->sound_enabled == status->sound_enabled &&
           profile->system_alerts_enabled == status->system_alerts_enabled;
}

static void restore_preferences_if_available(void)
{
    flash_coordinator_status_t flash = {0};
    flash_coordinator_get_status(&flash);
    if (!flash.ready) return;

    portENTER_CRITICAL(&s_lock);
    if (!s_restore_applied) {
        if (flash.notification_profile_valid) {
            s_status.general_enabled = flash.notification_profile.general_enabled;
            s_status.sound_enabled = flash.notification_profile.sound_enabled;
            s_status.system_alerts_enabled = flash.notification_profile.system_alerts_enabled;
            s_status.persisted_generation = flash.notification_profile_generation;
            ++s_status.generation;
            s_status.last_result = ESP_OK;
        } else if (flash.notification_profile_result == ESP_ERR_NOT_FOUND) {
            s_status.last_result = ESP_OK;
        } else {
            s_status.last_result = flash.notification_profile_result;
        }
        s_status.ready = true;
        s_restore_applied = true;
    }

    if (s_save_enqueued && flash.notification_profile_valid &&
        profile_matches_status(&flash.notification_profile, &s_status)) {
        s_status.persistence_pending = false;
        s_status.persisted_generation = flash.notification_profile_generation;
        s_status.last_result = ESP_OK;
        s_save_enqueued = false;
    }
    portEXIT_CRITICAL(&s_lock);
}

static void submit_pending_profile(void)
{
    notification_service_status_t snapshot = {0};
    bool should_submit = false;
    portENTER_CRITICAL(&s_lock);
    should_submit = s_status.ready && s_status.persistence_pending && !s_save_enqueued &&
                    (int32_t)(xTaskGetTickCount() - s_next_save_attempt_tick) >= 0;
    snapshot = s_status;
    portEXIT_CRITICAL(&s_lock);
    if (!should_submit) return;

    const notification_profile_t profile = {
        .general_enabled = snapshot.general_enabled,
        .sound_enabled = snapshot.sound_enabled,
        .system_alerts_enabled = snapshot.system_alerts_enabled,
    };
    const esp_err_t result = flash_coordinator_request_notification_profile_write(&profile);
    portENTER_CRITICAL(&s_lock);
    if (result == ESP_OK) {
        s_save_enqueued = true;
        s_next_save_attempt_tick = 0U;
    } else if (result != ESP_ERR_TIMEOUT && result != ESP_ERR_INVALID_STATE) {
        s_status.last_result = result;
        s_next_save_attempt_tick =
            xTaskGetTickCount() + pdMS_TO_TICKS(NOTIFICATION_RETRY_DELAY_MS);
    }
    portEXIT_CRITICAL(&s_lock);
}

static void notification_task(void *arg)
{
    (void)arg;
    for (;;) {
        restore_preferences_if_available();

        const uint32_t notifications = ulTaskNotifyTake(
            pdTRUE, pdMS_TO_TICKS(NOTIFICATION_SERVICE_POLL_MS));
        if (notifications != 0U) {
            /* Coalesce a quick sequence of switches into one NVS request. */
            (void)ulTaskNotifyTake(pdTRUE,
                                   pdMS_TO_TICKS(NOTIFICATION_PERSIST_DEBOUNCE_MS));
        }
        submit_pending_profile();
    }
}

esp_err_t notification_service_start(void)
{
    portENTER_CRITICAL(&s_lock);
    if (s_started) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    portEXIT_CRITICAL(&s_lock);

    if (xTaskCreate(notification_task, "notifications", NOTIFICATION_TASK_STACK_BYTES,
                    NULL, NOTIFICATION_TASK_PRIORITY, &s_task) != pdPASS) {
        portENTER_CRITICAL(&s_lock);
        s_started = false;
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t set_preference(uint8_t preference, bool enabled)
{
    portENTER_CRITICAL(&s_lock);
    if (!s_started || !s_status.ready) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }

    bool *const value = preference == 0U ? &s_status.general_enabled :
                        preference == 1U ? &s_status.sound_enabled :
                                            &s_status.system_alerts_enabled;
    if (*value != enabled) {
        *value = enabled;
        ++s_status.generation;
        s_status.persistence_pending = true;
        s_status.last_result = ESP_OK;
        s_save_enqueued = false;
    }
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);

    if (task != NULL) xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t notification_service_set_general_enabled(bool enabled)
{
    return set_preference(0U, enabled);
}

esp_err_t notification_service_set_sound_enabled(bool enabled)
{
    return set_preference(1U, enabled);
}

esp_err_t notification_service_set_system_alerts_enabled(bool enabled)
{
    return set_preference(2U, enabled);
}

void notification_service_get_status(notification_service_status_t *out_status)
{
    if (out_status == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_lock);
}
