#include "app_state.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "app_event_bus.h"
#include "connectivity_diagnostic.h"
#include "esp_log.h"
#include "flash_coordinator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_service.h"

#define APP_LOOP_STACK_BYTES 8192U
#define APP_LOOP_PRIORITY 3U
#define APP_STATE_REFRESH_PERIOD_MS 1000U
#define APP_WEATHER_STALE_AFTER_S UINT32_C(7200)
#define APP_MARKET_STALE_AFTER_S UINT32_C(1800)
#define APP_VALID_EPOCH_SECONDS UINT32_C(1735689600)

static const char *const TAG = "app_state";

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static app_ui_projection_t s_projection;
static bool s_started;

static app_network_state_t map_network_state(connectivity_diagnostic_state_t state)
{
    switch (state) {
    case CONNECTIVITY_DIAGNOSTIC_STATE_STARTING:
        return APP_NETWORK_STATE_STARTING;
    case CONNECTIVITY_DIAGNOSTIC_STATE_LINK_UP:
        return APP_NETWORK_STATE_LINK_UP;
    case CONNECTIVITY_DIAGNOSTIC_STATE_WIFI_READY:
        return APP_NETWORK_STATE_WIFI_READY;
    case CONNECTIVITY_DIAGNOSTIC_STATE_SCANNING:
        return APP_NETWORK_STATE_SCANNING;
    case CONNECTIVITY_DIAGNOSTIC_STATE_SCAN_COMPLETE:
        return APP_NETWORK_STATE_SCAN_COMPLETE;
    case CONNECTIVITY_DIAGNOSTIC_STATE_ASSOCIATING:
        return APP_NETWORK_STATE_ASSOCIATING;
    case CONNECTIVITY_DIAGNOSTIC_STATE_WAITING_FOR_IP:
        return APP_NETWORK_STATE_WAITING_FOR_IP;
    case CONNECTIVITY_DIAGNOSTIC_STATE_ONLINE:
        return APP_NETWORK_STATE_ONLINE;
    case CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF:
        return APP_NETWORK_STATE_BACKOFF;
    case CONNECTIVITY_DIAGNOSTIC_STATE_LINK_DOWN:
        return APP_NETWORK_STATE_LINK_DOWN;
    case CONNECTIVITY_DIAGNOSTIC_STATE_RECOVERING_LINK:
        return APP_NETWORK_STATE_RECOVERING_LINK;
    case CONNECTIVITY_DIAGNOSTIC_STATE_FAILED:
        return APP_NETWORK_STATE_FAILED;
    case CONNECTIVITY_DIAGNOSTIC_STATE_IDLE:
    default:
        return APP_NETWORK_STATE_IDLE;
    }
}

static uint32_t snapshot_age_s(uint32_t observed_at_unix_s, bool time_trusted)
{
    const time_t now = time(NULL);
    if (!time_trusted || observed_at_unix_s == 0U ||
        now < (time_t)APP_VALID_EPOCH_SECONDS) {
        return UINT32_MAX;
    }
    const uint64_t current_time = (uint64_t)now;
    return current_time > observed_at_unix_s
               ? (current_time - observed_at_unix_s > UINT32_MAX
                      ? UINT32_MAX
                      : (uint32_t)(current_time - observed_at_unix_s))
               : 0U;
}

static offline_data_snapshot_t project_offline_data(const flash_coordinator_status_t *storage,
                                                    bool time_trusted,
                                                    uint32_t *out_weather_age_s,
                                                    uint32_t *out_market_age_s)
{
    if (out_weather_age_s != NULL) {
        *out_weather_age_s = UINT32_MAX;
    }
    if (out_market_age_s != NULL) {
        *out_market_age_s = UINT32_MAX;
    }
    if (storage == NULL || !storage->offline_data_valid) {
        return (offline_data_snapshot_t){0};
    }
    offline_data_snapshot_t projection = storage->offline_data;
    projection.origin = OFFLINE_DATA_ORIGIN_CACHE;
    if (projection.weather.available) {
        const uint32_t age_s =
            snapshot_age_s(projection.weather.observed_at_unix_s, time_trusted);
        if (out_weather_age_s != NULL) {
            *out_weather_age_s = age_s;
        }
        projection.weather.stale = projection.weather.stale || age_s == UINT32_MAX ||
                                   age_s > APP_WEATHER_STALE_AFTER_S;
    }
    if (projection.market.available) {
        const uint32_t age_s =
            snapshot_age_s(projection.market.observed_at_unix_s, time_trusted);
        if (out_market_age_s != NULL) {
            *out_market_age_s = age_s;
        }
        projection.market.stale = projection.market.stale || age_s == UINT32_MAX ||
                                  age_s > APP_MARKET_STALE_AFTER_S;
    }
    return projection;
}

