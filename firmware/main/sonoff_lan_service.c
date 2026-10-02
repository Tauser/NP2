#include "sonoff_lan_service.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "connectivity_diagnostic.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#define SONOFF_LAN_TASK_STACK_BYTES (5U * 1024U)
#define SONOFF_LAN_TASK_PRIORITY 2U
#define SONOFF_LAN_SCAN_TIMEOUT_MS 3000U
#define SONOFF_LAN_POLL_MS 100U

static const char *const TAG = "sonoff_lan";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static sonoff_lan_status_t s_status = {.last_result = ESP_ERR_INVALID_STATE};
static bool s_started;
static bool s_scan_requested;
static bool s_mdns_initialized;

static void copy_text(char *out, size_t out_size, const char *value)
{
    if (out == NULL || out_size == 0U) return;
    out[0] = '\0';
    if (value == NULL) return;
    const size_t length = strnlen(value, out_size - 1U);
    memcpy(out, value, length);
    out[length] = '\0';
}

static bool starts_with_ewelink(const char *instance)
{
    static const char prefix[] = "ewelink_";
    if (instance == NULL || strnlen(instance, 9U) < sizeof(prefix) - 1U) return false;
    for (size_t i = 0U; i < sizeof(prefix) - 1U; ++i) {
        if ((char)tolower((unsigned char)instance[i]) != prefix[i]) return false;
    }
    return true;
}

static const char *txt_value(const mdns_result_t *result, const char *key)
{
    if (result == NULL || key == NULL) return NULL;
    for (size_t i = 0U; i < result->txt_count; ++i) {
        if (result->txt[i].key != NULL && strcmp(result->txt[i].key, key) == 0) {
            return result->txt[i].value;
        }
    }
    return NULL;
}

static bool parse_device(const mdns_result_t *result, sonoff_lan_device_t *out)
{
    if (result == NULL || out == NULL || !starts_with_ewelink(result->instance_name)) {
        return false;
    }
    const char *const id = result->instance_name + 8U;
    if (strnlen(id, sizeof(out->device_id)) != 10U) return false;
    for (size_t i = 0U; i < 10U; ++i) {
        if (!isalnum((unsigned char)id[i])) return false;
    }

    memset(out, 0, sizeof(*out));
    memcpy(out->device_id, id, 10U);
    out->device_id[10] = '\0';
    for (const mdns_ip_addr_t *address = result->addr;
         address != NULL && out->address[0] == '\0';
         address = address->next) {
        if (address->addr.type != ESP_IPADDR_TYPE_V4) continue;
        const int written = snprintf(out->address, sizeof(out->address), IPSTR,
                                     IP2STR(&address->addr.u_addr.ip4));
        if (written <= 0 || (size_t)written >= sizeof(out->address)) return false;
    }
    copy_text(out->local_type, sizeof(out->local_type), txt_value(result, "type"));
    const char *const encrypted = txt_value(result, "encrypt");
    out->encrypted = encrypted != NULL && strcmp(encrypted, "true") == 0;
    return out->address[0] != '\0';
}

static void scan_devices(void)
{
    connectivity_diagnostic_status_t network = {0};
    connectivity_diagnostic_get_status(&network);
    if (!network.online) {
        taskENTER_CRITICAL(&s_lock);
        s_status.last_result = ESP_ERR_INVALID_STATE;
        s_status.device_count = 0U;
        s_status.scan_busy = false;
        s_status.scan_generation++;
        taskEXIT_CRITICAL(&s_lock);
        return;
    }

    if (!s_mdns_initialized) {
        const esp_err_t init_result = mdns_init();
        if (init_result == ESP_OK) {
            s_mdns_initialized = true;
            (void)mdns_hostname_set("novapanel");
        } else {
            taskENTER_CRITICAL(&s_lock);
            s_status.last_result = init_result;
            s_status.device_count = 0U;
            s_status.scan_busy = false;
            s_status.scan_generation++;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
    }

    mdns_result_t *results = NULL;
    const esp_err_t query_result = mdns_query_ptr(
        "_ewelink", "_tcp", SONOFF_LAN_SCAN_TIMEOUT_MS,
        SONOFF_LAN_MAX_DISCOVERED, &results);
    sonoff_lan_device_t found[SONOFF_LAN_MAX_DISCOVERED] = {0};
    uint8_t count = 0U;
    if (query_result == ESP_OK) {
        for (const mdns_result_t *result = results;
             result != NULL && count < SONOFF_LAN_MAX_DISCOVERED;
             result = result->next) {
            sonoff_lan_device_t candidate = {0};
            if (!parse_device(result, &candidate)) continue;
            bool duplicate = false;
            for (uint8_t i = 0U; i < count; ++i) {
                duplicate |= strcmp(found[i].device_id, candidate.device_id) == 0;
            }
            if (!duplicate) found[count++] = candidate;
        }
    }
    if (results != NULL) mdns_query_results_free(results);

    taskENTER_CRITICAL(&s_lock);
    memset(s_status.devices, 0, sizeof(s_status.devices));
    if (count > 0U) memcpy(s_status.devices, found, sizeof(found));
    s_status.device_count = count;
    s_status.last_result = query_result;
    s_status.scan_busy = false;
    s_status.scan_generation++;
    taskEXIT_CRITICAL(&s_lock);

    ESP_LOGI(TAG, "LAN discovery complete result=%s devices=%u",
             esp_err_to_name(query_result), (unsigned int)count);
    memset(found, 0, sizeof(found));
}

static void sonoff_lan_worker(void *arg)
{
    (void)arg;
    while (true) {
        bool requested = false;
        taskENTER_CRITICAL(&s_lock);
        requested = s_scan_requested;
        s_scan_requested = false;
        taskEXIT_CRITICAL(&s_lock);
        if (requested) scan_devices();
        vTaskDelay(pdMS_TO_TICKS(SONOFF_LAN_POLL_MS));
    }
}

esp_err_t sonoff_lan_service_start(void)
{
    if (s_started) return ESP_OK;
    const BaseType_t created = xTaskCreate(sonoff_lan_worker, "sonoff_lan",
                                            SONOFF_LAN_TASK_STACK_BYTES, NULL,
                                            SONOFF_LAN_TASK_PRIORITY, NULL);
    if (created != pdPASS) return ESP_ERR_NO_MEM;
    s_started = true;
    taskENTER_CRITICAL(&s_lock);
    s_status.ready = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t sonoff_lan_service_request_scan(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&s_lock);
    if (s_status.scan_busy || s_scan_requested) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_status.scan_busy = true;
    s_status.last_result = ESP_ERR_TIMEOUT;
    s_scan_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

void sonoff_lan_service_get_status(sonoff_lan_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
