#include "app_event_bus.h"

#include <string.h>

#include "offline_data_codec.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#define APP_EVENT_BUS_QUEUE_LENGTH 32U
#define APP_EVENT_BUS_SNAPSHOT_SLOTS 4U

/* Queue entries carry only their active payload. The snapshot slots belong to
 * the bus from reservation until receive (or a failed enqueue). No borrowed
 * pointers cross tasks, and saturation never blocks a producer. */
typedef struct {
    app_event_type_t type;
    union {
        struct { uint8_t slot; uint32_t generation; } product;
        user_profile_t user_profile;
        struct { pomodoro_command_t command; uint16_t value; } pomodoro;
        uint32_t notification_id;
        char onvif_address[16];
        struct { char device_id[32]; uint8_t channel; bool enabled; } sonoff;
    } payload;
} app_queued_event_t;

typedef struct {
    offline_data_snapshot_t snapshot;
    uint32_t generation;
    bool in_use;
} app_snapshot_slot_t;

_Static_assert(sizeof(app_queued_event_t) <= 128U, "event queue payload grew unexpectedly");

static QueueHandle_t s_queue;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static app_event_bus_status_t s_status;
static app_snapshot_slot_t s_snapshot_slots[APP_EVENT_BUS_SNAPSHOT_SLOTS];

