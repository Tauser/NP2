/*
 * Bounded Phase 3 external-connectivity diagnostic.
 *
 * This is deliberately a maintenance test, not a provider framework. A
 * physically armed request runs DNS, one-shot SNTP, then one HTTPS HEAD in a
 * single task. It retains neither time nor credentials and accepts no URL.
 */
#include "network_validation_service.h"
#include "esp_hosted.h"
#include "esp_ota_ops.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"

#include <string.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#include "connectivity_diagnostic.h"
#include "app_event_bus.h"
#include "data_refresh_scheduler.h"
#include "flash_coordinator.h"
#include "offline_data_codec.h"
#include "offline_data_provider.h"
#include "time_service.h"
#include "np_ewelink.h"
#include "weather_condition.h"
#include "update_admission.h"
#include "update_journal.h"
#include "update_p4_writer.h"

#define NP2_NETWORK_VALIDATION_TASK_STACK_BYTES (8U * 1024U)
#define NP2_NETWORK_VALIDATION_TASK_PRIORITY 2U
#define NP2_DNS_RESOLVER_TASK_STACK_BYTES (4U * 1024U)
#define NP2_NETWORK_VALIDATION_POLL_MS 100U
#define NP2_DNS_TIMEOUT_MS 5000U
#define NP2_HTTPS_TIMEOUT_MS 10000U
#define NP2_HTTPS_READ_TIMEOUT_MS 5000U
#define NP2_REQUEST_TOTAL_TIMEOUT_MS 20000U
#define NP2_PRODUCT_CACHE_WRITE_INTERVAL_US (30LL * 60LL * 1000LL * 1000LL)
#define NP2_TIME_SYNC_INTERVAL_S UINT32_C(21600)
#define NP2_HTTPS_MAX_BODY_BYTES 512U
#define NP2_PROVIDER_MAX_BODY_BYTES OFFLINE_MARKET_MAX_BODY_BYTES
#define NP2_UPDATE_URL_MAX_BYTES 192U
#define NP2_UPDATE_TOTAL_TIMEOUT_MS (20U * 60U * 1000U)
#define NP2_UPDATE_OPERATION_TIMEOUT_MS 15000U
#define NP2_UPDATE_JOURNAL_TIMEOUT_MS 20000U
#define NP2_VALID_EPOCH_SECONDS 1735689600LL /* 2025-01-01 UTC */
/* The Home sparkline is a short-term trend. A gap larger than this resets the
 * local series so disconnected periods are never joined by a misleading line. */
#define NP2_MARKET_HISTORY_MAX_GAP_S UINT32_C(300)
#define NP2_MARKET_CONTROL_PROBE_COOLDOWN_US (10LL * 60LL * 1000LL * 1000LL)

static const char *const TAG = "np2_netcheck";
static const char *const NP2_HTTPS_URL = "https://example.com/";
static const char *const NP2_NXDOMAIN_HOST = "np2-connectivity.invalid";
static const char *const NP2_TLS_REJECT_URL = "https://expired.badssl.com/";
static const char *const NP2_HTTPS_SLOW_URL = "https://httpbin.org/delay/15";
static const char *const NP2_HTTPS_OVERSIZE_URL = "https://httpbin.org/bytes/2048";
/*
 * Brasília-DF, selected by the product owner. These are fixed query-only
 * endpoints: no user location, credential, URL or provider response leaves
 * this worker.
 */
static const char *const NP2_OPEN_METEO_BRASILIA_URL =
    "https://api.open-meteo.com/v1/forecast?latitude=-15.793889&longitude=-47.882778"
    "&current=temperature_2m,relative_humidity_2m,weather_code,apparent_temperature,"
    "wind_speed_10m,wind_direction_10m,uv_index"
    "&hourly=temperature_2m,weather_code,precipitation_probability&forecast_hours=5"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_sum"
    "&forecast_days=5&timezone=America%2FSao_Paulo&timeformat=unixtime";
static const char *const NP2_COINGECKO_BITCOIN_URL =
    "https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd"
    "&ids=bitcoin,ethereum,solana,binancecoin,ripple&order=market_cap_desc"
    "&per_page=5&page=1&sparkline=false";
static const char *const NP2_BCB_USD_BRL_URL =
    "https://api.bcb.gov.br/dados/serie/bcdata.sgs.1/dados/ultimos/2?formato=json";
static const char *const NP2_ALTERNATIVE_ME_FEAR_GREED_URL =
    "https://api.alternative.me/fng/?limit=1&format=json";
static const char *const NP2_BRAPI_MARKET_INDEX_URLS[] = {
    "https://brapi.dev/api/quote/%5EBVSP",
    "https://brapi.dev/api/quote/%5EGSPC",
    "https://brapi.dev/api/quote/%5EIXIC",
};

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static network_validation_status_t s_status = {
    .dns_result = ESP_ERR_INVALID_STATE,
    .ntp_result = ESP_ERR_INVALID_STATE,
    .https_result = ESP_ERR_INVALID_STATE,
};
static bool s_started;
static bool s_request_pending;
static network_validation_mode_t s_request_mode;
static char s_update_manifest_url[NP2_UPDATE_URL_MAX_BYTES];
static char s_update_signature_url[NP2_UPDATE_URL_MAX_BYTES];
static char s_update_image_url[NP2_UPDATE_URL_MAX_BYTES];
static update_keyring_entry_t s_update_keyring_entries[NETWORK_P4_UPDATE_MAX_TRUSTED_KEYS];
static size_t s_update_keyring_entries_count;
static update_environment_t s_update_environment;
static uint8_t s_update_stream_chunk[UPDATE_IMAGE_HASH_CHUNK_MAX_BYTES];
/* Only the HTTPS worker mutates this snapshot. The app_loop owns its separate
 * projected copy, received through the bounded EventBus. */
static offline_data_snapshot_t s_product_snapshot;
static bool s_product_snapshot_initialized;
/* Owned exclusively by the HTTPS worker; retain newest data on bus saturation. */
static bool s_product_delivery_pending;
static int64_t s_last_product_cache_write_us;
/* Owned by the sole HTTPS worker; kept out of its 8 KiB task stack. */
static uint8_t s_product_body[NP2_PROVIDER_MAX_BODY_BYTES];

typedef struct {
    SemaphoreHandle_t done;
    char host[64];
    esp_err_t result;
    bool in_flight;
} dns_resolver_context_t;

static dns_resolver_context_t s_dns_resolver;

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) {
        *bytes++ = 0U;
    }
}

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
        result = validate_dns(TIME_SERVICE_NTP_HOST);
    }
    const esp_err_t restore_result =
        esp_netif_set_dns_info(station_netif, ESP_NETIF_DNS_MAIN, &original_dns);
    if (result == ESP_OK) {
        result = ESP_FAIL;
    }
    return restore_result == ESP_OK ? result : restore_result;
}