static void refresh_projection(void)
{
    flash_coordinator_status_t storage = {0};
    connectivity_diagnostic_status_t network = {0};
    time_service_status_t time_status = {0};
    app_ui_projection_t candidate = {0};

    flash_coordinator_get_status(&storage);
    connectivity_diagnostic_get_status(&network);
    time_service_get_status(&time_status);

    candidate.ready = true;
    candidate.storage = (app_storage_projection_t){
        .ready = storage.ready,
        .busy = storage.busy,
        .pending = storage.pending,
        .littlefs_ready = storage.littlefs_ready,
        .last_littlefs_format = storage.last_littlefs_format,
        .cache_valid = storage.cache_valid,
        .config_valid = storage.config_valid,
        .completed_count = storage.completed_count,
        .last_sequence = storage.last_sequence,
        .last_duration_ms = storage.last_duration_ms,
        .last_batch_writes = storage.last_batch_writes,
        .last_free_entries_before = storage.last_free_entries_before,
        .last_free_entries_after = storage.last_free_entries_after,
        .last_littlefs_writes = storage.last_littlefs_writes,
        .last_littlefs_verified_bytes = storage.last_littlefs_verified_bytes,
        .cache_generation = storage.cache_generation,
        .cache_schema_version = storage.cache_schema_version,
        .config_generation = storage.config_generation,
        .init_result = storage.init_result,
        .littlefs_init_result = storage.littlefs_init_result,
        .cache_result = storage.cache_result,
        .config_result = storage.config_result,
        .last_result = storage.last_result,
    };
    candidate.network = (app_network_projection_t){
        .state = map_network_state(network.state),
        .access_points_found = network.access_points_found,
        .reconnect_attempts = network.reconnect_attempts,
        .transport_failures = network.transport_failures,
        .online = network.online,
        .last_result = network.last_result,
    };
    candidate.time_trusted = time_status.trusted;
    candidate.last_time_sync_unix_s = time_status.last_sync_unix_s;
    candidate.offline_data = project_offline_data(&storage, candidate.time_trusted,
                                                   &candidate.weather_age_s,
                                                   &candidate.market_age_s);

    portENTER_CRITICAL(&s_state_lock);
    const uint32_t prior_revision = s_projection.revision;
    if (memcmp(&candidate.ready, &s_projection.ready,
               sizeof(candidate) - offsetof(app_ui_projection_t, ready)) != 0) {
        candidate.revision = prior_revision + 1U;
        s_projection = candidate;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

static void app_loop_task(void *arg)
{
    (void)arg;
    refresh_projection();
    for (;;) {
        app_event_t event = {0};
        const esp_err_t result =
            app_event_bus_receive(&event, APP_STATE_REFRESH_PERIOD_MS);
        if (result == ESP_OK && event.type != APP_EVENT_REFRESH_PLATFORM) {
            ESP_LOGW(TAG, "discarded unknown app event %u", (unsigned int)event.type);
        }
        refresh_projection();
    }
}

esp_err_t app_state_start(void)
{
    portENTER_CRITICAL(&s_state_lock);
    if (s_started) {
        portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    portEXIT_CRITICAL(&s_state_lock);

    esp_err_t result = app_event_bus_start();
    if (result != ESP_OK) {
        portENTER_CRITICAL(&s_state_lock);
        s_started = false;
        portEXIT_CRITICAL(&s_state_lock);
        return result;
    }
    if (xTaskCreate(app_loop_task, "app_loop", APP_LOOP_STACK_BYTES, NULL,
                    APP_LOOP_PRIORITY, NULL) != pdPASS) {
        portENTER_CRITICAL(&s_state_lock);
        s_started = false;
        portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_state_request_refresh(void)
{
    const app_event_t event = {.type = APP_EVENT_REFRESH_PLATFORM};
    return app_event_bus_post(&event);
}

void app_state_get_ui_projection(app_ui_projection_t *out_projection)
{
    if (out_projection == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_lock);
    *out_projection = s_projection;
    portEXIT_CRITICAL(&s_state_lock);
}
