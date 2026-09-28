/*
 * Phase 3 Hosted/Wi-Fi owner for the ESP32-P4/C6 pair.
 *
 * Hosted setup can wait tens of seconds when the C6 is absent, so all work
 * stays out of app_main and LVGL. Station settings use WIFI_STORAGE_RAM.
 * The one-entry private mailbox copies a request, never logs it, and is wiped
 * after the worker consumes it. A separately opted-in development build may
 * restore a disposable lab credential from FlashCoordinator after startup.
 */
#include "connectivity_diagnostic.h"

#include <string.h>

#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "flash_coordinator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *const TAG = "np2_connect";

#define NP2_CONNECTIVITY_TASK_STACK_BYTES (8U * 1024U)
#define NP2_CONNECTIVITY_TASK_PRIORITY 3U
#define NP2_CONNECTIVITY_POLL_MS 250U
#define NP2_WIFI_SSID_MAX_BYTES 32U
#define NP2_WIFI_SCAN_RESULTS_MAX CONNECTIVITY_DIAGNOSTIC_MAX_SCAN_RESULTS
#define NP2_WIFI_PASSWORD_MAX_BYTES 63U
#define NP2_WIFI_ASSOCIATION_TIMEOUT_MS 15000U
#define NP2_WIFI_DHCP_TIMEOUT_MS 20000U
#define NP2_WIFI_STA_INACTIVE_TIME_SECONDS 6U
#define NP2_DNS_REASSOCIATION_COOLDOWN_MS 60000U
#define NP2_HOSTED_RECOVERY_WINDOW_MS (10U * 60U * 1000U)
#define NP2_HOSTED_RECOVERY_COOLDOWN_MS (5U * 60U * 1000U)
#define NP2_HOSTED_RECOVERY_MAX_CYCLES 3U
#define NP2_RECOVERY_CAMPAIGN_IP_TIMEOUT_MS 20000U
#define NP2_HOSTED_STARTUP_TIMEOUT_MS 30000U
#define NP2_HOSTED_STARTUP_TASK_STACK_BYTES (6U * 1024U)
#define NP2_WIFI_CREDENTIAL_PERSIST_STABLE_MS 30000U

typedef enum {
    CONNECTIVITY_REQUEST_JOIN = 0,
    CONNECTIVITY_REQUEST_FORGET,
    CONNECTIVITY_REQUEST_SCAN,
    CONNECTIVITY_REQUEST_RECOVER_HOSTED,
    CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_CAMPAIGN,
    CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_FULL_CAMPAIGN,
    CONNECTIVITY_REQUEST_DHCP_SILENCE_INJECTION,
} connectivity_request_type_t;

typedef struct {
    connectivity_request_type_t type;
    char ssid[NP2_WIFI_SSID_MAX_BYTES + 1U];
    char password[NP2_WIFI_PASSWORD_MAX_BYTES + 1U];
} connectivity_request_t;

typedef struct {
    SemaphoreHandle_t done;
    esp_err_t result;
} hosted_startup_context_t;

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_request_lock = portMUX_INITIALIZER_UNLOCKED;
static connectivity_diagnostic_status_t s_status = {
    .state = CONNECTIVITY_DIAGNOSTIC_STATE_IDLE,
    .last_result = ESP_OK,
};
static connectivity_request_t s_request_mailbox;
static connectivity_request_t s_active_station_request;
static bool s_request_pending;
static bool s_active_station_request_valid;
static bool s_started;
static bool s_wifi_events_registered;
static bool s_hosted_recovery_in_progress;
static esp_netif_t *s_station_netif;
/*
 * esp_wifi_disconnect() is asynchronous.  When replacing a station
 * configuration or forgetting it, the driver can publish a disconnect event
 * after the request has already been queued. Consume one self-initiated
 * event; every other disconnect remains observable and retryable.
 */
static bool s_intentional_station_disconnect_pending;
static bool s_retry_pending;
static bool s_dns_reassociation_pending;
static bool s_dhcp_silence_active;
static bool s_dhcp_silence_timeout_observed;
static int64_t s_station_deadline_us;
static int64_t s_next_dns_reassociation_us;
static int64_t s_recovery_attempts_us[NP2_HOSTED_RECOVERY_MAX_CYCLES];
static int64_t s_recovery_cooldown_until_us;
static int64_t s_online_since_us;
static uint32_t s_credential_vault_expected_generation;
static bool s_credential_vault_write_in_flight;
static bool s_credential_vault_write_attempted;

static esp_err_t recover_hosted_link(void);
static void set_status(connectivity_diagnostic_state_t state, esp_err_t result);

static void hosted_startup_task(void *arg)
{
    hosted_startup_context_t *const context = arg;
    esp_err_t result = (esp_err_t)esp_hosted_init();
    if (result == ESP_OK) {
        result = (esp_err_t)esp_hosted_connect_to_slave();
    }
    context->result = result;
    (void)xSemaphoreGive(context->done);
    vTaskDelete(NULL);
}

