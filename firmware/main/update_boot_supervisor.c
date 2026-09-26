#include "update_boot_supervisor.h"

#include "app_state.h"
#include "board_bringup.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "flash_coordinator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "update_policy.h"

#define UPDATE_BOOT_SUPERVISOR_PERIOD_MS 250U
#define UPDATE_BOOT_SUPERVISOR_STACK_BYTES 4096U
#define UPDATE_BOOT_SUPERVISOR_PRIORITY 3U

static const char *const TAG = "update_boot";
static bool s_started;

static bool fallback_is_valid(void)
{
    const esp_partition_t *const fallback = esp_ota_get_next_update_partition(NULL);
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    return fallback != NULL && esp_ota_get_state_partition(fallback, &state) == ESP_OK &&
           state == ESP_OTA_IMG_VALID;
}

static update_health_sample_t sample_health(void)
{
    board_bringup_health_t board = {0};
    app_state_health_t app = {0};
    flash_coordinator_status_t flash = {0};
    board_bringup_get_health(&board);
    app_state_get_health(&app);
    flash_coordinator_get_status(&flash);
    return (update_health_sample_t){
        .display_ready = board.display_ready,
        .first_frame_presented = board.first_frame_presented,
        .local_services_ready = flash.ready,
        .app_progress_seen = app.ready && app.last_progress_ms != 0U,
        .ui_progress_seen = board.last_ui_progress_ms != 0U,
        .app_progress_ms = app.last_progress_ms,
        .ui_progress_ms = board.last_ui_progress_ms,
        .fallback_bootable = fallback_is_valid(),
    };
}

static void update_boot_supervisor_task(void *arg)
{
    (void)arg;
    update_boot_policy_t policy = {0};
    update_boot_policy_init(&policy, (uint64_t)esp_timer_get_time() / 1000U, true);
    bool queued = false;
    bool confirmation_failed = false;
    uint32_t queued_sequence = 0U;
    update_boot_action_t queued_action = UPDATE_BOOT_WAIT;

    for (;;) {
        const esp_partition_t *const running = esp_ota_get_running_partition();
        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        if (running != NULL && esp_ota_get_state_partition(running, &state) == ESP_OK &&
            state == ESP_OTA_IMG_VALID) {
            ESP_LOGI(TAG, "P4 running slot VALID verified from otadata");
            break;
        }
        flash_coordinator_status_t flash = {0};
        flash_coordinator_get_status(&flash);
        if (queued && !flash.busy && !flash.pending && flash.last_sequence >= queued_sequence) {
            ESP_LOGE(TAG, "P4 boot action=%u completed without VALID/reboot: %s",
                     (unsigned)queued_action, esp_err_to_name(flash.last_result));
            if (queued_action == UPDATE_BOOT_ROLLBACK) break;
            queued = false;
            confirmation_failed = true;
        }
        update_health_sample_t sample = sample_health();
        if (confirmation_failed) sample.local_services_ready = false;
        const update_boot_action_t action = update_boot_policy_poll(
            &policy, (uint64_t)esp_timer_get_time() / 1000U, &sample);
        if (!queued && (action == UPDATE_BOOT_CONFIRM || action == UPDATE_BOOT_ROLLBACK)) {
            const esp_err_t result = action == UPDATE_BOOT_CONFIRM
                                         ? flash_coordinator_request_p4_ota_confirm()
                                         : flash_coordinator_request_p4_ota_rollback();
            if (result == ESP_OK) {
                queued = true;
                queued_sequence = flash.last_sequence + 1U;
                queued_action = action;
                ESP_LOGI(TAG, "P4 boot action=%u queued; awaiting persisted result", (unsigned)action);
            }
        } else if (action == UPDATE_BOOT_RECOVERY) {
            ESP_LOGE(TAG, "P4 pending image failed health without verified fallback");
            break;
        }
        if ((uint64_t)esp_timer_get_time() / 1000U - policy.boot_ms >=
            UPDATE_HEALTH_DEADLINE_MS + 5000U) {
            ESP_LOGE(TAG, "P4 recovery blocked: flash worker did not complete boot action");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(UPDATE_BOOT_SUPERVISOR_PERIOD_MS));
    }
    vTaskDelete(NULL);
}

esp_err_t update_boot_supervisor_start(void)
{
    if (s_started) return ESP_ERR_INVALID_STATE;
    const esp_partition_t *const running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == NULL) return ESP_FAIL;
    const esp_err_t result = esp_ota_get_state_partition(running, &state);
    ESP_LOGI(TAG, "running=%s state=%u lookup=%s", running->label,
             (unsigned)state, esp_err_to_name(result));
    /* A serial-provisioned baseline can have no OTA record yet. It must not
     * be mistaken for a confirmed fallback by the update admission path. */
    if (result == ESP_ERR_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_OK;
    }
    if (xTaskCreate(update_boot_supervisor_task, "ota_boot", UPDATE_BOOT_SUPERVISOR_STACK_BYTES,
                    NULL, UPDATE_BOOT_SUPERVISOR_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    ESP_LOGW(TAG, "P4 image is pending verify; local health supervisor started");
    return ESP_OK;
}