static uint32_t remaining_phase_budget_ms(int64_t start_us, uint32_t budget_ms)
{
    const uint32_t elapsed_ms = elapsed_ms_since(start_us);
    return elapsed_ms >= budget_ms ? 0U : budget_ms - elapsed_ms;
}

static esp_err_t set_client_budget(esp_http_client_handle_t client, int64_t start_us,
                                   uint32_t budget_ms, uint32_t operation_timeout_ms)
{
    uint32_t remaining_ms = remaining_phase_budget_ms(start_us, budget_ms);
    if (remaining_ms == 0U) {
        return ESP_ERR_TIMEOUT;
    }
    if (remaining_ms > operation_timeout_ms) {
        remaining_ms = operation_timeout_ms;
    }
    return esp_http_client_set_timeout_ms(client, (int)remaining_ms);
}

static esp_err_t perform_https_request_with_header(
    const char *url, uint32_t timeout_ms, esp_http_client_method_t method,
    const char *header_name, const char *header_value, size_t maximum_body_bytes,
    uint8_t *out_body, size_t out_body_size, size_t *out_body_length)
{
    if (out_body_length != NULL) {
        *out_body_length = 0U;
    }
    if (url == NULL || timeout_ms == 0U ||
        ((header_name == NULL) != (header_value == NULL)) ||
        ((out_body == NULL) != (out_body_length == NULL)) ||
        (out_body != NULL && out_body_size < maximum_body_bytes)) {
        return timeout_ms == 0U ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_ARG;
    }
    if (maximum_body_bytes > NP2_PROVIDER_MAX_BODY_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* Never log the URL or headers: OTA URLs may contain private query data. */
    const char *provider = "diagnostic-or-update";
    if (url == NP2_COINGECKO_BITCOIN_URL) provider = "coingecko";
    else if (url == NP2_OPEN_METEO_BRASILIA_URL) provider = "open-meteo";
    else if (url == NP2_BCB_USD_BRL_URL) provider = "bcb";
    else if (url == NP2_ALTERNATIVE_ME_FEAR_GREED_URL) provider = "alternative-me";
    else if (url == NP2_HTTPS_URL) provider = "control";
    else {
        for (size_t index = 0U; index < sizeof(NP2_BRAPI_MARKET_INDEX_URLS) /
                                           sizeof(NP2_BRAPI_MARKET_INDEX_URLS[0]); ++index) {
            if (url == NP2_BRAPI_MARKET_INDEX_URLS[index]) provider = "brapi";
        }
    }
    const int64_t start_us = esp_timer_get_time();
    const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t internal_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "HTTPS begin provider=%s budget=%lums internal_free=%u largest=%u",
             provider, (unsigned long)timeout_ms, (unsigned int)internal_free,
             (unsigned int)internal_largest);
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
        .crt_bundle_attach = esp_crt_bundle_attach,
#if CONFIG_MBEDTLS_DYNAMIC_BUFFER
        .tls_dyn_buf_strategy = HTTP_TLS_DYN_BUF_RX_STATIC,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGW(TAG, "HTTPS complete provider=%s phase=init result=ESP_ERR_NO_MEM", provider);
        return ESP_ERR_NO_MEM;
    }

    const char *phase = "configure";
    esp_err_t result = ESP_OK;
    if (header_name != NULL) {
        result = esp_http_client_set_header(client, header_name, header_value);
    }
    if (result == ESP_OK) {
        result = set_client_budget(client, start_us, timeout_ms, NP2_HTTPS_TIMEOUT_MS);
    }
    if (result == ESP_OK) {
        phase = "connect-tls";
        result = esp_http_client_open(client, 0);
    }

    int64_t content_length = -1;
    if (result == ESP_OK) {
        result = set_client_budget(client, start_us, timeout_ms, NP2_HTTPS_TIMEOUT_MS);
    }
    if (result == ESP_OK) {
        phase = "headers";
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            result = content_length == -ESP_ERR_HTTP_EAGAIN ? ESP_ERR_TIMEOUT
                                                            : ESP_ERR_HTTP_FETCH_HEADER;
        }
    }

    const int status_code = result == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    if (result == ESP_OK && (status_code < 200 || status_code >= 300)) {
        ESP_LOGW(TAG, "HTTPS rejected status=%d content_length=%lld",
                 status_code, (long long)content_length);
    }
    if (result == ESP_OK && maximum_body_bytes > 0U && content_length > 0 &&
        (uint64_t)content_length > maximum_body_bytes) {
        result = ESP_ERR_INVALID_SIZE;
    }

    size_t received_bytes = 0U;
    uint8_t read_buffer[256];
    while (result == ESP_OK && method != HTTP_METHOD_HEAD) {
        phase = "body";
        result = set_client_budget(client, start_us, timeout_ms,
                                   NP2_HTTPS_READ_TIMEOUT_MS);
        if (result != ESP_OK) {
            break;
        }
        size_t read_size = sizeof(read_buffer);
        if (maximum_body_bytes > 0U) {
            if (received_bytes > maximum_body_bytes) {
                result = ESP_ERR_INVALID_SIZE;
                break;
            }
            const size_t remaining_body_bytes = maximum_body_bytes - received_bytes;
            read_size = remaining_body_bytes < sizeof(read_buffer) ? remaining_body_bytes + 1U
                                                                    : sizeof(read_buffer);
        }
        const int read_result =
            esp_http_client_read(client, (char *)read_buffer, (int)read_size);
        if (read_result == -ESP_ERR_HTTP_EAGAIN) {
            result = ESP_ERR_TIMEOUT;
        } else if (read_result < 0) {
            result = ESP_FAIL;
        } else if (read_result == 0) {
            break;
        } else {
            const size_t read_bytes = (size_t)read_result;
            if (maximum_body_bytes > 0U &&
                (read_bytes > maximum_body_bytes - received_bytes)) {
                result = ESP_ERR_INVALID_SIZE;
            } else {
                if (out_body != NULL) {
                    memcpy(&out_body[received_bytes], read_buffer, read_bytes);
                }
                received_bytes += read_bytes;
            }
        }
    }
    if (result == ESP_OK && method != HTTP_METHOD_HEAD && content_length > 0 &&
        received_bytes < (uint64_t)content_length) {
        result = ESP_ERR_HTTP_INCOMPLETE_DATA;
    }

    /* Capture transport details before close/cleanup discards the error handle. */
    int tls_code = 0;
    int tls_flags = 0;
    const esp_err_t tls_result =
        esp_http_client_get_and_clear_last_tls_error(client, &tls_code, &tls_flags);
    const int socket_errno = esp_http_client_get_errno(client);
    const esp_err_t final_result = result != ESP_OK ? result
                                  : (status_code >= 200 && status_code < 300 ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "HTTPS complete provider=%s phase=%s result=%s status=%d bytes=%u "
                 "tls=0x%x code=%d flags=0x%x errno=%d duration=%lums internal_free=%u largest=%u",
             provider, phase, esp_err_to_name(final_result), status_code, (unsigned int)received_bytes,
             (unsigned int)tls_result, tls_code, (unsigned int)tls_flags, socket_errno,
             (unsigned long)elapsed_ms_since(start_us),
             (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (result != ESP_OK) {
        return result;
    }
    if (status_code < 200 || status_code >= 300) {
        return ESP_FAIL;
    }
    if (out_body_length != NULL) {
        *out_body_length = received_bytes;
    }
    return ESP_OK;
}

static esp_err_t perform_https_request(const char *url, uint32_t timeout_ms,
                                       esp_http_client_method_t method, size_t maximum_body_bytes,
                                       uint8_t *out_body, size_t out_body_size,
                                       size_t *out_body_length)
{
    return perform_https_request_with_header(
        url, timeout_ms, method, NULL, NULL, maximum_body_bytes,
        out_body, out_body_size, out_body_length);
}

static esp_err_t perform_coingecko_request(const char *url, uint32_t timeout_ms,
                                           size_t maximum_body_bytes,
                                           uint8_t *out_body, size_t out_body_size,
                                           size_t *out_body_length)
{
    char api_key[FLASH_COORDINATOR_COINGECKO_API_KEY_BYTES] = {0};
    esp_err_t result =
        flash_coordinator_copy_coingecko_api_key(api_key, sizeof(api_key));
    if (result != ESP_OK) {
        secure_zero(api_key, sizeof(api_key));
        ESP_LOGE(TAG, "CoinGecko API key unavailable: %s", esp_err_to_name(result));
        return result;
    }

    result = perform_https_request_with_header(
        url, timeout_ms, HTTP_METHOD_GET, "x-cg-demo-api-key", api_key,
        maximum_body_bytes, out_body, out_body_size, out_body_length);

    secure_zero(api_key, sizeof(api_key));
    return result;
}

static esp_err_t perform_brapi_request(const char *url, uint32_t timeout_ms,
                                       size_t maximum_body_bytes, uint8_t *out_body,
                                       size_t out_body_size, size_t *out_body_length)
{
    char api_key[FLASH_COORDINATOR_BRAPI_API_KEY_BYTES] = {0};
    esp_err_t result = flash_coordinator_copy_brapi_api_key(api_key, sizeof(api_key));
    if (result != ESP_OK) {
        secure_zero(api_key, sizeof(api_key));
        ESP_LOGW(TAG, "Brapi API key unavailable: %s", esp_err_to_name(result));
        return result;
    }
    char authorization[FLASH_COORDINATOR_BRAPI_API_KEY_BYTES + 8U] = {0};
    const int written = snprintf(authorization, sizeof(authorization), "Bearer %s", api_key);
    secure_zero(api_key, sizeof(api_key));
    if (written < 0 || (size_t)written >= sizeof(authorization)) {
        secure_zero(authorization, sizeof(authorization));
        return ESP_ERR_INVALID_SIZE;
    }
    result = perform_https_request_with_header(
        url, timeout_ms, HTTP_METHOD_GET, "Authorization", authorization,
        maximum_body_bytes, out_body, out_body_size, out_body_length);
    secure_zero(authorization, sizeof(authorization));
    return result;
}

static esp_err_t validate_https(const char *url, uint32_t timeout_ms,
                                esp_http_client_method_t method, size_t maximum_body_bytes)
{
    return perform_https_request(url, timeout_ms, method, maximum_body_bytes,
                                 NULL, 0U, NULL);
}

static esp_err_t stream_update_image(const char *url, uint32_t expected_bytes,
                                     update_p4_writer_t *writer,
                                     update_image_hash_session_t *hash)
{
    if (url == NULL || expected_bytes == 0U || writer == NULL || hash == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const int64_t start_us = esp_timer_get_time();
    const esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = NP2_UPDATE_OPERATION_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .max_redirection_count = 0,
        .buffer_size = 512,
        .buffer_size_tx = 512,
        .crt_bundle_attach = esp_crt_bundle_attach,
#if CONFIG_MBEDTLS_DYNAMIC_BUFFER
        .tls_dyn_buf_strategy = HTTP_TLS_DYN_BUF_RX_STATIC,
#endif
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = set_client_budget(client, start_us, NP2_UPDATE_TOTAL_TIMEOUT_MS,
                                         NP2_UPDATE_OPERATION_TIMEOUT_MS);
    if (result == ESP_OK) {
        result = esp_http_client_open(client, 0);
    }
    int64_t content_length = -1;
    if (result == ESP_OK) {
        result = set_client_budget(client, start_us, NP2_UPDATE_TOTAL_TIMEOUT_MS,
                                   NP2_UPDATE_OPERATION_TIMEOUT_MS);
    }
    if (result == ESP_OK) {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            result = content_length == -ESP_ERR_HTTP_EAGAIN ? ESP_ERR_TIMEOUT
                                                            : ESP_ERR_HTTP_FETCH_HEADER;
        } else if ((uint64_t)content_length != expected_bytes) {
            result = ESP_ERR_INVALID_SIZE;
        }
    }
    if (result == ESP_OK) {
        const int status = esp_http_client_get_status_code(client);
        if (status < 200 || status >= 300) {
            result = ESP_FAIL;
        }
    }
    uint32_t received = 0U;
    while (result == ESP_OK && received < expected_bytes) {
        result = set_client_budget(client, start_us, NP2_UPDATE_TOTAL_TIMEOUT_MS,
                                   NP2_UPDATE_OPERATION_TIMEOUT_MS);
        if (result != ESP_OK) {
            break;
        }
        const int read = esp_http_client_read(client, (char *)s_update_stream_chunk,
                                              sizeof(s_update_stream_chunk));
        if (read == -ESP_ERR_HTTP_EAGAIN) {
            result = ESP_ERR_TIMEOUT;
        } else if (read <= 0) {
            result = read == 0 ? ESP_ERR_HTTP_INCOMPLETE_DATA : ESP_FAIL;
        } else if ((uint32_t)read > expected_bytes - received ||
                   update_p4_writer_append(writer, hash, s_update_stream_chunk,
                                           (size_t)read) != UPDATE_P4_WRITER_OK) {
            result = ESP_FAIL;
        } else {
            received += (uint32_t)read;
        }
    }
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result == ESP_OK && received == expected_bytes ? ESP_OK
                                                           : (result == ESP_OK ? ESP_ERR_HTTP_INCOMPLETE_DATA : result);
}

static esp_err_t persist_update_journal_and_wait(const update_journal_record_t *record,
                                                  uint32_t *out_generation)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    flash_coordinator_status_t before = {0};
    flash_coordinator_get_status(&before);
    const uint32_t previous_generation = before.update_journal_generation;
    const esp_err_t request_result = flash_coordinator_request_update_journal(record);
    if (request_result != ESP_OK) {
        return request_result;
    }
    const int64_t deadline = esp_timer_get_time() +
                             (int64_t)NP2_UPDATE_JOURNAL_TIMEOUT_MS * 1000LL;
    for (;;) {
        flash_coordinator_status_t after = {0};
        flash_coordinator_get_status(&after);
        if (!after.busy && !after.pending) {
            if (after.last_result != ESP_OK) {
                return after.last_result;
            }
            if (after.update_journal_valid && after.update_journal_state == record->state &&
                after.update_journal_generation > previous_generation) {
                if (out_generation != NULL) {
                    *out_generation = after.update_journal_generation;
                }
                return ESP_OK;
            }
            return ESP_FAIL;
        }
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(NP2_NETWORK_VALIDATION_POLL_MS));
    }
}

