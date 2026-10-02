#include "app_event_bus.h"

#include <string.h>

#include "offline_data_codec.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

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

esp_err_t app_event_bus_post_pomodoro(pomodoro_command_t command, uint16_t value)
{
    if ((command != POMODORO_COMMAND_TOGGLE &&
         command != POMODORO_COMMAND_RESET &&
         command != POMODORO_COMMAND_SELECT_MINUTES) ||
        (command == POMODORO_COMMAND_SELECT_MINUTES &&
         (value == 0U || value > 99U))) {
        return ESP_ERR_INVALID_ARG;
    }
    const app_event_t event = {
        .type = APP_EVENT_POMODORO_COMMAND,
        .pomodoro_command = command,
        .pomodoro_value = value,
    };
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_post_onvif_scan_request(void)
{
    const app_event_t event = {.type = APP_EVENT_ONVIF_SCAN_REQUEST};
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_post_onvif_address_request(const char *address)
{
    if (address == NULL) return ESP_ERR_INVALID_ARG;
    const size_t length = strnlen(address, sizeof(((app_event_t *)0)->onvif_address));
    if (length == 0U || length >= sizeof(((app_event_t *)0)->onvif_address)) {
        return ESP_ERR_INVALID_ARG;
    }
    struct in_addr ipv4 = {0};
    if (inet_pton(AF_INET, address, &ipv4) != 1) return ESP_ERR_INVALID_ARG;
    const uint32_t host_order = ntohl(ipv4.s_addr);
    const bool private_ipv4 =
        (host_order & UINT32_C(0xff000000)) == UINT32_C(0x0a000000) ||
        (host_order & UINT32_C(0xfff00000)) == UINT32_C(0xac100000) ||
        (host_order & UINT32_C(0xffff0000)) == UINT32_C(0xc0a80000);
    if (!private_ipv4) return ESP_ERR_INVALID_ARG;
    app_event_t event = {.type = APP_EVENT_ONVIF_ADDRESS_REQUEST};
    memcpy(event.onvif_address, address, length);
    event.onvif_address[length] = '\0';
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
