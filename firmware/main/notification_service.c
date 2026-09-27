#include "notification_service.h"

#include "device_control_service.h"
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

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static bool s_started;
static bool s_restore_applied;
static bool s_save_enqueued;
static uint32_t s_pending_flash_sequence;
static notification_profile_t s_submitted_profile;
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

static bool profiles_match(const notification_profile_t *left,
                           const notification_profile_t *right)
{
    return left != NULL && right != NULL &&
           left->general_enabled == right->general_enabled &&
           left->sound_enabled == right->sound_enabled &&
           left->system_alerts_enabled == right->system_alerts_enabled;
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

    if (s_save_enqueued &&
        flash.notification_profile_completed_sequence == s_pending_flash_sequence) {
        s_save_enqueued = false;
        s_pending_flash_sequence = 0U;
        if (flash.notification_profile_last_write_result == ESP_OK &&
            flash.notification_profile_valid &&
            profiles_match(&flash.notification_profile, &s_submitted_profile)) {
            s_status.persisted_generation = flash.notification_profile_generation;
            s_status.last_result = ESP_OK;
            /* A preference may have changed while this request was in flight.
             * Preserve that dirty state so the worker submits the newer profile
             * only after the confirmed write above has released the coordinator. */
            s_status.persistence_pending = !profile_matches_status(&s_submitted_profile,
                                                                     &s_status);
        } else {
            s_status.persistence_pending = false;
            s_status.last_result = flash.notification_profile_last_write_result == ESP_OK
                                       ? ESP_FAIL
                                       : flash.notification_profile_last_write_result;
        }
    }
    portEXIT_CRITICAL(&s_lock);
}

static void submit_pending_profile(void)
{
    notification_service_status_t snapshot = {0};
    bool should_submit = false;
    portENTER_CRITICAL(&s_lock);
    should_submit = s_status.ready && s_status.persistence_pending && !s_save_enqueued;
    snapshot = s_status;
    portEXIT_CRITICAL(&s_lock);
    if (!should_submit) return;

    const notification_profile_t profile = {
        .general_enabled = snapshot.general_enabled,
        .sound_enabled = snapshot.sound_enabled,
        .system_alerts_enabled = snapshot.system_alerts_enabled,
    };
    uint32_t sequence = 0U;
    const esp_err_t result =
        flash_coordinator_request_notification_profile_write(&profile, &sequence);
    portENTER_CRITICAL(&s_lock);
    if (result == ESP_OK) {
        s_save_enqueued = true;
        s_pending_flash_sequence = sequence;
        s_submitted_profile = profile;
    } else {
        /* Submission itself failed. A later user change is the next retry. */
        s_status.persistence_pending = false;
        s_status.last_result = result;
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

esp_err_t notification_service_request_alert_sound(void)
{
    portENTER_CRITICAL(&s_lock);
    const bool allowed = s_started && s_status.ready &&
                         s_status.general_enabled && s_status.sound_enabled;
    portEXIT_CRITICAL(&s_lock);
    return allowed ? device_control_play_notification_tone() : ESP_ERR_INVALID_STATE;
}

void notification_service_get_status(notification_service_status_t *out_status)
{
    if (out_status == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_lock);
}