static esp_err_t start_hosted_with_supervisor(void)
{
    hosted_startup_context_t context = {
        .done = xSemaphoreCreateBinary(),
        .result = ESP_ERR_INVALID_STATE,
    };
    if (context.done == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const BaseType_t created = xTaskCreate(hosted_startup_task, "np2_hosted_start",
                                           NP2_HOSTED_STARTUP_TASK_STACK_BYTES, &context,
                                           NP2_CONNECTIVITY_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        vSemaphoreDelete(context.done);
        return ESP_ERR_NO_MEM;
    }

    const int64_t deadline_us = esp_timer_get_time() +
                                (int64_t)NP2_HOSTED_STARTUP_TIMEOUT_MS * 1000LL;
    bool deadline_reported = false;
    for (;;) {
        if (xSemaphoreTake(context.done, pdMS_TO_TICKS(NP2_CONNECTIVITY_POLL_MS)) == pdTRUE) {
            vSemaphoreDelete(context.done);
            return context.result;
        }
        if (!deadline_reported && esp_timer_get_time() >= deadline_us) {
            deadline_reported = true;
            set_status(CONNECTIVITY_DIAGNOSTIC_STATE_LINK_DOWN, ESP_ERR_TIMEOUT);
            ESP_LOGW(TAG, "Hosted startup exceeded 30s; local UI remains offline-ready");
        }
    }
}

static bool recovery_campaign_station_is_online(void)
{
    bool online;
    taskENTER_CRITICAL(&s_status_lock);
    online = s_status.online;
    taskEXIT_CRITICAL(&s_status_lock);
    return online;
}

static int64_t recovery_cooldown_until_us(void)
{
    int64_t until_us;
    taskENTER_CRITICAL(&s_status_lock);
    until_us = s_recovery_cooldown_until_us;
    taskEXIT_CRITICAL(&s_status_lock);
    return until_us;
}

static bool hosted_link_is_up(void)
{
    bool link_up;
    taskENTER_CRITICAL(&s_status_lock);
    link_up = s_status.link_up;
    taskEXIT_CRITICAL(&s_status_lock);
    return link_up;
}

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) {
        *bytes++ = 0;
    }
}

static size_t bounded_length(const char *value, size_t limit)
{
    size_t length = 0;
    while (length < limit && value[length] != '\0') {
        ++length;
    }
    return length;
}

static void set_status(connectivity_diagnostic_state_t state, esp_err_t result)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status.state = state;
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void fail_probe(const char *step, esp_err_t err)
{
    ESP_LOGE(TAG, "%s failed: %s", step, esp_err_to_name(err));
    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_FAILED, err);
}

static bool credentials_are_active(void)
{
    bool active;
    taskENTER_CRITICAL(&s_status_lock);
    active = s_status.station_credentials_in_ram;
    taskEXIT_CRITICAL(&s_status_lock);
    return active;
}

static void clear_active_credentials(void)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_retry_pending = false;
    s_station_deadline_us = 0;
    secure_zero(&s_active_station_request, sizeof(s_active_station_request));
    s_active_station_request_valid = false;
    s_status.reconnect_attempts = 0;
    s_status.online = false;
    memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
    s_status.station_credentials_in_ram = false;
    s_status.credential_vault_saved = false;
    s_status.credential_vault_save_pending = false;
    s_online_since_us = 0;
    s_credential_vault_expected_generation = 0;
    s_credential_vault_write_in_flight = false;
    s_credential_vault_write_attempted = false;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void set_station_deadline(connectivity_diagnostic_state_t state,
                                 esp_err_t result, uint32_t timeout_ms)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_status.state = state;
    s_status.last_result = result;
    s_station_deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000LL;
    taskEXIT_CRITICAL(&s_status_lock);
}

static void request_station_retry(esp_err_t result)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_station_deadline_us = 0;
    s_status.online = false;
    memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
    s_status.last_result = result;
    if (s_status.station_credentials_in_ram) {
        s_retry_pending = true;
        s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF;
    }
    taskEXIT_CRITICAL(&s_status_lock);
}

static bool station_deadline_expired(void)
{
    bool expired = false;
    taskENTER_CRITICAL(&s_status_lock);
    if (s_station_deadline_us != 0 && esp_timer_get_time() >= s_station_deadline_us) {
        s_station_deadline_us = 0;
        expired = true;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    return expired;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        const wifi_event_sta_connected_t *const connected = event_data;
        taskENTER_CRITICAL(&s_status_lock);
        memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
        if (connected != NULL) {
            const size_t length = connected->ssid_len < sizeof(s_status.connected_ssid)
                                      ? connected->ssid_len : sizeof(s_status.connected_ssid) - 1U;
            memcpy(s_status.connected_ssid, connected->ssid, length);
        }
        taskEXIT_CRITICAL(&s_status_lock);
        set_station_deadline(CONNECTIVITY_DIAGNOSTIC_STATE_WAITING_FOR_IP, ESP_OK,
                             NP2_WIFI_DHCP_TIMEOUT_MS);
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        bool dhcp_silence_recovered = false;
        taskENTER_CRITICAL(&s_status_lock);
        s_retry_pending = false;
        s_station_deadline_us = 0;
        s_status.reconnect_attempts = 0;
        s_status.online = true;
        s_online_since_us = esp_timer_get_time();
        s_status.last_result = ESP_OK;
        s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_ONLINE;
        dhcp_silence_recovered = s_dhcp_silence_timeout_observed;
        s_dhcp_silence_timeout_observed = false;
        s_dhcp_silence_active = false;
        taskEXIT_CRITICAL(&s_status_lock);
        ESP_LOGI(TAG, "station received an IP address");
        if (dhcp_silence_recovered) {
            ESP_LOGI(TAG, "DHCP silence injection passed: deadline then automatic recovery");
        }
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *const disconnected = event_data;
        const uint8_t reason = disconnected == NULL ? 0U : disconnected->reason;
        bool intentional_disconnect = false;
        bool terminal_credentials_rejected = false;
        taskENTER_CRITICAL(&s_status_lock);
        intentional_disconnect = s_intentional_station_disconnect_pending;
        if (intentional_disconnect) {
            s_intentional_station_disconnect_pending = false;
        }
        if (intentional_disconnect) {
            s_station_deadline_us = 0;
            s_status.online = false;
            memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
            s_status.last_result = ESP_OK;
            s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF;
            s_retry_pending = s_status.station_credentials_in_ram;
            taskEXIT_CRITICAL(&s_status_lock);
            ESP_LOGI(TAG, "station configuration replaced; retrying after disconnect completes");
            return;
        }
        s_status.online = false;
        memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
        s_station_deadline_us = 0;
        s_status.last_disconnect_reason = reason;
        /* AUTH_EXPIRE and handshake timeouts occur after an AP restart or a
         * brief RF loss. They are retriable and must never discard the saved
         * network. Only definitive credential rejection pauses retries; the
         * durable record remains available until an explicit FORGET. */
        terminal_credentials_rejected = reason == WIFI_REASON_AUTH_FAIL ||
                                       reason == WIFI_REASON_802_1X_AUTH_FAILED;
        if (terminal_credentials_rejected) {
            s_retry_pending = false;
            s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_FAILED;
            s_status.last_result = ESP_ERR_WIFI_PASSWORD;
        } else if (s_status.station_credentials_in_ram) {
            s_retry_pending = true;
            s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF;
        }
        taskEXIT_CRITICAL(&s_status_lock);
        if (terminal_credentials_rejected) {
            ESP_LOGW(TAG, "station authentication rejected (reason=%u); durable credential retained",
                     (unsigned int)reason);
            return;
        }
        ESP_LOGW(TAG, "station disconnected; retry is handled by connectivity worker");
    }
}

