/*
 * Controlled Phase 2 persistence probe.
 *
 * Flash access is serialized here because this board's flash cannot suspend
 * erase/write traffic safely while the MIPI-DSI pipeline reads PSRAM. This
 * diagnostic intentionally exercises one small NVS commit; larger writes,
 * filesystem GC, staging and maintenance mode are separate future requests.
 */
#include "flash_coordinator.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define FLASH_COORDINATOR_QUEUE_LENGTH 1
#define FLASH_COORDINATOR_TASK_STACK_BYTES 6144
#define FLASH_COORDINATOR_TASK_PRIORITY 2
#define FLASH_COORDINATOR_NVS_MIN_INTERVAL_US (60LL * 1000LL * 1000LL)

static const char *const TAG = "flash_coord";
static const char *const NVS_PARTITION = "nvs";
static const char *const NVS_NAMESPACE = "np2_diag";
static const char *const NVS_KEY = "probe_seq";

typedef enum {
    FLASH_REQUEST_NVS_PROBE,
} flash_request_kind_t;

typedef struct {
    flash_request_kind_t kind;
    uint32_t sequence;
} flash_request_t;

static QueueHandle_t s_request_queue;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static flash_coordinator_status_t s_status;
static int64_t s_last_success_us;

static void set_busy(bool busy, bool pending)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.busy = busy;
    s_status.pending = pending;
    portEXIT_CRITICAL(&s_status_lock);
}

static void complete_request(uint32_t sequence, esp_err_t result, uint32_t duration_ms)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.busy = false;
    s_status.pending = false;
    s_status.last_sequence = sequence;
    s_status.last_duration_ms = duration_ms;
    s_status.last_result = result;
    if (result == ESP_OK) {
        s_status.completed_count++;
    } else {
        s_status.rejected_count++;
    }
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t commit_nvs_probe(uint32_t sequence)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(NVS_PARTITION, NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u32(handle, NVS_KEY, sequence);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void flash_worker_task(void *arg)
{
    (void)arg;

    const esp_err_t init_result = nvs_flash_init_partition(NVS_PARTITION);
    portENTER_CRITICAL(&s_status_lock);
    s_status.init_result = init_result;
    s_status.ready = (init_result == ESP_OK);
    s_status.last_result = init_result;
    portEXIT_CRITICAL(&s_status_lock);

    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s; refusing all writes", esp_err_to_name(init_result));
        vTaskDelete(NULL);
        return;
    }

    flash_request_t request;
    while (xQueueReceive(s_request_queue, &request, portMAX_DELAY) == pdTRUE) {
        set_busy(true, false);
        const int64_t started_us = esp_timer_get_time();
        const esp_err_t result = commit_nvs_probe(request.sequence);
        const uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - started_us) / 1000LL);
        if (result == ESP_OK) {
            portENTER_CRITICAL(&s_status_lock);
            s_last_success_us = esp_timer_get_time();
            portEXIT_CRITICAL(&s_status_lock);
            ESP_LOGI(TAG, "diagnostic NVS commit %lu completed in %lums",
                     (unsigned long)request.sequence, (unsigned long)duration_ms);
        } else {
            ESP_LOGE(TAG, "diagnostic NVS commit failed: %s", esp_err_to_name(result));
        }
        complete_request(request.sequence, result, duration_ms);
    }
}

esp_err_t flash_coordinator_start(void)
{
    if (s_request_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_request_queue = xQueueCreate(FLASH_COORDINATOR_QUEUE_LENGTH, sizeof(flash_request_t));
    ESP_RETURN_ON_FALSE(s_request_queue != NULL, ESP_ERR_NO_MEM, TAG, "Flash queue allocation failed");

    portENTER_CRITICAL(&s_status_lock);
    s_status.init_result = ESP_ERR_INVALID_STATE;
    s_status.last_result = ESP_ERR_INVALID_STATE;
    portEXIT_CRITICAL(&s_status_lock);

    const BaseType_t task_created = xTaskCreate(flash_worker_task, "flash_worker",
                                                 FLASH_COORDINATOR_TASK_STACK_BYTES, NULL,
                                                 FLASH_COORDINATOR_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t flash_coordinator_request_nvs_probe(void)
{
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    flash_coordinator_status_t status;
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }

    const int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_status_lock);
    const int64_t last_success_us = s_last_success_us;
    portEXIT_CRITICAL(&s_status_lock);
    if (last_success_us != 0 && now_us - last_success_us < FLASH_COORDINATOR_NVS_MIN_INTERVAL_US) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_INVALID_STATE;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const flash_request_t request = {
        .kind = FLASH_REQUEST_NVS_PROBE,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

void flash_coordinator_get_status(flash_coordinator_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_status_lock);
}