static esp_err_t provider_result_to_esp_err(offline_provider_result_t result)
{
    switch (result) {
    case OFFLINE_PROVIDER_OK:
        return ESP_OK;
    case OFFLINE_PROVIDER_BODY_TOO_LARGE:
        return ESP_ERR_INVALID_SIZE;
    case OFFLINE_PROVIDER_INVALID_ARGUMENT:
        return ESP_ERR_INVALID_ARG;
    case OFFLINE_PROVIDER_MALFORMED_RESPONSE:
    case OFFLINE_PROVIDER_OUT_OF_RANGE:
    default:
        return ESP_ERR_INVALID_RESPONSE;
    }
}

static void retain_weather_as_stale(offline_data_snapshot_t *snapshot)
{
    if (snapshot != NULL && snapshot->weather.available) {
        snapshot->weather.stale = true;
    }
}

static void retain_market_as_stale(offline_data_snapshot_t *snapshot)
{
    if (snapshot != NULL && snapshot->market.available) {
        snapshot->market.stale = true;
    }
}

static void append_market_history(offline_market_data_t *fresh,
                                  const offline_market_data_t *previous)
{
    if (fresh == NULL || !fresh->available || fresh->bitcoin_usd_cents == 0U) {
        return;
    }

    uint8_t count = 0U;
    bool continuation = false;
    if (previous != NULL && previous->available && previous->observed_at_unix_s != 0U &&
        fresh->observed_at_unix_s > previous->observed_at_unix_s) {
        const uint32_t gap_s = fresh->observed_at_unix_s - previous->observed_at_unix_s;
        continuation = gap_s <= NP2_MARKET_HISTORY_MAX_GAP_S;
    }

    if (continuation) {
        count = previous->history_count;
        if (count > OFFLINE_MARKET_HISTORY_MAX) {
            count = OFFLINE_MARKET_HISTORY_MAX;
        }
        if (count > 0U) {
            memcpy(fresh->history_usd_cents, previous->history_usd_cents,
                   (size_t)count * sizeof(fresh->history_usd_cents[0]));
        } else if (previous->bitcoin_usd_cents != 0U) {
            fresh->history_usd_cents[0] = previous->bitcoin_usd_cents;
            count = 1U;
        }
    }

    if (count < OFFLINE_MARKET_HISTORY_MAX) {
        fresh->history_usd_cents[count++] = fresh->bitcoin_usd_cents;
    } else {
        memmove(&fresh->history_usd_cents[0], &fresh->history_usd_cents[1],
                (OFFLINE_MARKET_HISTORY_MAX - 1U) *
                    sizeof(fresh->history_usd_cents[0]));
        fresh->history_usd_cents[OFFLINE_MARKET_HISTORY_MAX - 1U] =
            fresh->bitcoin_usd_cents;
    }
    fresh->history_count = count;
}