/*
 * The Hosted transport itself owns reset54.  This callback only records a
 * fault and prevents the Wi-Fi worker from issuing RPCs on a dead SDIO link.
 * Recovery stays a separately gated operation until its full lifecycle has
 * physical evidence; neither this callback nor the UI can restart the P4.
 */
static void hosted_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base != EH_HOST_EVENT) {
        return;
    }

    taskENTER_CRITICAL(&s_status_lock);
    if (event_id == EH_HOST_EVENT_TRANSPORT_UP) {
        s_status.link_up = true;
        if (!s_status.wifi_ready) {
            s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_LINK_UP;
        }
        s_status.last_result = ESP_OK;
        taskEXIT_CRITICAL(&s_status_lock);
        ESP_LOGI(TAG, "Hosted SDIO transport is up");
        return;
    }

    if (event_id == EH_HOST_EVENT_TRANSPORT_FAILURE ||
        event_id == EH_HOST_EVENT_TRANSPORT_DOWN) {
        s_status.link_up = false;
        s_status.wifi_ready = false;
        s_status.online = false;
        memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
        s_station_deadline_us = 0;
        s_retry_pending = false;
        s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_LINK_DOWN;
        s_status.last_result = ESP_ERR_INVALID_STATE;
        if (!s_hosted_recovery_in_progress && s_status.transport_failures < UINT32_MAX) {
            ++s_status.transport_failures;
        }
        taskEXIT_CRITICAL(&s_status_lock);
        ESP_LOGW(TAG, "Hosted SDIO transport is down; local UI remains offline-ready");
        return;
    }
    taskEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t configure_station_from_request(const connectivity_request_t *request)
{
    wifi_config_t config = {0};
    bool replace_active_station_config = false;
    const size_t ssid_length = bounded_length(request->ssid, sizeof(request->ssid));
    const size_t password_length = bounded_length(request->password, sizeof(request->password));

    if (ssid_length == 0U || ssid_length >= sizeof(request->ssid) ||
        password_length >= sizeof(request->password) ||
        (password_length > 0U && password_length < 8U)) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(config.sta.ssid, request->ssid, ssid_length);
    memcpy(config.sta.password, request->password, password_length);
    config.sta.threshold.authmode =
        password_length == 0U ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    taskENTER_CRITICAL(&s_status_lock);
    replace_active_station_config = s_active_station_request_valid;
    s_retry_pending = false;
    s_intentional_station_disconnect_pending = replace_active_station_config;
    s_status.reconnect_attempts = 0;
    s_status.online = false;
    memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
    s_status.station_credentials_in_ram = true;
    s_status.credential_vault_saved = false;
    s_status.credential_vault_save_pending = false;
    s_online_since_us = 0;
    s_credential_vault_expected_generation = 0;
    s_credential_vault_write_in_flight = false;
    s_credential_vault_write_attempted = false;
    taskEXIT_CRITICAL(&s_status_lock);

    esp_err_t err = ESP_OK;
    if (replace_active_station_config) {
        err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
            taskENTER_CRITICAL(&s_status_lock);
            s_intentional_station_disconnect_pending = false;
            taskEXIT_CRITICAL(&s_status_lock);
            secure_zero(&config, sizeof(config));
            clear_active_credentials();
            return err;
        }
        if (err == ESP_ERR_WIFI_NOT_CONNECT) {
            taskENTER_CRITICAL(&s_status_lock);
            s_intentional_station_disconnect_pending = false;
            taskEXIT_CRITICAL(&s_status_lock);
        }
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &config);
    secure_zero(&config, sizeof(config));
    if (err != ESP_OK) {
        taskENTER_CRITICAL(&s_status_lock);
        s_intentional_station_disconnect_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);
        clear_active_credentials();
        return err;
    }

    taskENTER_CRITICAL(&s_status_lock);
    secure_zero(&s_active_station_request, sizeof(s_active_station_request));
    memcpy(&s_active_station_request, request, sizeof(s_active_station_request));
    s_active_station_request_valid = true;
    taskEXIT_CRITICAL(&s_status_lock);

    set_station_deadline(CONNECTIVITY_DIAGNOSTIC_STATE_ASSOCIATING, ESP_OK,
                         NP2_WIFI_ASSOCIATION_TIMEOUT_MS);
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        request_station_retry(err);
    }
    return err;
}

static esp_err_t forget_station(void)
{
    wifi_config_t empty_config = {0};

    clear_active_credentials();

    taskENTER_CRITICAL(&s_status_lock);
    s_intentional_station_disconnect_pending = true;
    taskEXIT_CRITICAL(&s_status_lock);

    /* FORGET is the sole path that removes the durable credential. */
    const esp_err_t credential_clear_result =
        flash_coordinator_request_credential_vault_clear();
    if (credential_clear_result != ESP_OK && credential_clear_result != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "credential vault clear request failed: %s",
                 esp_err_to_name(credential_clear_result));
    }

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
        taskENTER_CRITICAL(&s_status_lock);
        s_intentional_station_disconnect_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);
        secure_zero(&empty_config, sizeof(empty_config));
        return err;
    }
    if (err == ESP_ERR_WIFI_NOT_CONNECT) {
        taskENTER_CRITICAL(&s_status_lock);
        s_intentional_station_disconnect_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &empty_config);
    secure_zero(&empty_config, sizeof(empty_config));
    if (err != ESP_OK) {
        taskENTER_CRITICAL(&s_status_lock);
        s_intentional_station_disconnect_pending = false;
        taskEXIT_CRITICAL(&s_status_lock);
    }
    if (err == ESP_OK) {
        set_status(CONNECTIVITY_DIAGNOSTIC_STATE_WIFI_READY, ESP_OK);
    }
    return err;
}

