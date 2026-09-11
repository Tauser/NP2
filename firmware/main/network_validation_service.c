/*
 * Bounded Phase 3 external-connectivity diagnostic.
 *
 * This is deliberately a maintenance test, not a provider framework. A
 * physically armed request runs DNS, one-shot SNTP, then one HTTPS HEAD in a
 * single task. It retains neither time nor credentials and accepts no URL.
 */
#include "network_validation_service.h"

#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#include "connectivity_diagnostic.h"

#define NP2_NETWORK_VALIDATION_TASK_STACK_BYTES (8U * 1024U)
#define NP2_NETWORK_VALIDATION_TASK_PRIORITY 2U
#define NP2_DNS_RESOLVER_TASK_STACK_BYTES (4U * 1024U)
#define NP2_NETWORK_VALIDATION_POLL_MS 100U
#define NP2_DNS_TIMEOUT_MS 5000U
#define NP2_NTP_TIMEOUT_MS 15000U
#define NP2_HTTPS_TIMEOUT_MS 10000U
#define NP2_REQUEST_TOTAL_TIMEOUT_MS 20000U
#define NP2_HTTPS_MAX_BODY_BYTES 512U
#define NP2_VALID_EPOCH_SECONDS 1735689600LL /* 2025-01-01 UTC */

static const char *const TAG = "np2_netcheck";
static const char *const NP2_NTP_HOST = "time.cloudflare.com";
static const char *const NP2_HTTPS_URL = "https://example.com/";
static const char *const NP2_NXDOMAIN_HOST = "np2-connectivity.invalid";
static const char *const NP2_TLS_REJECT_URL = "https://expired.badssl.com/";
static const char *const NP2_HTTPS_SLOW_URL = "https://httpbin.org/delay/15";
static const char *const NP2_HTTPS_OVERSIZE_URL = "https://httpbin.org/bytes/2048";

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static network_validation_status_t s_status = {
    .dns_result = ESP_ERR_INVALID_STATE,
    .ntp_result = ESP_ERR_INVALID_STATE,
    .https_result = ESP_ERR_INVALID_STATE,
};
static bool s_started;
static bool s_request_pending;
static network_validation_mode_t s_request_mode;

typedef struct {
    SemaphoreHandle_t done;
    char host[64];
    esp_err_t result;
    bool in_flight;
} dns_resolver_context_t;

typedef struct {
    size_t received_bytes;
    size_t maximum_bytes;
    bool exceeded;
} https_body_limit_context_t;

static dns_resolver_context_t s_dns_resolver;

static uint32_t elapsed_ms_since(int64_t start_us)
{
    const int64_t elapsed_us = esp_timer_get_time() - start_us;
    return elapsed_us <= 0 ? 0U : (uint32_t)(elapsed_us / 1000LL);
}

static uint32_t remaining_request_budget_ms(int64_t start_us)
{
    const uint32_t elapsed_ms = elapsed_ms_since(start_us);
    return elapsed_ms >= NP2_REQUEST_TOTAL_TIMEOUT_MS
               ? 0U
               : NP2_REQUEST_TOTAL_TIMEOUT_MS - elapsed_ms;
}

static bool has_valid_system_time(void)
{
    return (int64_t)time(NULL) >= NP2_VALID_EPOCH_SECONDS;
}

static void dns_resolver_task(void *arg)
{
    dns_resolver_context_t *const context = arg;
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *result = NULL;
    const int resolver_result = getaddrinfo(context->host, NULL, &hints, &result);
    if (result != NULL) {
        freeaddrinfo(result);
    }
    taskENTER_CRITICAL(&s_status_lock);
    context->result = resolver_result == 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
    context->in_flight = false;
    taskEXIT_CRITICAL(&s_status_lock);
    (void)xSemaphoreGive(context->done);
    vTaskDelete(NULL);
}