static void retain_exchange_as_stale(offline_data_snapshot_t *snapshot)
{
    if (snapshot != NULL && snapshot->exchange.available) {
        snapshot->exchange.stale = true;
    }
}

static void retain_market_overview_as_stale(offline_data_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    for (size_t index = 0U; index < OFFLINE_MARKET_ALTCOIN_COUNT; ++index)
        if (snapshot->altcoins[index].available) snapshot->altcoins[index].stale = true;
}

static void retain_fear_greed_as_stale(offline_data_snapshot_t *snapshot)
{
    if (snapshot != NULL && snapshot->fear_greed.available)
        snapshot->fear_greed.stale = true;
}

/* The snapshot persists the last confirmed period, not a clock. A later boot
 * can therefore restore the exact visual choice without treating a stale
 * timestamp as current time. */
static void update_weather_visual(offline_data_snapshot_t *snapshot, time_t now)
{
    if (snapshot == NULL || !snapshot->weather.available) {
        return;
    }
    struct tm local = {0};
    if (localtime_r(&now, &local) == NULL || local.tm_hour < 0 || local.tm_hour > 23) {
        return;
    }
    snapshot->weather_visual = (offline_weather_visual_t){
        .available = true,
        .is_day = weather_condition_is_day((uint8_t)local.tm_hour),
    };
}

static void ensure_product_snapshot(void)
{
    if (s_product_snapshot_initialized) return;
    flash_coordinator_status_t storage = {0};
    flash_coordinator_get_status(&storage);
    s_product_snapshot = storage.offline_data_valid ? storage.offline_data
                                                     : (offline_data_snapshot_t){0};
    s_product_snapshot.schema_version = OFFLINE_DATA_SCHEMA_VERSION;
    s_product_snapshot.origin = OFFLINE_DATA_ORIGIN_LIVE;
    s_product_snapshot_initialized = true;
}