static esp_err_t wait_for_recovery_campaign_ip(void)
{
    const int64_t deadline_us = esp_timer_get_time() +
                                (int64_t)NP2_RECOVERY_CAMPAIGN_IP_TIMEOUT_MS * 1000LL;
    while (!recovery_campaign_station_is_online() && esp_timer_get_time() < deadline_us) {
        vTaskDelay(pdMS_TO_TICKS(NP2_CONNECTIVITY_POLL_MS));
    }
    return recovery_campaign_station_is_online() ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t run_hosted_cooldown_campaign(bool verify_cooldown_expiry)
{
    bool has_active_station = false;
    taskENTER_CRITICAL(&s_status_lock);
    has_active_station = s_active_station_request_valid && s_status.online;
    taskEXIT_CRITICAL(&s_status_lock);
    if (!has_active_station) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_OK;
    for (uint32_t cycle = 1U; cycle <= NP2_HOSTED_RECOVERY_MAX_CYCLES; ++cycle) {
        result = recover_hosted_link();
        if (result != ESP_OK || (result = wait_for_recovery_campaign_ip()) != ESP_OK) {
            return result;
        }
        ESP_LOGI(TAG, "Hosted cooldown campaign cycle %lu recovered IP",
                 (unsigned long)cycle);
    }

    const esp_err_t fourth_result = recover_hosted_link();
    if (fourth_result != ESP_ERR_TIMEOUT) {
        return fourth_result == ESP_OK ? ESP_FAIL : fourth_result;
    }
    if (!verify_cooldown_expiry) {
        ESP_LOGI(TAG, "Hosted cooldown campaign passed: 3 cycles, fourth rejected");
        return ESP_OK;
    }

    const int64_t cooldown_until_us = recovery_cooldown_until_us();
    ESP_LOGI(TAG, "Hosted full cooldown campaign waiting for expiry");
    while (esp_timer_get_time() < cooldown_until_us) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    result = recover_hosted_link();
    if (result != ESP_OK || (result = wait_for_recovery_campaign_ip()) != ESP_OK) {
        return result;
    }
    ESP_LOGI(TAG, "Hosted full cooldown campaign passed: fifth recovery allowed after cooldown");
    return ESP_OK;
}

static void process_request(connectivity_request_t *request)
{
    esp_err_t err;
    if (request->type == CONNECTIVITY_REQUEST_JOIN) {
        err = configure_station_from_request(request);
        if (err != ESP_OK) {
            if (credentials_are_active()) {
                ESP_LOGW(TAG, "station association request will retry: %s",
                         esp_err_to_name(err));
            } else {
                fail_probe("station association request", err);
            }
        } else {
            ESP_LOGI(TAG, "station association requested from private mailbox");
        }
    } else if (request->type == CONNECTIVITY_REQUEST_FORGET) {
        err = forget_station();
        if (err != ESP_OK) {
            fail_probe("station forget request", err);
        } else {
            ESP_LOGI(TAG, "station configuration and durable credential cleared");
        }
    } else if (request->type == CONNECTIVITY_REQUEST_SCAN) {
        const wifi_scan_config_t scan_cfg = {.show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE};
        set_status(CONNECTIVITY_DIAGNOSTIC_STATE_SCANNING, ESP_OK);
        err = esp_wifi_scan_start(&scan_cfg, true);
        if (err == ESP_OK) {
            uint16_t found = 0;
            err = esp_wifi_scan_get_ap_num(&found);
            static wifi_ap_record_t records[NP2_WIFI_SCAN_RESULTS_MAX];
            uint16_t count = found < NP2_WIFI_SCAN_RESULTS_MAX ? found : NP2_WIFI_SCAN_RESULTS_MAX;
            if (err == ESP_OK && count > 0U) err = esp_wifi_scan_get_ap_records(&count, records);
            if (err == ESP_OK) {
                taskENTER_CRITICAL(&s_status_lock);
                s_status.access_points_found = found;
                s_status.scan_results_count = (uint8_t)count;
                memset(s_status.scan_results, 0, sizeof(s_status.scan_results));
                for (uint16_t i = 0; i < count; ++i) {
                    memcpy(s_status.scan_results[i].ssid, records[i].ssid,
                           sizeof(s_status.scan_results[i].ssid) - 1U);
                    s_status.scan_results[i].rssi = records[i].rssi;
                    s_status.scan_results[i].secure = records[i].authmode != WIFI_AUTH_OPEN;
                }
                taskEXIT_CRITICAL(&s_status_lock);
                set_status(CONNECTIVITY_DIAGNOSTIC_STATE_SCAN_COMPLETE, ESP_OK);
            }
        }
        if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi scan request failed: %s", esp_err_to_name(err));
    } else if (request->type == CONNECTIVITY_REQUEST_RECOVER_HOSTED) {
        err = recover_hosted_link();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Hosted recovery request ended: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Hosted recovery lifecycle completed");
        }
    } else if (request->type == CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_CAMPAIGN ||
               request->type == CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_FULL_CAMPAIGN) {
        err = run_hosted_cooldown_campaign(
            request->type == CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_FULL_CAMPAIGN);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Hosted cooldown campaign ended: %s", esp_err_to_name(err));
        }
    } else {
        connectivity_request_t active_request = {0};
        bool can_inject = false;
        taskENTER_CRITICAL(&s_status_lock);
        can_inject = s_active_station_request_valid && s_status.online &&
                     !s_dhcp_silence_active;
        if (can_inject) {
            memcpy(&active_request, &s_active_station_request, sizeof(active_request));
            s_dhcp_silence_active = true;
            s_dhcp_silence_timeout_observed = false;
        }
        taskEXIT_CRITICAL(&s_status_lock);
        if (!can_inject || s_station_netif == NULL) {
            err = ESP_ERR_INVALID_STATE;
        } else {
            err = esp_netif_dhcpc_stop(s_station_netif);
            if (err == ESP_OK) {
                err = configure_station_from_request(&active_request);
            }
            if (err != ESP_OK) {
                (void)esp_netif_dhcpc_start(s_station_netif);
                taskENTER_CRITICAL(&s_status_lock);
                s_dhcp_silence_active = false;
                s_dhcp_silence_timeout_observed = false;
                taskEXIT_CRITICAL(&s_status_lock);
                ESP_LOGW(TAG, "DHCP silence injection setup failed: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "DHCP silence injection started; waiting for 20s deadline");
            }
        }
        secure_zero(&active_request, sizeof(active_request));
    }
    secure_zero(request, sizeof(*request));
}