esp_err_t app_event_bus_start(void)
{
    if (s_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_queue = xQueueCreate(APP_EVENT_BUS_QUEUE_LENGTH, sizeof(app_queued_event_t));
    if (s_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&s_status_lock);
    s_status.ready = true;
    portEXIT_CRITICAL(&s_status_lock);
    ESP_LOGI("np2_events", "queue entries=%u item=%u payload=%u snapshot_slots=%u pool=%u",
             (unsigned int)APP_EVENT_BUS_QUEUE_LENGTH, (unsigned int)sizeof(app_queued_event_t),
             (unsigned int)(APP_EVENT_BUS_QUEUE_LENGTH * sizeof(app_queued_event_t)),
             (unsigned int)APP_EVENT_BUS_SNAPSHOT_SLOTS, (unsigned int)sizeof(s_snapshot_slots));
    return ESP_OK;
}

static esp_err_t enqueue_event(const app_queued_event_t *event)
{
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

esp_err_t app_event_bus_post(const app_event_t *event)
{
    if (event == NULL || s_queue == NULL) return ESP_ERR_INVALID_STATE;
    app_queued_event_t queued = {.type = event->type};
    switch (event->type) {
    case APP_EVENT_PRODUCT_DATA_UPDATED:
        return app_event_bus_post_product_data(&event->offline_data);
    case APP_EVENT_USER_PROFILE_UPDATED:
        queued.payload.user_profile = event->user_profile;
        break;
    case APP_EVENT_POMODORO_COMMAND:
        queued.payload.pomodoro.command = event->pomodoro_command;
        queued.payload.pomodoro.value = event->pomodoro_value;
        break;
    case APP_EVENT_ONVIF_ADDRESS_REQUEST:
        memcpy(queued.payload.onvif_address, event->onvif_address, sizeof(event->onvif_address));
        break;
    case APP_EVENT_SONOFF_SWITCH_REQUEST:
        memcpy(queued.payload.sonoff.device_id, event->sonoff_device_id,
               sizeof(event->sonoff_device_id));
        queued.payload.sonoff.channel = event->sonoff_channel;
        queued.payload.sonoff.enabled = event->sonoff_enabled;
        break;
    case APP_EVENT_NOTIFICATION_MARK_READ:
        queued.payload.notification_id = event->notification_id;
        break;
    case APP_EVENT_REFRESH_PLATFORM:
    case APP_EVENT_ONVIF_SCAN_REQUEST:
    case APP_EVENT_NOTIFICATION_MARK_ALL_READ:
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return enqueue_event(&queued);
}

esp_err_t app_event_bus_post_product_data(const offline_data_snapshot_t *snapshot)
{
    if (!offline_data_snapshot_is_valid(snapshot)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_queue == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t index = APP_EVENT_BUS_SNAPSHOT_SLOTS;
    uint32_t generation = 0U;
    portENTER_CRITICAL(&s_status_lock);
    for (uint8_t i = 0U; i < APP_EVENT_BUS_SNAPSHOT_SLOTS; ++i) {
        if (s_snapshot_slots[i].in_use) continue;
        index = i;
        s_snapshot_slots[i].in_use = true;
        generation = ++s_snapshot_slots[i].generation;
        if (generation == 0U) generation = ++s_snapshot_slots[i].generation;
        break;
    }
    if (index == APP_EVENT_BUS_SNAPSHOT_SLOTS) ++s_status.dropped_count;
    portEXIT_CRITICAL(&s_status_lock);
    if (index == APP_EVENT_BUS_SNAPSHOT_SLOTS) return ESP_ERR_TIMEOUT;

    /* Only this producer can access the reserved slot before publishing its
     * handle. Queue synchronization publishes the completed copy. */
    s_snapshot_slots[index].snapshot = *snapshot;
    const app_queued_event_t queued = {
        .type = APP_EVENT_PRODUCT_DATA_UPDATED,
        .payload.product = {.slot = index, .generation = generation},
    };
    const esp_err_t result = enqueue_event(&queued);
    if (result != ESP_OK) {
        portENTER_CRITICAL(&s_status_lock);
        s_snapshot_slots[index].in_use = false;
        portEXIT_CRITICAL(&s_status_lock);
    }
    return result;
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

esp_err_t app_event_bus_post_sonoff_switch_request(const char *device_id,
                                                    uint8_t channel,
                                                    bool enabled)
{
    if (device_id == NULL || channel >= 3U || strnlen(device_id, 11U) != 10U)
        return ESP_ERR_INVALID_ARG;
    for (size_t i = 0U; i < 10U; ++i) {
        const char c = device_id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z'))) return ESP_ERR_INVALID_ARG;
    }
    app_event_t event = {
        .type = APP_EVENT_SONOFF_SWITCH_REQUEST,
        .sonoff_channel = channel,
        .sonoff_enabled = enabled,
    };
    memcpy(event.sonoff_device_id, device_id, 10U);
    event.sonoff_device_id[10] = '\0';
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_post_notification_mark_read(uint32_t id)
{
    if (id == 0U) return ESP_ERR_INVALID_ARG;
    const app_event_t event = {
        .type = APP_EVENT_NOTIFICATION_MARK_READ,
        .notification_id = id,
    };
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_post_notification_mark_all_read(void)
{
    const app_event_t event = {
        .type = APP_EVENT_NOTIFICATION_MARK_ALL_READ,
    };
    return app_event_bus_post(&event);
}

esp_err_t app_event_bus_receive(app_event_t *out_event, uint32_t timeout_ms)
{
    if (out_event == NULL || s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    app_queued_event_t queued = {0};
    if (xQueueReceive(s_queue, &queued, pdMS_TO_TICKS(timeout_ms)) != pdPASS)
        return ESP_ERR_TIMEOUT;
    memset(out_event, 0, sizeof(*out_event));
    out_event->type = queued.type;
    switch (queued.type) {
    case APP_EVENT_PRODUCT_DATA_UPDATED: {
        const uint8_t index = queued.payload.product.slot;
        if (index >= APP_EVENT_BUS_SNAPSHOT_SLOTS) return ESP_ERR_INVALID_STATE;
        portENTER_CRITICAL(&s_status_lock);
        app_snapshot_slot_t *slot = &s_snapshot_slots[index];
        const bool valid = slot->in_use && slot->generation == queued.payload.product.generation;
        if (valid) {
            out_event->offline_data = slot->snapshot;
            slot->in_use = false;
        }
        portEXIT_CRITICAL(&s_status_lock);
        return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    case APP_EVENT_USER_PROFILE_UPDATED:
        out_event->user_profile = queued.payload.user_profile;
        break;
    case APP_EVENT_POMODORO_COMMAND:
        out_event->pomodoro_command = queued.payload.pomodoro.command;
        out_event->pomodoro_value = queued.payload.pomodoro.value;
        break;
    case APP_EVENT_ONVIF_ADDRESS_REQUEST:
        memcpy(out_event->onvif_address, queued.payload.onvif_address, sizeof(out_event->onvif_address));
        break;
    case APP_EVENT_SONOFF_SWITCH_REQUEST:
        memcpy(out_event->sonoff_device_id, queued.payload.sonoff.device_id,
               sizeof(out_event->sonoff_device_id));
        out_event->sonoff_channel = queued.payload.sonoff.channel;
        out_event->sonoff_enabled = queued.payload.sonoff.enabled;
        break;
    case APP_EVENT_NOTIFICATION_MARK_READ:
        out_event->notification_id = queued.payload.notification_id;
        break;
    case APP_EVENT_REFRESH_PLATFORM:
    case APP_EVENT_ONVIF_SCAN_REQUEST:
    case APP_EVENT_NOTIFICATION_MARK_ALL_READ:
        break;
    default:
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
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