static esp_err_t validate_dns(const char *host)
{
    if (s_dns_resolver.done == NULL || host == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t host_length = strnlen(host, sizeof(s_dns_resolver.host));
    if (host_length == 0U || host_length >= sizeof(s_dns_resolver.host)) {
        return ESP_ERR_INVALID_ARG;
    }

    while (xSemaphoreTake(s_dns_resolver.done, 0) == pdTRUE) {
    }

    taskENTER_CRITICAL(&s_status_lock);
    const bool resolver_busy = s_dns_resolver.in_flight;
    if (!resolver_busy) {
        memcpy(s_dns_resolver.host, host, host_length);
        s_dns_resolver.host[host_length] = '\0';
        s_dns_resolver.in_flight = true;
        s_dns_resolver.result = ESP_ERR_TIMEOUT;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    if (resolver_busy) {
        return ESP_ERR_TIMEOUT;
    }

    const BaseType_t created = xTaskCreate(dns_resolver_task, "np2_dns",
                                           NP2_DNS_RESOLVER_TASK_STACK_BYTES,
                                           &s_dns_resolver,
                                           NP2_NETWORK_VALIDATION_TASK_PRIORITY,
                                           NULL);
    if (created != pdPASS) {
        taskENTER_CRITICAL(&s_status_lock);
        s_dns_resolver.in_flight = false;
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_dns_resolver.done, pdMS_TO_TICKS(NP2_DNS_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    taskENTER_CRITICAL(&s_status_lock);
    const esp_err_t result = s_dns_resolver.result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

static esp_err_t validate_dns_timeout(void)
{
    esp_netif_t *const station_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (station_netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_netif_dns_info_t original_dns = {0};
    esp_err_t result = esp_netif_get_dns_info(station_netif, ESP_NETIF_DNS_MAIN, &original_dns);
    if (result != ESP_OK) {
        return result;
    }

    esp_netif_dns_info_t timeout_dns = {0};
    timeout_dns.ip.type = ESP_IPADDR_TYPE_V4;
    IP4_ADDR(&timeout_dns.ip.u_addr.ip4, 192, 0, 2, 1);
    result = esp_netif_set_dns_info(station_netif, ESP_NETIF_DNS_MAIN, &timeout_dns);
    if (result == ESP_OK) {
        result = validate_dns(NP2_NTP_HOST);
    }
    const esp_err_t restore_result =
        esp_netif_set_dns_info(station_netif, ESP_NETIF_DNS_MAIN, &original_dns);
    if (result == ESP_OK) {
        result = ESP_FAIL;
    }
    return restore_result == ESP_OK ? result : restore_result;
}

static esp_err_t validate_ntp(void)
{
    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NP2_NTP_HOST);
    esp_err_t result = esp_netif_sntp_init(&config);
    if (result != ESP_OK) {
        return result;
    }

    result = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(NP2_NTP_TIMEOUT_MS));
    const bool time_valid = has_valid_system_time();
    esp_netif_sntp_deinit();
    return result == ESP_OK && time_valid ? ESP_OK : (result == ESP_OK ? ESP_FAIL : result);
}

static esp_err_t https_body_limit_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL || event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0 ||
        event->user_data == NULL) {
        return ESP_OK;
    }
    https_body_limit_context_t *const context = event->user_data;
    if (context->maximum_bytes == 0U) {
        return ESP_OK;
    }
    const size_t data_length = (size_t)event->data_len;
    if (context->received_bytes > context->maximum_bytes ||
        data_length > context->maximum_bytes - context->received_bytes) {
        context->exceeded = true;
    } else {
        context->received_bytes += data_length;
    }
    return ESP_OK;
}

static esp_err_t validate_https(const char *url, uint32_t timeout_ms,
                                esp_http_client_method_t method, size_t maximum_body_bytes)
{
    if (timeout_ms == 0U) {
        return ESP_ERR_TIMEOUT;
    }
    https_body_limit_context_t body_context = {
        .maximum_bytes = maximum_body_bytes,
    };
    const esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = timeout_ms > NP2_HTTPS_TIMEOUT_MS
                          ? NP2_HTTPS_TIMEOUT_MS
                          : timeout_ms,
        .disable_auto_redirect = true,
        .max_redirection_count = 0,
        .buffer_size = 512,
        .buffer_size_tx = 512,
        .event_handler = https_body_limit_event_handler,
        .user_data = &body_context,
        .crt_bundle_attach = esp_crt_bundle_attach,
#if CONFIG_MBEDTLS_DYNAMIC_BUFFER
        .tls_dyn_buf_strategy = HTTP_TLS_DYN_BUF_RX_STATIC,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const esp_err_t result = esp_http_client_perform(client);
    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK) {
        return result;
    }
    if (body_context.exceeded) {
        return ESP_ERR_INVALID_SIZE;
    }
    return status_code >= 200 && status_code < 400 ? ESP_OK : ESP_FAIL;
}

static void publish_results(esp_err_t dns_result, esp_err_t ntp_result,
                            esp_err_t https_result, uint32_t duration_ms)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status.busy = false;
    s_status.time_synced = ntp_result == ESP_OK;
    s_status.dns_result = dns_result;
    s_status.ntp_result = ntp_result;
    s_status.https_result = https_result;
    s_status.last_duration_ms = duration_ms;
    ++s_status.completed_checks;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void network_validation_task(void *arg)
{
    (void)arg;
    for (;;) {
        bool request_pending = false;
        network_validation_mode_t mode = NETWORK_VALIDATION_MODE_NORMAL;
        taskENTER_CRITICAL(&s_status_lock);
        request_pending = s_request_pending;
        mode = s_request_mode;
        s_request_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);
        if (!request_pending) {
            vTaskDelay(pdMS_TO_TICKS(NP2_NETWORK_VALIDATION_POLL_MS));
            continue;
        }

        const int64_t start_us = esp_timer_get_time();
        esp_err_t dns_result;
        if (mode == NETWORK_VALIDATION_MODE_DNS_NXDOMAIN) {
            dns_result = validate_dns(NP2_NXDOMAIN_HOST);
        } else if (mode == NETWORK_VALIDATION_MODE_DNS_TIMEOUT) {
            dns_result = validate_dns_timeout();
        } else {
            dns_result = validate_dns(NP2_NTP_HOST);
        }
        connectivity_diagnostic_report_dns_result(dns_result);
        esp_err_t ntp_result = ESP_ERR_INVALID_STATE;
        esp_err_t https_result = ESP_ERR_INVALID_STATE;
        if (dns_result == ESP_OK && mode != NETWORK_VALIDATION_MODE_DNS_NXDOMAIN &&
            mode != NETWORK_VALIDATION_MODE_DNS_TIMEOUT) {
            ntp_result = validate_ntp();
        }
        const uint32_t https_budget_ms = remaining_request_budget_ms(start_us);
        if (ntp_result == ESP_OK && https_budget_ms > 0U) {
            const bool slow_https = mode == NETWORK_VALIDATION_MODE_HTTPS_TIMEOUT;
            const bool oversize_https = mode == NETWORK_VALIDATION_MODE_HTTPS_OVERSIZE;
            https_result = validate_https(mode == NETWORK_VALIDATION_MODE_TLS_REJECT
                                              ? NP2_TLS_REJECT_URL
                                              : (slow_https ? NP2_HTTPS_SLOW_URL
                                                            : (oversize_https
                                                                   ? NP2_HTTPS_OVERSIZE_URL
                                                                   : NP2_HTTPS_URL)),
                                          https_budget_ms,
                                          slow_https || oversize_https
                                              ? HTTP_METHOD_GET
                                              : HTTP_METHOD_HEAD,
                                          oversize_https ? NP2_HTTPS_MAX_BODY_BYTES : 0U);
        } else if (ntp_result == ESP_OK) {
            https_result = ESP_ERR_TIMEOUT;
        }

        const uint32_t duration_ms = elapsed_ms_since(start_us);
        if (duration_ms > NP2_REQUEST_TOTAL_TIMEOUT_MS) {
            https_result = ESP_ERR_TIMEOUT;
        }
        publish_results(dns_result, ntp_result, https_result, duration_ms);
        ESP_LOGI(TAG, "maintenance check complete mode=%u dns=%s ntp=%s https=%s duration=%lums",
                 (unsigned int)mode,
                 esp_err_to_name(dns_result), esp_err_to_name(ntp_result),
                 esp_err_to_name(https_result), (unsigned long)duration_ms);
    }
}