static uint32_t next_backoff_delay_ms(void)
{
    static const uint32_t retry_seconds[] = {2U, 4U, 8U, 16U, 30U};
    uint32_t attempt;
    taskENTER_CRITICAL(&s_status_lock);
    attempt = s_status.reconnect_attempts;
    if (s_status.reconnect_attempts < UINT32_MAX) {
        ++s_status.reconnect_attempts;
    }
    taskEXIT_CRITICAL(&s_status_lock);

    const size_t index = attempt < (sizeof(retry_seconds) / sizeof(retry_seconds[0]))
                             ? (size_t)attempt
                             : (sizeof(retry_seconds) / sizeof(retry_seconds[0])) - 1U;
    return retry_seconds[index] * 1000U + (esp_random() % 500U);
}

static void maybe_persist_credential_vault(void)
{
    if (!flash_coordinator_credential_vault_ready()) return;

    connectivity_request_t credentials = {0};
    bool eligible = false;
    bool write_in_flight = false;
    bool write_attempted = false;
    uint32_t expected_generation = 0U;
    const int64_t now_us = esp_timer_get_time();
    taskENTER_CRITICAL(&s_status_lock);
    eligible = s_status.online && s_status.station_credentials_in_ram &&
               s_online_since_us != 0 &&
               now_us - s_online_since_us >=
                   (int64_t)NP2_WIFI_CREDENTIAL_PERSIST_STABLE_MS * 1000LL;
    write_in_flight = s_credential_vault_write_in_flight;
    write_attempted = s_credential_vault_write_attempted;
    expected_generation = s_credential_vault_expected_generation;
    if (eligible && !write_in_flight && !write_attempted) {
        memcpy(&credentials, &s_active_station_request, sizeof(credentials));
    }
    taskEXIT_CRITICAL(&s_status_lock);

    if (!eligible) {
        secure_zero(&credentials, sizeof(credentials));
        return;
    }

    flash_coordinator_status_t storage = {0};
    flash_coordinator_get_status(&storage);
    if (write_in_flight) {
        if (storage.pending || storage.busy) return;
        const bool saved = storage.credential_vault_valid &&
                           storage.credential_vault_generation == expected_generation;
        taskENTER_CRITICAL(&s_status_lock);
        s_credential_vault_write_in_flight = false;
        s_status.credential_vault_save_pending = false;
        s_status.credential_vault_saved = saved;
        s_status.last_result = saved ? ESP_OK : storage.credential_vault_result;
        taskEXIT_CRITICAL(&s_status_lock);
        if (saved) {
            ESP_LOGI(TAG, "credential vault credentials retained after 30s stable IP");
        } else {
            ESP_LOGW(TAG, "credential vault credential retention failed: %s",
                     esp_err_to_name(storage.credential_vault_result));
        }
        return;
    }
    if (write_attempted) {
        secure_zero(&credentials, sizeof(credentials));
        return;
    }
    if (!storage.ready || storage.busy || storage.pending) {
        secure_zero(&credentials, sizeof(credentials));
        return;
    }

    const uint32_t next_generation = storage.credential_vault_valid
                                         ? storage.credential_vault_generation + 1U
                                         : 1U;
    const esp_err_t result = flash_coordinator_request_credential_vault_write(
        credentials.ssid, credentials.password);
    secure_zero(&credentials, sizeof(credentials));
    taskENTER_CRITICAL(&s_status_lock);
    if (result == ESP_OK) {
        s_credential_vault_write_attempted = true;
        s_credential_vault_write_in_flight = true;
        s_credential_vault_expected_generation = next_generation;
        s_status.credential_vault_save_pending = true;
    } else if (result != ESP_ERR_INVALID_STATE && result != ESP_ERR_TIMEOUT) {
        s_credential_vault_write_attempted = true;
        s_status.last_result = result;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE && result != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "credential vault credential retention request failed: %s",
                 esp_err_to_name(result));
    }
}

static esp_err_t restore_credential_vault(void)
{
    if (!flash_coordinator_credential_vault_ready()) return ESP_ERR_NOT_SUPPORTED;
    char ssid[NP2_WIFI_SSID_MAX_BYTES + 1U] = {0};
    char password[NP2_WIFI_PASSWORD_MAX_BYTES + 1U] = {0};
    const esp_err_t load_result = flash_coordinator_copy_credential_vault(
        ssid, sizeof(ssid), password, sizeof(password));
    if (load_result != ESP_OK) {
        secure_zero(ssid, sizeof(ssid));
        secure_zero(password, sizeof(password));
        return load_result;
    }
    connectivity_request_t request = {.type = CONNECTIVITY_REQUEST_JOIN};
    memcpy(request.ssid, ssid, sizeof(request.ssid));
    memcpy(request.password, password, sizeof(request.password));
    const esp_err_t result = configure_station_from_request(&request);
    secure_zero(&request, sizeof(request));
    secure_zero(ssid, sizeof(ssid));
    secure_zero(password, sizeof(password));
    if (result == ESP_OK) {
        taskENTER_CRITICAL(&s_status_lock);
        s_status.credential_vault_saved = true;
        s_credential_vault_write_attempted = true;
        taskEXIT_CRITICAL(&s_status_lock);
        ESP_LOGI(TAG, "credential vault credentials restored to private mailbox");
    }
    return result;
}