static void publish_product_snapshot(void)
{
    if (!offline_data_snapshot_is_valid(&s_product_snapshot)) {
        s_product_delivery_pending = false;
        ESP_LOGW(TAG, "product-data delivery rejected: invalid snapshot");
        return;
    }
    const esp_err_t event_result = app_event_bus_post_product_data(&s_product_snapshot);
    s_product_delivery_pending = event_result != ESP_OK;
    ESP_LOGI(TAG, "product-data delivery result=%s market_stale=%u weather_stale=%u "
                 "exchange_stale=%u index_stale=%u",
             esp_err_to_name(event_result), (unsigned int)s_product_snapshot.market.stale,
             (unsigned int)s_product_snapshot.weather.stale,
             (unsigned int)s_product_snapshot.exchange.stale,
             (unsigned int)s_product_snapshot.ibovespa.stale);
    if (event_result != ESP_OK) {
        ESP_LOGW(TAG, "product-data event deferred: %s", esp_err_to_name(event_result));
    }
    const int64_t now_us = esp_timer_get_time();
    if (s_last_product_cache_write_us != 0LL &&
        now_us - s_last_product_cache_write_us < NP2_PRODUCT_CACHE_WRITE_INTERVAL_US) {
        return;
    }
    const esp_err_t persist_result =
        flash_coordinator_request_offline_data_write(&s_product_snapshot);
    if (persist_result == ESP_OK || persist_result == ESP_ERR_TIMEOUT) {
        /* ESP_ERR_TIMEOUT here means the coordinator's own write throttle. */
        s_last_product_cache_write_us = now_us;
    } else if (persist_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "product-data cache deferred: %s", esp_err_to_name(persist_result));
    }
}

static void retry_pending_product_delivery(void)
{
    if (!s_product_delivery_pending) return;
    if (app_event_bus_post_product_data(&s_product_snapshot) == ESP_OK) {
        s_product_delivery_pending = false;
        ESP_LOGI(TAG, "product-data deferred delivery completed");
    }
}

/* This runs only in the sole HTTPS worker, after DNS and trusted time. */
static esp_err_t refresh_product_domain(data_refresh_domain_t domain, int64_t request_start_us)
{
    ensure_product_snapshot();

    const time_t now = time(NULL);
    if (now < (time_t)NP2_VALID_EPOCH_SECONDS || (uint64_t)now > UINT32_MAX) {
        return ESP_ERR_INVALID_STATE;
    }
    update_weather_visual(&s_product_snapshot, now);

    uint8_t *const body = s_product_body;
    size_t body_size = 0U;
    esp_err_t result = ESP_ERR_INVALID_ARG;
    switch (domain) {
    case DATA_REFRESH_DOMAIN_WEATHER:
        result = perform_https_request(NP2_OPEN_METEO_BRASILIA_URL,
                                       remaining_request_budget_ms(request_start_us),
                                       HTTP_METHOD_GET, OFFLINE_WEATHER_MAX_BODY_BYTES, body,
                                       NP2_PROVIDER_MAX_BODY_BYTES, &body_size);
        if (result == ESP_OK) {
            result = provider_result_to_esp_err(offline_open_meteo_parse_forecast(
                body, body_size, (uint32_t)now, &s_product_snapshot.weather));
        }
        if (result == ESP_OK) {
            s_product_snapshot.weather.stale = false;
            update_weather_visual(&s_product_snapshot, now);
        }
        else retain_weather_as_stale(&s_product_snapshot);
        break;
    case DATA_REFRESH_DOMAIN_BITCOIN: {
        const offline_market_data_t previous_market = s_product_snapshot.market;
        offline_market_data_t fresh_market = {0};

        result = perform_coingecko_request(
            NP2_COINGECKO_BITCOIN_URL,
            remaining_request_budget_ms(request_start_us),
            OFFLINE_MARKET_MAX_BODY_BYTES, body,
            NP2_PROVIDER_MAX_BODY_BYTES, &body_size);
        offline_altcoin_data_t fresh_altcoins[OFFLINE_MARKET_ALTCOIN_COUNT] = {0};
        if (result == ESP_OK) result = provider_result_to_esp_err(
            offline_coingecko_parse_market_overview(body, body_size, (uint32_t)now,
                                                     &fresh_market, fresh_altcoins));
        if (result == ESP_OK) {
            append_market_history(&fresh_market, &previous_market);
            fresh_market.stale = false;
            s_product_snapshot.market = fresh_market;
            memcpy(s_product_snapshot.altcoins, fresh_altcoins, sizeof(fresh_altcoins));
        } else {
            retain_market_as_stale(&s_product_snapshot);
            retain_market_overview_as_stale(&s_product_snapshot);
            /* A bounded, read-only control separates provider failure from loss
             * of all external TLS. Keep the original market result and cadence.
             * The previous client has already been destroyed; no overlap. */
            static int64_t next_control_probe_us;
            const int64_t probe_now_us = esp_timer_get_time();
            const uint32_t probe_budget_ms = remaining_request_budget_ms(request_start_us);
            if ((result == ESP_ERR_HTTP_CONNECT || result == ESP_ERR_TIMEOUT) &&
                probe_now_us >= next_control_probe_us && probe_budget_ms >= 2000U) {
                next_control_probe_us = probe_now_us + NP2_MARKET_CONTROL_PROBE_COOLDOWN_US;
                const esp_err_t control_result = validate_https(
                    NP2_HTTPS_URL, probe_budget_ms, HTTP_METHOD_HEAD, 0U);
                ESP_LOGI(TAG, "market failure control=%s market=%s",
                         esp_err_to_name(control_result), esp_err_to_name(result));
            }
        }
        break;
    }
    case DATA_REFRESH_DOMAIN_USD_BRL:
        result = perform_https_request(NP2_BCB_USD_BRL_URL,
                                       remaining_request_budget_ms(request_start_us),
                                       HTTP_METHOD_GET, OFFLINE_EXCHANGE_MAX_BODY_BYTES, body,
                                       NP2_PROVIDER_MAX_BODY_BYTES, &body_size);
        if (result == ESP_OK) {
            result = provider_result_to_esp_err(offline_bcb_parse_usd_brl(
                body, body_size, (uint32_t)now, &s_product_snapshot.exchange));
        }
        if (result == ESP_OK) s_product_snapshot.exchange.stale = false;
        else retain_exchange_as_stale(&s_product_snapshot);
        break;
    case DATA_REFRESH_DOMAIN_FEAR_GREED:
        result = perform_https_request(NP2_ALTERNATIVE_ME_FEAR_GREED_URL,
                                       remaining_request_budget_ms(request_start_us),
                                       HTTP_METHOD_GET, OFFLINE_EXCHANGE_MAX_BODY_BYTES, body,
                                       NP2_PROVIDER_MAX_BODY_BYTES, &body_size);
        if (result == ESP_OK) result = provider_result_to_esp_err(
            offline_alternative_me_parse_fear_greed(body, body_size, (uint32_t)now,
                                                     &s_product_snapshot.fear_greed));
        if (result == ESP_OK) s_product_snapshot.fear_greed.stale = false;
        else retain_fear_greed_as_stale(&s_product_snapshot);
        break;
    case DATA_REFRESH_DOMAIN_MARKET_INDICES: {
        if (s_product_snapshot.ibovespa.available) s_product_snapshot.ibovespa.stale = true;
        if (s_product_snapshot.sp500.available) s_product_snapshot.sp500.stale = true;
        if (s_product_snapshot.nasdaq.available) s_product_snapshot.nasdaq.stale = true;
        esp_err_t first_failure = ESP_OK;
        for (size_t index = 0U;
             index < sizeof(NP2_BRAPI_MARKET_INDEX_URLS) /
                         sizeof(NP2_BRAPI_MARKET_INDEX_URLS[0]);
             ++index) {
            const uint32_t timeout_ms = remaining_request_budget_ms(request_start_us);
            if (timeout_ms == 0U) {
                if (first_failure == ESP_OK) first_failure = ESP_ERR_TIMEOUT;
                break;
            }
            offline_ibovespa_data_t ibovespa = {0};
            offline_index_data_t sp500 = {0}, nasdaq = {0};
            result = perform_brapi_request(NP2_BRAPI_MARKET_INDEX_URLS[index], timeout_ms,
                                           OFFLINE_MARKET_MAX_BODY_BYTES, body,
                                           NP2_PROVIDER_MAX_BODY_BYTES, &body_size);
            if (result == ESP_OK) result = provider_result_to_esp_err(
                offline_brapi_parse_market_indices(body, body_size, (uint32_t)now,
                                                   &ibovespa, &sp500, &nasdaq));
            if (result == ESP_OK && index == 0U && ibovespa.available) {
                s_product_snapshot.ibovespa = ibovespa;
            } else if (result == ESP_OK && index == 1U && sp500.available) {
                s_product_snapshot.sp500 = sp500;
            } else if (result == ESP_OK && index == 2U && nasdaq.available) {
                s_product_snapshot.nasdaq = nasdaq;
            } else if (result == ESP_OK) {
                result = ESP_ERR_NOT_FOUND;
            }
            if (result != ESP_OK) {
                if (first_failure == ESP_OK) first_failure = result;
            }
        }
        result = first_failure;
        break;
    }
    case DATA_REFRESH_DOMAIN_COUNT:
    default:
        return ESP_ERR_INVALID_ARG;
    }
    publish_product_snapshot();
    return result;
}

