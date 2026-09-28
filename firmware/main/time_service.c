#include "time_service.h"

#include <stdlib.h>
#include <time.h>

#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#define TIME_SERVICE_SYNC_TIMEOUT_MS 15000U
#define TIME_SERVICE_VALID_EPOCH_SECONDS 1735689600LL /* 2025-01-01 UTC */

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static time_service_status_t s_status = {
    .last_result = ESP_ERR_INVALID_STATE,
};
static bool s_started;
static bool s_sntp_initialized;

static esp_err_t apply_timezone(uint16_t timezone_index)
{
    char posix_timezone[72] = {0};
    if (!timezone_catalog_copy_posix(timezone_index, posix_timezone,
                                     sizeof(posix_timezone))) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Newlib owns internal locks while changing TZ. Keep this operation out of
     * the small FreeRTOS critical section used for service state. */
    if (setenv("TZ", posix_timezone, 1) != 0) return ESP_FAIL;
    tzset();
    return ESP_OK;
}

static uint32_t elapsed_ms_since(int64_t start_us)
{
    const int64_t elapsed_us = esp_timer_get_time() - start_us;
    return elapsed_us <= 0 ? 0U : (uint32_t)(elapsed_us / 1000LL);
}

static bool read_valid_system_time(uint32_t *out_unix_s)
{
    const time_t now = time(NULL);
    if (now < (time_t)TIME_SERVICE_VALID_EPOCH_SECONDS || (uint64_t)now > UINT32_MAX) {
        return false;
    }
    if (out_unix_s != NULL) {
        *out_unix_s = (uint32_t)now;
    }
    return true;
}

esp_err_t time_service_start(void)
{
    const esp_err_t timezone_result = apply_timezone(TIME_SERVICE_TIMEZONE_SAO_PAULO);
    if (timezone_result != ESP_OK) return timezone_result;

    portENTER_CRITICAL(&s_status_lock);
    if (s_started) {
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    s_status.ready = true;
    s_status.timezone_index = TIME_SERVICE_TIMEZONE_SAO_PAULO;
    s_status.last_result = ESP_ERR_INVALID_STATE;
    portEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t time_service_set_timezone_index(uint16_t timezone_index)
{
    if (!timezone_catalog_is_valid(timezone_index)) return ESP_ERR_INVALID_ARG;

    portENTER_CRITICAL(&s_status_lock);
    const bool started = s_started;
    const bool unchanged = s_status.timezone_index == timezone_index;
    portEXIT_CRITICAL(&s_status_lock);
    if (!started) return ESP_ERR_INVALID_STATE;
    if (unchanged) return ESP_OK;

    const esp_err_t result = apply_timezone(timezone_index);
    portENTER_CRITICAL(&s_status_lock);
    s_status.last_result = result;
    if (result == ESP_OK) s_status.timezone_index = timezone_index;
    portEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t time_service_sync(void)
{
    portENTER_CRITICAL(&s_status_lock);
    if (!s_started || s_status.sync_in_progress) {
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_status.sync_in_progress = true;
    portEXIT_CRITICAL(&s_status_lock);

    const int64_t start_us = esp_timer_get_time();
    esp_err_t result = ESP_OK;
    if (!s_sntp_initialized) {
        const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(TIME_SERVICE_NTP_HOST);
        result = esp_netif_sntp_init(&config);
        if (result == ESP_OK) {
            s_sntp_initialized = true;
        }
    } else {
        result = esp_netif_sntp_start();
    }
    if (result == ESP_OK) {
        result = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(TIME_SERVICE_SYNC_TIMEOUT_MS));
    }

    uint32_t unix_s = 0U;
    const bool trusted = result == ESP_OK && read_valid_system_time(&unix_s);
    if (result == ESP_OK && !trusted) {
        result = ESP_FAIL;
    }

    portENTER_CRITICAL(&s_status_lock);
    s_status.sync_in_progress = false;
    s_status.trusted = trusted;
    s_status.last_sync_unix_s = trusted ? unix_s : 0U;
    s_status.last_duration_ms = elapsed_ms_since(start_us);
    s_status.last_result = result;
    if (result == ESP_OK) {
        ++s_status.completed_syncs;
    }
    portEXIT_CRITICAL(&s_status_lock);
    return result;
}

void time_service_get_status(time_service_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_status_lock);
}