static void run_station_loop(void)
{
    int64_t retry_due_at_us = 0;

    for (;;) {
        bool retry_requested = false;
        bool dns_reassociation_requested = false;
        bool online = false;
        taskENTER_CRITICAL(&s_status_lock);
        retry_requested = s_retry_pending;
        s_retry_pending = false;
        dns_reassociation_requested = s_dns_reassociation_pending;
        s_dns_reassociation_pending = false;
        online = s_status.online;
        taskEXIT_CRITICAL(&s_status_lock);

        if (online) {
            retry_due_at_us = 0;
            maybe_persist_credential_vault();
        } else if (retry_requested && credentials_are_active() && hosted_link_is_up()) {
            const uint32_t delay_ms = next_backoff_delay_ms();
            retry_due_at_us = esp_timer_get_time() + (int64_t)delay_ms * 1000LL;
            set_status(CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF, ESP_OK);
            ESP_LOGW(TAG, "station retry scheduled in %lu ms", (unsigned long)delay_ms);
        }

        connectivity_request_t request = {0};
        bool request_received = false;
        taskENTER_CRITICAL(&s_request_lock);
        if (s_request_pending) {
            memcpy(&request, &s_request_mailbox, sizeof(request));
            secure_zero(&s_request_mailbox, sizeof(s_request_mailbox));
            s_request_pending = false;
            request_received = true;
        }
        taskEXIT_CRITICAL(&s_request_lock);
        if (request_received) {
            process_request(&request);
            retry_due_at_us = 0;
            continue;
        }

        if (dns_reassociation_requested && credentials_are_active() && hosted_link_is_up()) {
            ESP_LOGW(TAG, "DNS policy requested one bounded reassociation");
            request_station_retry(ESP_ERR_TIMEOUT);
            const esp_err_t disconnect_err = esp_wifi_disconnect();
            if (disconnect_err != ESP_OK && disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {
                ESP_LOGW(TAG, "DNS policy disconnect failed: %s",
                         esp_err_to_name(disconnect_err));
            }
        }

        if (station_deadline_expired() && credentials_are_active() && hosted_link_is_up()) {
            bool dhcp_silence_timed_out = false;
            taskENTER_CRITICAL(&s_status_lock);
            dhcp_silence_timed_out = s_dhcp_silence_active;
            if (dhcp_silence_timed_out) {
                s_dhcp_silence_active = false;
                s_dhcp_silence_timeout_observed = true;
            }
            taskEXIT_CRITICAL(&s_status_lock);
            if (dhcp_silence_timed_out && s_station_netif != NULL) {
                const esp_err_t dhcp_start_result = esp_netif_dhcpc_start(s_station_netif);
                if (dhcp_start_result != ESP_OK) {
                    ESP_LOGW(TAG, "DHCP silence injection restore failed: %s",
                             esp_err_to_name(dhcp_start_result));
                } else {
                    ESP_LOGI(TAG, "DHCP silence injection deadline observed; restoring DHCP");
                }
            }
            ESP_LOGW(TAG, "station association or DHCP deadline expired");
            request_station_retry(ESP_ERR_TIMEOUT);
            const esp_err_t disconnect_err = esp_wifi_disconnect();
            if (disconnect_err != ESP_OK && disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {
                ESP_LOGW(TAG, "station timeout disconnect failed: %s",
                         esp_err_to_name(disconnect_err));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(NP2_CONNECTIVITY_POLL_MS));

        if (retry_due_at_us != 0 && esp_timer_get_time() >= retry_due_at_us &&
            credentials_are_active() && hosted_link_is_up()) {
            retry_due_at_us = 0;
            set_station_deadline(CONNECTIVITY_DIAGNOSTIC_STATE_ASSOCIATING, ESP_OK,
                                 NP2_WIFI_ASSOCIATION_TIMEOUT_MS);
            const esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                request_station_retry(err);
                ESP_LOGW(TAG, "station retry request failed: %s", esp_err_to_name(err));
            }
        }
    }
}

static esp_err_t start_ram_only_wifi(void)
{
    const wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    if (err != ESP_OK) {
        (void)esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
        return err;
    }
    s_wifi_events_registered = true;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_set_inactive_time(WIFI_IF_STA, NP2_WIFI_STA_INACTIVE_TIME_SECONDS);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t inactive_time_seconds = 0;
    err = esp_wifi_get_inactive_time(WIFI_IF_STA, &inactive_time_seconds);
    if (err != ESP_OK || inactive_time_seconds != NP2_WIFI_STA_INACTIVE_TIME_SECONDS) {
        return err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }

    taskENTER_CRITICAL(&s_status_lock);
    s_status.wifi_ready = true;
    taskEXIT_CRITICAL(&s_status_lock);
    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_WIFI_READY, ESP_OK);
    return ESP_OK;
}

static esp_err_t stop_ram_only_wifi(void)
{
    if (s_wifi_events_registered) {
        (void)esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
        (void)esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);
        s_wifi_events_registered = false;
    }

    esp_err_t stop_result = esp_wifi_stop();
    if (stop_result != ESP_OK && stop_result != ESP_ERR_WIFI_NOT_STARTED &&
        stop_result != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "esp_wifi_stop during Hosted recovery: %s",
                 esp_err_to_name(stop_result));
    }
    const esp_err_t deinit_result = esp_wifi_deinit();
    if (deinit_result != ESP_OK && deinit_result != ESP_ERR_WIFI_NOT_INIT) {
        return deinit_result;
    }
    return ESP_OK;
}