static esp_err_t refresh_all_product_domains(int64_t request_start_us)
{
    esp_err_t first_failure = ESP_OK;
    const data_refresh_domain_t domains[] = {
        DATA_REFRESH_DOMAIN_BITCOIN,
        DATA_REFRESH_DOMAIN_WEATHER,
        DATA_REFRESH_DOMAIN_USD_BRL,
        DATA_REFRESH_DOMAIN_FEAR_GREED,
        DATA_REFRESH_DOMAIN_MARKET_INDICES,
    };
    for (size_t index = 0U; index < sizeof(domains) / sizeof(domains[0]); ++index) {
        const esp_err_t result = refresh_product_domain(domains[index], request_start_us);
        if (result != ESP_OK && first_failure == ESP_OK) first_failure = result;
        if (remaining_request_budget_ms(request_start_us) == 0U) break;
    }
    return first_failure;
}

static esp_err_t preflight_p4_update(int64_t request_start_us)
{
    uint8_t manifest[104] = {0};
    uint8_t signature[388] = {0};
    size_t manifest_bytes = 0U;
    size_t signature_bytes = 0U;
    esp_err_t result = perform_https_request(s_update_manifest_url,
                                              remaining_request_budget_ms(request_start_us),
                                              HTTP_METHOD_GET, sizeof(manifest), manifest,
                                              sizeof(manifest), &manifest_bytes);
    if (result != ESP_OK || manifest_bytes != sizeof(manifest)) {
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }
    result = perform_https_request(s_update_signature_url,
                                   remaining_request_budget_ms(request_start_us),
                                   HTTP_METHOD_GET, sizeof(signature), signature,
                                   sizeof(signature), &signature_bytes);
    if (result != ESP_OK || signature_bytes != sizeof(signature)) {
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }
    /* Image URL is validated at request time but is not fetched until a keyring,
     * metadata environment and journal transaction are supplied to this worker. */
    return ESP_OK;
}

