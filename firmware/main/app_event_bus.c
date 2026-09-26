#include "app_event_bus.h"

#include "offline_data_codec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define APP_EVENT_BUS_QUEUE_LENGTH 32U

static QueueHandle_t s_queue;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static app_event_bus_status_t s_status;

esp_err_t app_event_bus_start(void)
{
    if (s_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_queue = xQueueCreate(APP_EVENT_BUS_QUEUE_LENGTH, sizeof(app_event_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&s_status_lock);
    s_status.ready = true;
    portEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t app_event_bus_post(const app_event_t *event)
{
    if (event == NULL || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_queue, event, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        ++s_status.dropped_count;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }

    portENTER_CRITICAL(&s_status_lock);
    ++s_status.posted_count;
    portEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t app_event_bus_post_product_data(const offline_data_snapshot_t *snapshot)
{
    if (!offline_data_snapshot_is_valid(snapshot)) {
        return ESP_ERR_INVALID_ARG;
    }
    const app_event_t event = {
        .type = APP_EVENT_PRODUCT_DATA_UPDATED,
        .offline_data = *snapshot,
    };
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_receive(app_event_t *out_event, uint32_t timeout_ms)
{
    if (out_event == NULL || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return xQueueReceive(s_queue, out_event, pdMS_TO_TICKS(timeout_ms)) == pdPASS
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

void app_event_bus_get_status(app_event_bus_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_status_lock);
}