static bool begin_hosted_recovery_cycle(void)
{
    const int64_t now_us = esp_timer_get_time();
    uint32_t retained = 0;
    bool allowed = false;

    taskENTER_CRITICAL(&s_status_lock);
    if (now_us < s_recovery_cooldown_until_us) {
        taskEXIT_CRITICAL(&s_status_lock);
        return false;
    }
    if (s_recovery_cooldown_until_us != 0) {
        /* A completed cooldown starts a fresh ten-minute recovery window. */
        memset(s_recovery_attempts_us, 0, sizeof(s_recovery_attempts_us));
        s_recovery_cooldown_until_us = 0;
        s_status.recovery_cycles = 0;
    }
    for (uint32_t index = 0; index < NP2_HOSTED_RECOVERY_MAX_CYCLES; ++index) {
        if (s_recovery_attempts_us[index] != 0 &&
            now_us - s_recovery_attempts_us[index] <=
                (int64_t)NP2_HOSTED_RECOVERY_WINDOW_MS * 1000LL) {
            s_recovery_attempts_us[retained++] = s_recovery_attempts_us[index];
        }
    }
    for (uint32_t index = retained; index < NP2_HOSTED_RECOVERY_MAX_CYCLES; ++index) {
        s_recovery_attempts_us[index] = 0;
    }
    if (retained < NP2_HOSTED_RECOVERY_MAX_CYCLES) {
        s_recovery_attempts_us[retained++] = now_us;
        s_status.recovery_cycles = retained;
        s_status.state = CONNECTIVITY_DIAGNOSTIC_STATE_RECOVERING_LINK;
        s_status.last_result = ESP_OK;
        s_status.link_up = false;
        s_status.wifi_ready = false;
        s_status.online = false;
        memset(s_status.connected_ssid, 0, sizeof(s_status.connected_ssid));
        s_station_deadline_us = 0;
        s_retry_pending = false;
        s_hosted_recovery_in_progress = true;
        allowed = true;
    } else {
        s_recovery_cooldown_until_us =
            now_us + (int64_t)NP2_HOSTED_RECOVERY_COOLDOWN_MS * 1000LL;
        s_status.recovery_cycles = retained;
        s_status.last_result = ESP_ERR_TIMEOUT;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    return allowed;
}

static esp_err_t recover_hosted_link(void)
{
    if (!begin_hosted_recovery_cycle()) {
        return ESP_ERR_TIMEOUT;
    }

    connectivity_request_t active_request = {0};
    bool restore_station = false;
    taskENTER_CRITICAL(&s_status_lock);
    restore_station = s_active_station_request_valid;
    if (restore_station) {
        memcpy(&active_request, &s_active_station_request, sizeof(active_request));
    }
    taskEXIT_CRITICAL(&s_status_lock);

    const esp_err_t stop_result = stop_ram_only_wifi();
    if (stop_result != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi teardown before Hosted recovery: %s",
                 esp_err_to_name(stop_result));
    }
    if (s_station_netif != NULL) {
        esp_netif_destroy(s_station_netif);
        s_station_netif = NULL;
    }
    esp_err_t err = (esp_err_t)esp_hosted_deinit();
    if (err == ESP_OK) {
        /* Network Split requires WIFI_STA_DEF before Hosted initialization. */
        s_station_netif = esp_netif_create_default_wifi_sta();
        err = s_station_netif == NULL ? ESP_ERR_NO_MEM : ESP_OK;
    }
    if (err == ESP_OK) {
        err = (esp_err_t)esp_hosted_init();
    }
    if (err == ESP_OK) {
        err = (esp_err_t)esp_hosted_connect_to_slave();
    }
    if (err == ESP_OK) {
        taskENTER_CRITICAL(&s_status_lock);
        s_status.link_up = true;
        taskEXIT_CRITICAL(&s_status_lock);
        err = start_ram_only_wifi();
    }
    if (err == ESP_OK && restore_station) {
        err = configure_station_from_request(&active_request);
    }
    secure_zero(&active_request, sizeof(active_request));
    taskENTER_CRITICAL(&s_status_lock);
    s_hosted_recovery_in_progress = false;
    taskEXIT_CRITICAL(&s_status_lock);
    if (err != ESP_OK) {
        fail_probe("Hosted recovery", err);
    }
    return err;
}