static esp_err_t apply_p4_update(int64_t request_start_us)
{
    uint8_t manifest_wire[UPDATE_MANIFEST_WIRE_BYTES] = {0};
    uint8_t signature_wire[UPDATE_SIGNATURE_WIRE_BYTES] = {0};
    size_t manifest_bytes = 0U;
    size_t signature_bytes = 0U;
    esp_err_t result = perform_https_request(
        s_update_manifest_url, remaining_request_budget_ms(request_start_us), HTTP_METHOD_GET,
        sizeof(manifest_wire), manifest_wire, sizeof(manifest_wire), &manifest_bytes);
    if (result != ESP_OK || manifest_bytes != sizeof(manifest_wire)) {
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }
    result = perform_https_request(
        s_update_signature_url, remaining_request_budget_ms(request_start_us), HTTP_METHOD_GET,
        sizeof(signature_wire), signature_wire, sizeof(signature_wire), &signature_bytes);
    if (result != ESP_OK || signature_bytes != sizeof(signature_wire)) {
        return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
    }

    update_journal_record_t prior = {0};
    const esp_err_t prior_result = flash_coordinator_get_update_journal(&prior);
    if (prior_result != ESP_OK && prior_result != ESP_ERR_NOT_FOUND) {
        return prior_result;
    }
    if (prior_result == ESP_OK && prior.state != UPDATE_JOURNAL_IDLE) {
        return ESP_ERR_INVALID_STATE;
    }
    update_replay_record_t accepted = {0};
    if (prior_result == ESP_OK && !update_journal_to_replay_record(&prior, &accepted)) {
        return ESP_ERR_INVALID_STATE;
    }

    flash_coordinator_status_t storage = {0};
    flash_coordinator_get_status(&storage);
    update_environment_t environment = s_update_environment;
    /* Re-read device facts on the sole network worker, not from caller data.
     * RPC v2/SW_AGGR are the fixed, bench-qualified 3.0.6 transport profile;
     * this version RPC does not attest the installed C6 hash/bootloader. */
    esp_hosted_coprocessor_fwver_t c6 = {0};
    if (esp_hosted_get_coprocessor_fwversion(&c6) != ESP_OK ||
        c6.major1 != 3U || c6.minor1 != 0U || c6.patch1 != 6U) {
        return ESP_ERR_INVALID_VERSION;
    }
    environment.current_c6 = (update_link_version_t){3U, 0U, 6U, 2U, true};
    const esp_partition_t *const running = esp_ota_get_running_partition();
    const esp_partition_t *const next = esp_ota_get_next_update_partition(NULL);
    esp_ota_img_states_t running_state = ESP_OTA_IMG_UNDEFINED;
    if (running == NULL || next == NULL || next == running) return ESP_ERR_INVALID_STATE;
    environment.current_app_confirmed =
        esp_ota_get_state_partition(running, &running_state) == ESP_OK &&
        running_state == ESP_OTA_IMG_VALID;
    environment.inactive_slot_bytes = next->size;
    environment.security_version = esp_app_get_description()->secure_version;
    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);
    environment.revision = chip.revision;
    environment.transaction_idle = !storage.busy && !storage.pending &&
                                   !storage.p4_ota_active && !storage.p4_ota_finished &&
                                   (prior_result == ESP_ERR_NOT_FOUND ||
                                    prior.state == UPDATE_JOURNAL_IDLE);
    const update_keyring_t keyring = {
        .entries = s_update_keyring_entries,
        .entries_count = s_update_keyring_entries_count,
    };
    update_image_hash_session_t hash = {0};
    update_manifest_t manifest = {0};
    if (update_admission_begin(manifest_wire, manifest_bytes, signature_wire, signature_bytes,
                               &keyring, &environment, &accepted, &hash,
                               &manifest) != UPDATE_ADMISSION_OK) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    update_journal_record_t journal = {0};
    update_journal_init(&journal);
    if (!update_journal_prepare(&journal, &manifest) ||
        !update_journal_transition(&journal, UPDATE_JOURNAL_P4_STAGED)) {
        update_image_hash_abort(&hash);
        return ESP_ERR_INVALID_STATE;
    }
    result = persist_update_journal_and_wait(&journal, NULL);
    if (result != ESP_OK) {
        update_image_hash_abort(&hash);
        return result;
    }
    const update_journal_record_t staged_journal = journal;
    bool pending_journal_persisted = false;

    update_p4_writer_t writer = {0};
    if (update_p4_writer_begin(&writer, &hash) != UPDATE_P4_WRITER_OK) {
        result = ESP_FAIL;
        goto abort_staged;
    }
    result = stream_update_image(s_update_image_url, manifest.metadata.image_bytes, &writer, &hash);
    if (result != ESP_OK || update_p4_writer_finish(&writer, &hash) != UPDATE_P4_WRITER_OK) {
        result = result == ESP_OK ? ESP_FAIL : result;
        goto abort_writer;
    }
    uint32_t persisted_generation = 0U;
    if (!update_journal_transition(&journal, UPDATE_JOURNAL_P4_PENDING) ||
        persist_update_journal_and_wait(&journal, &persisted_generation) != ESP_OK) {
        result = ESP_FAIL;
        goto abort_writer;
    }
    pending_journal_persisted = true;
    /* Changing generation directly leaves the CRC stale and blocks activation. */
    if (!update_journal_set_generation(&journal, persisted_generation)) {
        result = ESP_ERR_INVALID_CRC;
        goto abort_writer;
    }
    if (update_p4_writer_request_activation(&writer, &journal) != UPDATE_P4_WRITER_OK) {
        result = ESP_FAIL;
        goto abort_writer;
    }

    ESP_LOGW(TAG, "authenticated P4 candidate selected; restarting into pending verification");
    esp_restart();

abort_writer:
    update_p4_writer_abort(&writer, &hash);