esp_err_t network_validation_service_start(void)
{
    taskENTER_CRITICAL(&s_status_lock);
    if (s_started) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    taskEXIT_CRITICAL(&s_status_lock);

    s_dns_resolver.done = xSemaphoreCreateBinary();
    if (s_dns_resolver.done == NULL) {
        taskENTER_CRITICAL(&s_status_lock);
        s_started = false;
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_NO_MEM;
    }

    const BaseType_t created = xTaskCreate(network_validation_task, "np2_netcheck",
                                           NP2_NETWORK_VALIDATION_TASK_STACK_BYTES, NULL,
                                           NP2_NETWORK_VALIDATION_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        vSemaphoreDelete(s_dns_resolver.done);
        s_dns_resolver.done = NULL;
        taskENTER_CRITICAL(&s_status_lock);
        s_started = false;
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t network_validation_service_request_check(network_validation_mode_t mode)
{
    connectivity_diagnostic_status_t connectivity = {0};
    connectivity_diagnostic_get_status(&connectivity);
    if (!s_started || !connectivity.online || mode > NETWORK_VALIDATION_MODE_HTTPS_OVERSIZE) {
        return ESP_ERR_INVALID_STATE;
    }
    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.busy || s_request_pending) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_request_pending = true;
    s_request_mode = mode;
    s_status.busy = true;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

void network_validation_service_get_status(network_validation_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    taskENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
}