static void connectivity_probe_task(void *arg)
{
    (void)arg;

    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_STARTING, ESP_OK);

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        fail_probe("esp_netif_init", err);
        vTaskDelete(NULL);
        return;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        fail_probe("esp_event_loop_create_default", err);
        vTaskDelete(NULL);
        return;
    }

    err = esp_event_handler_register(EH_HOST_EVENT, ESP_EVENT_ANY_ID,
                                     hosted_event_handler, NULL);
    if (err != ESP_OK) {
        fail_probe("Hosted event registration", err);
        vTaskDelete(NULL);
        return;
    }

    /* Network Split requires WIFI_STA_DEF before esp_hosted_init(). */
    s_station_netif = esp_netif_create_default_wifi_sta();
    if (s_station_netif == NULL) {
        fail_probe("esp_netif_create_default_wifi_sta", ESP_FAIL);
        vTaskDelete(NULL);
        return;
    }

    err = start_hosted_with_supervisor();
    if (err != ESP_OK) {
        fail_probe("Hosted startup", err);
        vTaskDelete(NULL);
        return;
    }

    taskENTER_CRITICAL(&s_status_lock);
    s_status.link_up = true;
    taskEXIT_CRITICAL(&s_status_lock);
    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_LINK_UP, ESP_OK);
    ESP_LOGI(TAG, "Hosted SDIO link is up; initializing Wi-Fi station");

    err = start_ram_only_wifi();
    if (err != ESP_OK) {
        fail_probe("Wi-Fi startup", err);
        vTaskDelete(NULL);
        return;
    }

    const wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_SCANNING, ESP_OK);
    err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        fail_probe("esp_wifi_scan_start", err);
        vTaskDelete(NULL);
        return;
    }

    uint16_t access_points_found = 0;
    err = esp_wifi_scan_get_ap_num(&access_points_found);
    if (err != ESP_OK) {
        fail_probe("esp_wifi_scan_get_ap_num", err);
        vTaskDelete(NULL);
        return;
    }

    static wifi_ap_record_t records[NP2_WIFI_SCAN_RESULTS_MAX];
    memset(records, 0, sizeof(records));
    uint16_t records_count = access_points_found < NP2_WIFI_SCAN_RESULTS_MAX
                                 ? access_points_found : NP2_WIFI_SCAN_RESULTS_MAX;
    err = records_count > 0U ? esp_wifi_scan_get_ap_records(&records_count, records) : ESP_OK;
    if (err != ESP_OK) {
        fail_probe("esp_wifi_scan_get_ap_records", err);
        vTaskDelete(NULL);
        return;
    }
    taskENTER_CRITICAL(&s_status_lock);
    s_status.access_points_found = access_points_found;
    s_status.scan_results_count = (uint8_t)records_count;
    memset(s_status.scan_results, 0, sizeof(s_status.scan_results));
    for (uint16_t index = 0; index < records_count; ++index) {
        memcpy(s_status.scan_results[index].ssid, records[index].ssid,
               sizeof(s_status.scan_results[index].ssid) - 1U);
        s_status.scan_results[index].rssi = records[index].rssi;
        s_status.scan_results[index].secure = records[index].authmode != WIFI_AUTH_OPEN;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    set_status(CONNECTIVITY_DIAGNOSTIC_STATE_SCAN_COMPLETE, ESP_OK);
    ESP_LOGI(TAG, "credential-free Wi-Fi scan complete: %u AP(s)",
             (unsigned int)access_points_found);

    const esp_err_t restore_result = restore_credential_vault();
    if (restore_result != ESP_OK && restore_result != ESP_ERR_NOT_FOUND &&
        restore_result != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "credential vault credential restore skipped: %s",
                 esp_err_to_name(restore_result));
    }

    run_station_loop();
}

esp_err_t connectivity_diagnostic_start(void)
{
    taskENTER_CRITICAL(&s_status_lock);
    if (s_started) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    taskEXIT_CRITICAL(&s_status_lock);

    const BaseType_t task_created = xTaskCreate(connectivity_probe_task,
                                                 "np2_connectivity",
                                                 NP2_CONNECTIVITY_TASK_STACK_BYTES,
                                                 NULL,
                                                 NP2_CONNECTIVITY_TASK_PRIORITY,
                                                 NULL);
    if (task_created != pdPASS) {
        taskENTER_CRITICAL(&s_status_lock);
        s_started = false;
        taskEXIT_CRITICAL(&s_status_lock);
        set_status(CONNECTIVITY_DIAGNOSTIC_STATE_FAILED, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_join(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL || !s_started) {
        return ESP_ERR_INVALID_ARG;
    }

    connectivity_request_t request = {0};
    request.type = CONNECTIVITY_REQUEST_JOIN;
    const size_t ssid_length = bounded_length(ssid, sizeof(request.ssid));
    const size_t password_length = bounded_length(password, sizeof(request.password));
    if (ssid_length == 0U || ssid_length >= sizeof(request.ssid) ||
        password_length >= sizeof(request.password) ||
        (password_length > 0U && password_length < 8U)) {
        secure_zero(&request, sizeof(request));
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(request.ssid, ssid, ssid_length);
    memcpy(request.password, password, password_length);

    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        secure_zero(&request, sizeof(request));
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    secure_zero(&request, sizeof(request));
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_scan(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    const connectivity_request_t request = {.type = CONNECTIVITY_REQUEST_SCAN};
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_forget(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    connectivity_request_t request = {
        .type = CONNECTIVITY_REQUEST_FORGET,
    };
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        secure_zero(&request, sizeof(request));
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    secure_zero(&request, sizeof(request));
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_hosted_recovery(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    const connectivity_request_t request = {
        .type = CONNECTIVITY_REQUEST_RECOVER_HOSTED,
    };
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_hosted_recovery_cooldown_campaign(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    const connectivity_request_t request = {
        .type = CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_CAMPAIGN,
    };
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_hosted_recovery_full_cooldown_campaign(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    const connectivity_request_t request = {
        .type = CONNECTIVITY_REQUEST_RECOVER_HOSTED_COOLDOWN_FULL_CAMPAIGN,
    };
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    return ESP_OK;
}

esp_err_t connectivity_diagnostic_request_dhcp_silence_injection(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    const connectivity_request_t request = {
        .type = CONNECTIVITY_REQUEST_DHCP_SILENCE_INJECTION,
    };
    taskENTER_CRITICAL(&s_request_lock);
    if (s_request_pending) {
        taskEXIT_CRITICAL(&s_request_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(&s_request_mailbox, &request, sizeof(request));
    s_request_pending = true;
    taskEXIT_CRITICAL(&s_request_lock);
    return ESP_OK;
}

void connectivity_diagnostic_get_status(connectivity_diagnostic_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }

    taskENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
}

void connectivity_diagnostic_report_dns_result(esp_err_t result)
{
    const int64_t now_us = esp_timer_get_time();
    taskENTER_CRITICAL(&s_status_lock);
    if (result == ESP_OK) {
        s_status.consecutive_dns_failures = 0;
    } else if (s_status.consecutive_dns_failures < UINT32_MAX) {
        ++s_status.consecutive_dns_failures;
        if (s_status.consecutive_dns_failures >= 2U &&
            s_status.station_credentials_in_ram && now_us >= s_next_dns_reassociation_us) {
            s_dns_reassociation_pending = true;
            s_next_dns_reassociation_us = now_us +
                                           (int64_t)NP2_DNS_REASSOCIATION_COOLDOWN_MS * 1000LL;
        }
    }
    taskEXIT_CRITICAL(&s_status_lock);
}