abort_staged:
    if (!pending_journal_persisted) {
        update_journal_record_t idle_journal = staged_journal;
        if (update_journal_transition(&idle_journal, UPDATE_JOURNAL_IDLE)) {
            (void)persist_update_journal_and_wait(&idle_journal, NULL);
        }
    }
    return result;
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
    data_refresh_scheduler_t scheduler = {0};
    data_refresh_scheduler_init(&scheduler);
    bool station_was_online = false;
    for (;;) {
        retry_pending_product_delivery();
        bool request_pending = false;
        bool scheduled_product_refresh = false;
        data_refresh_domain_t scheduled_domain = DATA_REFRESH_DOMAIN_COUNT;
        network_validation_mode_t mode = NETWORK_VALIDATION_MODE_NORMAL;
        taskENTER_CRITICAL(&s_status_lock);
        request_pending = s_request_pending;
        mode = s_request_mode;
        s_request_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);

        if (!request_pending) {
            connectivity_diagnostic_status_t connectivity = {0};
            connectivity_diagnostic_get_status(&connectivity);
            const int64_t now_us = esp_timer_get_time();
            if (!connectivity.online) {
                /* A new DHCP lease starts all domains as due. No offline
                 * panel attempts DNS, NTP or HTTPS. */
                if (station_was_online) data_refresh_scheduler_mark_all_due(&scheduler);
                station_was_online = false;
            } else if (data_refresh_scheduler_take_due(&scheduler, now_us, &scheduled_domain)) {
                request_pending = true;
                scheduled_product_refresh = true;
                taskENTER_CRITICAL(&s_status_lock);
                s_status.busy = true;
                taskEXIT_CRITICAL(&s_status_lock);
                ESP_LOGI(TAG, "product refresh scheduled domain=%u",
                         (unsigned int)scheduled_domain);
            }
            station_was_online = connectivity.online;
        }
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
            dns_result = validate_dns(TIME_SERVICE_NTP_HOST);
        }
        connectivity_diagnostic_report_dns_result(dns_result);
        esp_err_t ntp_result = ESP_ERR_INVALID_STATE;
        esp_err_t https_result = ESP_ERR_INVALID_STATE;
        if (dns_result == ESP_OK && mode != NETWORK_VALIDATION_MODE_DNS_NXDOMAIN &&
            mode != NETWORK_VALIDATION_MODE_DNS_TIMEOUT) {
            time_service_status_t time_status = {0};
            time_service_get_status(&time_status);
            const time_t now = time(NULL);
            const bool sync_due = !time_status.trusted || time_status.last_sync_unix_s == 0U ||
                                  now < (time_t)NP2_VALID_EPOCH_SECONDS ||
                                  (uint64_t)now - time_status.last_sync_unix_s >=
                                      NP2_TIME_SYNC_INTERVAL_S;
            ntp_result = sync_due ? time_service_sync()
                                  : (time_status.trusted ? ESP_OK : ESP_ERR_INVALID_STATE);
        }
        const uint32_t https_budget_ms = remaining_request_budget_ms(start_us);
        if (ntp_result == ESP_OK && https_budget_ms > 0U) {
            const bool slow_https = mode == NETWORK_VALIDATION_MODE_HTTPS_TIMEOUT;
            const bool oversize_https = mode == NETWORK_VALIDATION_MODE_HTTPS_OVERSIZE;
            if (scheduled_product_refresh) {
                https_result = refresh_product_domain(scheduled_domain, start_us);
            } else if (mode == NETWORK_VALIDATION_MODE_OFFLINE_DATA_REFRESH) {
                https_result = refresh_all_product_domains(start_us);
            } else if (mode == NETWORK_VALIDATION_MODE_P4_UPDATE_PREFLIGHT) {
                https_result = preflight_p4_update(start_us);
            } else if (mode == NETWORK_VALIDATION_MODE_P4_UPDATE_APPLY) {
                https_result = apply_p4_update(start_us);
            } else if (mode == NETWORK_VALIDATION_MODE_EWELINK_SYNC) {
                https_result = np_ewelink_service_process_sync();
                ESP_LOGI(TAG, "eWeLink sync task stack minimum free=%u bytes",
                         (unsigned int)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
            } else {
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
                                              slow_https || oversize_https
                                                  ? NP2_HTTPS_MAX_BODY_BYTES
                                                  : 0U);
            }
        } else if (ntp_result == ESP_OK) {
            https_result = ESP_ERR_TIMEOUT;
        }

        const uint32_t duration_ms = elapsed_ms_since(start_us);
        if (mode != NETWORK_VALIDATION_MODE_P4_UPDATE_APPLY &&
            mode != NETWORK_VALIDATION_MODE_EWELINK_SYNC &&
            duration_ms > NP2_REQUEST_TOTAL_TIMEOUT_MS) {
            https_result = ESP_ERR_TIMEOUT;
        }
        publish_results(dns_result, ntp_result, https_result, duration_ms);
        if (scheduled_product_refresh) {
            data_refresh_scheduler_note_result(&scheduler, scheduled_domain,
                                               esp_timer_get_time(), https_result == ESP_OK);
        } else if (mode == NETWORK_VALIDATION_MODE_OFFLINE_DATA_REFRESH) {
            const bool success = dns_result == ESP_OK && ntp_result == ESP_OK &&
                                 https_result == ESP_OK;
            for (size_t index = 0U; index < DATA_REFRESH_DOMAIN_COUNT; ++index) {
                data_refresh_scheduler_note_result(&scheduler, (data_refresh_domain_t)index,
                                                   esp_timer_get_time(), success);
            }
        }
        ESP_LOGI(TAG, "network work complete mode=%u domain=%u dns=%s ntp=%s https=%s duration=%lums",
                 (unsigned int)mode,
                 (unsigned int)scheduled_domain,
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
    if (!s_started || !connectivity.online ||
        mode > NETWORK_VALIDATION_MODE_OFFLINE_DATA_REFRESH) {
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

esp_err_t network_validation_service_request_offline_data_refresh(void)
{
    return network_validation_service_request_check(NETWORK_VALIDATION_MODE_OFFLINE_DATA_REFRESH);
}

esp_err_t network_validation_service_request_ewelink_sync(void)
{
    connectivity_diagnostic_status_t connectivity = {0};
    connectivity_diagnostic_get_status(&connectivity);
    if (!s_started || !connectivity.online) return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.busy || s_request_pending) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_request_pending = true;
    s_request_mode = NETWORK_VALIDATION_MODE_EWELINK_SYNC;
    s_status.busy = true;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t network_validation_service_request_p4_update_preflight(
    const update_https_endpoints_t *endpoints)
{
    connectivity_diagnostic_status_t connectivity = {0};
    connectivity_diagnostic_get_status(&connectivity);
    if (!s_started || !connectivity.online) return ESP_ERR_INVALID_STATE;
    if (update_https_endpoints_validate(endpoints) != UPDATE_HTTPS_POLICY_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t manifest_length = strlen(endpoints->manifest_url);
    const size_t signature_length = strlen(endpoints->signature_url);
    const size_t image_length = strlen(endpoints->image_url);
    if (manifest_length >= sizeof(s_update_manifest_url) ||
        signature_length >= sizeof(s_update_signature_url) ||
        image_length >= sizeof(s_update_image_url)) return ESP_ERR_INVALID_SIZE;
    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.busy || s_request_pending) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(s_update_manifest_url, endpoints->manifest_url, manifest_length + 1U);
    memcpy(s_update_signature_url, endpoints->signature_url, signature_length + 1U);
    memcpy(s_update_image_url, endpoints->image_url, image_length + 1U);
    s_request_pending = true;
    s_request_mode = NETWORK_VALIDATION_MODE_P4_UPDATE_PREFLIGHT;
    s_status.busy = true;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t network_validation_service_request_p4_update_apply(
    const network_p4_update_request_t *request)
{
    connectivity_diagnostic_status_t connectivity = {0};
    connectivity_diagnostic_get_status(&connectivity);
    if (!s_started || !connectivity.online) return ESP_ERR_INVALID_STATE;
    if (request == NULL ||
        update_https_endpoints_validate(&request->endpoints) != UPDATE_HTTPS_POLICY_OK ||
        request->keyring_entries == NULL || request->keyring_entries_count == 0U ||
        request->keyring_entries_count > NETWORK_P4_UPDATE_MAX_TRUSTED_KEYS) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t manifest_length = strlen(request->endpoints.manifest_url);
    const size_t signature_length = strlen(request->endpoints.signature_url);
    const size_t image_length = strlen(request->endpoints.image_url);
    if (manifest_length >= sizeof(s_update_manifest_url) ||
        signature_length >= sizeof(s_update_signature_url) ||
        image_length >= sizeof(s_update_image_url)) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t index = 0U; index < request->keyring_entries_count; ++index) {
        const update_trusted_key_t *const key = &request->keyring_entries[index].trusted_key;
        if (key->key_id == 0U || key->public_key_der == NULL ||
            key->public_key_der_bytes == 0U ||
            key->public_key_der_bytes > UPDATE_SIGNATURE_PUBLIC_KEY_MAX_BYTES) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_started || s_status.busy || s_request_pending) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(s_update_manifest_url, request->endpoints.manifest_url, manifest_length + 1U);
    memcpy(s_update_signature_url, request->endpoints.signature_url, signature_length + 1U);
    memcpy(s_update_image_url, request->endpoints.image_url, image_length + 1U);
    memcpy(s_update_keyring_entries, request->keyring_entries,
           request->keyring_entries_count * sizeof(s_update_keyring_entries[0]));
    s_update_keyring_entries_count = request->keyring_entries_count;
    s_update_environment = request->environment;
    s_request_pending = true;
    s_request_mode = NETWORK_VALIDATION_MODE_P4_UPDATE_APPLY;
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
