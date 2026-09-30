#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "offline_data_model.h"
#include "connectivity_diagnostic.h"
#include "user_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_NETWORK_STATE_IDLE = 0,
    APP_NETWORK_STATE_STARTING,
    APP_NETWORK_STATE_LINK_UP,
    APP_NETWORK_STATE_WIFI_READY,
    APP_NETWORK_STATE_SCANNING,
    APP_NETWORK_STATE_SCAN_COMPLETE,
    APP_NETWORK_STATE_ASSOCIATING,
    APP_NETWORK_STATE_WAITING_FOR_IP,
    APP_NETWORK_STATE_ONLINE,
    APP_NETWORK_STATE_BACKOFF,
    APP_NETWORK_STATE_LINK_DOWN,
    APP_NETWORK_STATE_RECOVERING_LINK,
    APP_NETWORK_STATE_FAILED,
} app_network_state_t;

/* This is a UI projection, not the complete application state. It contains no
 * password, request body or other secret material. Public SSIDs are allowed. */
typedef struct {
    bool ready;
    bool busy;
    bool pending;
    bool littlefs_ready;
    bool last_littlefs_format;
    bool cache_valid;
    bool config_valid;
    uint32_t completed_count;
    uint32_t last_sequence;
    uint32_t last_duration_ms;
    uint32_t last_batch_writes;
    uint32_t last_free_entries_before;
    uint32_t last_free_entries_after;
    uint32_t last_littlefs_writes;
    uint32_t last_littlefs_verified_bytes;
    uint32_t cache_generation;
    uint16_t cache_schema_version;
    uint32_t config_generation;
    esp_err_t init_result;
    esp_err_t littlefs_init_result;
    esp_err_t cache_result;
    esp_err_t config_result;
    esp_err_t last_result;
} app_storage_projection_t;

typedef struct {
    app_network_state_t state;
    uint16_t access_points_found;
    uint8_t scan_results_count;
    connectivity_scan_result_t scan_results[CONNECTIVITY_DIAGNOSTIC_MAX_SCAN_RESULTS];
    uint32_t reconnect_attempts;
    uint32_t transport_failures;
    /* Presence of an active private station configuration, never credentials. */
    bool credentials_active;
    bool online;
    char connected_ssid[33];
    char ip_address[16];
    char gateway[16];
    esp_err_t last_result;
} app_network_projection_t;

typedef struct {
    bool required;
    bool completed;
    bool clock_24h;
    uint16_t timezone_index;
    bool timezone_persistence_pending;
    uint8_t stage;
    esp_err_t last_result;
} app_onboarding_projection_t;

/* Only a stable animated icon descriptor crosses to the UI. The SD worker
 * owns mounting, buffers and file I/O. */
typedef struct {
    bool ready;
    bool mounted;
    bool pending;
    const void *icon_source;
    uint32_t generation;
    esp_err_t last_result;
} app_weather_asset_projection_t;

typedef struct {
    bool ready;
    bool general_enabled;
    bool sound_enabled;
    bool system_alerts_enabled;
    bool persistence_pending;
    uint32_t generation;
    uint32_t persisted_generation;
    esp_err_t last_result;
} app_notification_projection_t;

typedef struct {
    bool ready;
    uint8_t brightness_percent;
    uint8_t volume_percent;
    uint8_t effective_brightness_percent;
    bool night_mode_enabled;
    bool night_mode_active;
    bool persistence_pending;
    uint32_t save_completion_id;
    uint8_t save_completion_mask;
    esp_err_t save_result;
} app_device_control_projection_t;

typedef struct {
    char firmware_version[32];
    bool temperature_available;
    int16_t chip_temperature_deci_c;
    bool restart_pending;
    uint32_t restart_completion_id;
    esp_err_t restart_result;
} app_system_projection_t;

typedef struct {
    bool ready;
    bool configured;
    bool persistence_pending;
    uint32_t persisted_generation;
    esp_err_t last_result;
    user_profile_t profile;
} app_user_profile_projection_t;

typedef struct {
    uint32_t revision;
    bool ready;
    app_storage_projection_t storage;
    app_network_projection_t network;
    app_onboarding_projection_t onboarding;
    app_weather_asset_projection_t weather_assets;
    app_notification_projection_t notifications;
    app_device_control_projection_t device_controls;
    app_system_projection_t system;
    app_user_profile_projection_t user_profile;
    bool time_trusted;
    uint32_t current_unix_s;
    uint32_t last_time_sync_unix_s;
    offline_data_snapshot_t offline_data;
    uint32_t weather_age_s;
    uint32_t market_age_s;
    uint32_t exchange_age_s;
} app_ui_projection_t;

typedef struct {
    bool ready;
    uint64_t last_progress_ms;
} app_state_health_t;

/* Starts the 8 KiB app_loop task. It is the sole writer of the state below. */
esp_err_t app_state_start(void);

/* Coalescible notification for a consumer that has published new platform state. */
esp_err_t app_state_request_refresh(void);
/* Posts a bounded non-secret identity update to the app_loop writer. */
esp_err_t app_state_request_user_profile_update(const user_profile_t *profile);

/* Lock-protected copy for the LVGL task; no service I/O is performed here. */
void app_state_get_ui_projection(app_ui_projection_t *out_projection);

/* Read-only heartbeat produced by app_loop after each state projection pass. */
void app_state_get_health(app_state_health_t *out_health);

#ifdef __cplusplus
}
#endif
