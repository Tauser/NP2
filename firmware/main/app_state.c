#include "app_state.h"
#include "np_memory.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "app_event_bus.h"
#include "agenda_storage.h"
#include "google_agenda_sync.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "connectivity_diagnostic.h"
#include "device_control_service.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash_coordinator.h"
#include "offline_data_codec.h"
#include "onboarding_service.h"
#include "notification_service.h"
#include "np_ewelink.h"
#include "onvif_discovery_service.h"
#include "sonoff_lan_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time_service.h"
#include "weather_asset_service.h"
#include "photo_frame_service.h"

#define APP_LOOP_STACK_BYTES 8192U
#define APP_LOOP_PRIORITY 3U
#define APP_STATE_REFRESH_PERIOD_MS 1000U
#define APP_WEATHER_STALE_AFTER_S UINT32_C(14400)
#define APP_MARKET_STALE_AFTER_S UINT32_C(1800)
#define APP_EXCHANGE_STALE_AFTER_S UINT32_C(129600)
#if OFFLINE_DATA_SCHEMA_VERSION >= 4
#define APP_IBOV_STALE_AFTER_S UINT32_C(1800)
#define APP_INDEX_STALE_AFTER_S UINT32_C(1800)
#endif
#define APP_VALID_EPOCH_SECONDS UINT32_C(1735689600)

static const char *const TAG = "app_state";

static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static NP2_PSRAM_BSS app_ui_projection_t s_projection;
static app_state_health_t s_health;
static bool s_started;
/* Written only by app_loop after receiving a validated provider result. */
static NP2_PSRAM_BSS offline_data_snapshot_t s_live_offline_data;
static bool s_live_offline_data_valid;
/* The app_loop alone mutates identity. FlashCoordinator owns its NVS records. */
static user_profile_t s_user_profile;
static pomodoro_service_t s_pomodoro;
static bool s_pomodoro_completion_sound_latched;
static user_profile_t s_profile_submitted;
static bool s_user_profile_configured;
static bool s_profile_restored;
static bool s_profile_dirty;
static bool s_profile_inflight;
static uint32_t s_profile_sequence;
static uint32_t s_profile_persisted_generation;
static esp_err_t s_profile_result = ESP_OK;
static NP2_PSRAM_BSS app_notification_center_projection_t s_notification_center;
static NP2_PSRAM_BSS np_ewelink_public_inventory_t s_ewelink_public_inventory;
static bool s_notification_sources_initialized;
static bool s_previous_network_online;
static bool s_previous_network_credentials;
static bool s_previous_storage_ready;
static esp_err_t s_previous_storage_result = ESP_OK;
static uint32_t s_previous_restart_completion_id;
static uint32_t s_next_notification_id = 1U;
static NP2_PSRAM_BSS spotify_remote_projection_t s_spotify, s_spotify_published;
static uint8_t s_home_widget, s_home_submitted;
static bool s_home_restored, s_home_dirty, s_home_saving;
static uint32_t s_home_completion;
static uint64_t s_home_changed, s_home_last_write;
static agenda_data_t *s_agenda;
static NP2_PSRAM_BSS agenda_projection_t s_agenda_projection;
static agenda_projection_t *s_agenda_published;
static app_event_t *s_loop_event;
static void notification_center_push(app_notification_kind_t kind, uint32_t timestamp_unix_s);
static bool s_agenda_restored;
static bool s_agenda_dirty;
static bool s_agenda_inflight;
static uint32_t s_agenda_sequence;
static uint32_t s_agenda_generation;
static uint32_t s_agenda_submitted_generation;
static uint64_t s_agenda_changed_ms;
static uint64_t s_agenda_last_write_ms;
static uint64_t s_agenda_next_sync_ms;

static void agenda_changed(void)
{
    s_agenda_dirty = true;
    ++s_agenda_generation;
    s_agenda_changed_ms = (uint64_t)esp_timer_get_time() / 1000U;
    flash_coordinator_set_agenda_unsaved(true);
}

static void refresh_agenda(const flash_coordinator_status_t *storage,
                            const app_ui_projection_t *candidate)
{
    if (!s_agenda_restored && storage->agenda_loaded) {
        s_agenda_projection.storage_result = storage->agenda_load_result;
        if (storage->agenda_load_result == ESP_OK)
            s_agenda_projection.storage_result = agenda_storage_copy(s_agenda);
        s_agenda_projection.ready = storage->agenda_load_result == ESP_OK ||
            storage->agenda_load_result == ESP_ERR_NOT_FOUND;
        s_agenda_restored = true;
    }
    esp_err_t sync_result = ESP_OK;
    bool sync_changed = false;
    if (s_agenda_projection.sync_busy && google_agenda_take_result(s_agenda, &sync_result, &sync_changed)) {
        s_agenda_projection.sync_busy = false;
        s_agenda_projection.sync_result = sync_result;
        if (sync_result == AGENDA_SYNC_AUTH_REQUIRED) s_agenda_projection.google_auth_required = true;
        else if (sync_result == ESP_OK) s_agenda_projection.google_auth_required = false;
        if (sync_result == ESP_OK) s_agenda_projection.last_sync_unix_s = candidate->current_unix_s;
        /* Partial successes are also durable; never repeat an acknowledged POST. */
        if (sync_changed) agenda_changed();
    }
    if (s_agenda_inflight && storage->agenda_completed_sequence == s_agenda_sequence) {
        s_agenda_inflight = false;
        s_agenda_projection.storage_result = storage->agenda_write_result;
        if (storage->agenda_write_result != ESP_OK) s_agenda_dirty = true;
        else if (s_agenda_submitted_generation == s_agenda_generation) s_agenda_dirty = false;
    }
    if (s_agenda_projection.ready && !s_agenda_projection.sync_busy) {
        bool changed = false;
        agenda_item_t reminder = {0};
        const bool delivered = agenda_take_reminder(s_agenda, candidate->time_trusted,
            candidate->current_unix_s, &reminder, &changed);
        if (changed) agenda_changed();
        if (delivered && candidate->notifications.ready && candidate->notifications.general_enabled) {
            notification_center_push(reminder.kind == AGENDA_TASK
                ? APP_NOTIFICATION_KIND_AGENDA_TASK : APP_NOTIFICATION_KIND_AGENDA_EVENT,
                candidate->current_unix_s);
            app_notification_item_t *item = &s_notification_center.items[s_notification_center.count - 1U];
            memcpy(item->title, reminder.title, sizeof(item->title));
            (void)notification_service_request_alarm_sound();
        }
    }
    const uint64_t now_ms = (uint64_t)esp_timer_get_time() / 1000U;
    if (s_agenda_projection.ready && s_agenda_dirty && !s_agenda_inflight &&
        !s_agenda_projection.sync_busy && now_ms - s_agenda_changed_ms >= 5000U &&
        (s_agenda_last_write_ms == 0U || now_ms - s_agenda_last_write_ms >= 60000U)) {
        if (flash_coordinator_request_agenda_write(s_agenda, &s_agenda_sequence) == ESP_OK) {
            s_agenda_inflight = true;
            s_agenda_submitted_generation = s_agenda_generation;
            s_agenda_last_write_ms = now_ms;
        }
    }
    s_agenda_projection.google_linked = google_agenda_linked();
    if (s_agenda_projection.ready && s_agenda_projection.google_linked &&
        !s_agenda_projection.sync_busy && !s_agenda_dirty && !s_agenda_inflight &&
        candidate->network.online && candidate->time_trusted && now_ms >= s_agenda_next_sync_ms) {
        const esp_err_t result = google_agenda_request_sync(s_agenda);
        if (result == ESP_OK) {
            s_agenda_projection.sync_busy = true;
            s_agenda_projection.sync_result = ESP_OK;
        }
        s_agenda_next_sync_ms = now_ms + (result == ESP_OK ? 300000U : 15000U);
    }
    s_agenda_projection.persistence_pending = s_agenda_dirty || s_agenda_inflight;
    s_agenda_projection.generation = s_agenda_generation;
    flash_coordinator_set_agenda_unsaved(s_agenda_projection.persistence_pending ||
                                        s_agenda_projection.sync_busy);
    static uint64_t next_diagnostic_ms;
    if (now_ms >= next_diagnostic_ms) {
        next_diagnostic_ms = now_ms + 30000U;
        /* IDF 5.5.4 reports this API in bytes, unlike vanilla FreeRTOS. */
        ESP_LOGI("np2_agenda", "ready=%u items=%u pending=%u linked=%u sync_busy=%u sync_result=%ld app_stack_min_bytes=%u",
            (unsigned)s_agenda_projection.ready, (unsigned)s_agenda->count,
            (unsigned)s_agenda_projection.persistence_pending, (unsigned)s_agenda_projection.google_linked,
            (unsigned)s_agenda_projection.sync_busy, (long)s_agenda_projection.sync_result,
            (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
}

static bool user_profiles_equal(const user_profile_t *a, const user_profile_t *b)
{
    return a->avatar_color == b->avatar_color &&
           a->initial_screen == b->initial_screen &&
           memcmp(a->name, b->name, sizeof(a->name)) == 0;
}

static void refresh_user_profile(const flash_coordinator_status_t *storage)
{
    if (!storage->ready || storage->user_profile_result == ESP_ERR_INVALID_STATE) return;
    if (!s_profile_restored) {
        if (storage->user_profile_valid) {
            if (!s_profile_dirty) {
                s_user_profile = storage->user_profile;
                s_user_profile_configured = true;
            }
            s_profile_persisted_generation = storage->user_profile_generation;
        } else if (storage->user_profile_result != ESP_ERR_NOT_FOUND) {
            s_profile_result = storage->user_profile_result;
        }
        s_profile_restored = true;
    }
    if (s_profile_inflight &&
        storage->user_profile_completed_sequence == s_profile_sequence) {
        s_profile_inflight = false;
        if (storage->user_profile_last_write_result == ESP_OK &&
            storage->user_profile_valid &&
            user_profiles_equal(&storage->user_profile, &s_profile_submitted)) {
            s_profile_persisted_generation = storage->user_profile_generation;
            s_profile_result = ESP_OK;
        } else {
            s_profile_result = storage->user_profile_last_write_result == ESP_OK
                                   ? ESP_FAIL : storage->user_profile_last_write_result;
        }
    }
    if (!s_profile_dirty || s_profile_inflight || storage->busy || storage->pending) return;
    uint32_t sequence = 0U;
    const esp_err_t result = flash_coordinator_request_user_profile_write(
        &s_user_profile, &sequence);
    if (result == ESP_OK) {
        s_profile_submitted = s_user_profile;
        s_profile_sequence = sequence;
        s_profile_inflight = true;
        s_profile_dirty = false;
    } else if (result != ESP_ERR_INVALID_STATE && result != ESP_ERR_TIMEOUT) {
        s_profile_result = result;
        s_profile_dirty = false;
    }
}

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

static void notification_center_push(app_notification_kind_t kind,
                                     uint32_t timestamp_unix_s)
{
    if (s_notification_center.count == APP_NOTIFICATION_HISTORY_MAX) {
        if (s_notification_center.items[0].unread &&
            s_notification_center.unread_count > 0U) {
            --s_notification_center.unread_count;
        }
        memmove(&s_notification_center.items[0], &s_notification_center.items[1],
                sizeof(s_notification_center.items[0]) *
                    (APP_NOTIFICATION_HISTORY_MAX - 1U));
        --s_notification_center.count;
    }
    uint32_t id = s_next_notification_id++;
    if (id == 0U) id = s_next_notification_id++;
    s_notification_center.items[s_notification_center.count++] = (app_notification_item_t){
        .id = id,
        .timestamp_unix_s = timestamp_unix_s,
        .kind = kind,
        .unread = true,
    };
    if (s_notification_center.unread_count < APP_NOTIFICATION_HISTORY_MAX)
        ++s_notification_center.unread_count;
    if (kind != APP_NOTIFICATION_KIND_AGENDA_EVENT && kind != APP_NOTIFICATION_KIND_AGENDA_TASK)
        (void)notification_service_request_alert_sound();
}

static void notification_center_mark_read(uint32_t id, bool all)
{
    for (uint8_t i = 0U; i < s_notification_center.count; ++i) {
        app_notification_item_t *const item = &s_notification_center.items[i];
        if (item->unread && (all || item->id == id)) {
            item->unread = false;
            if (s_notification_center.unread_count > 0U)
                --s_notification_center.unread_count;
        }
    }
}

static void collect_notifications(app_ui_projection_t *candidate)
{
    if (candidate == NULL) return;
    if (!s_notification_sources_initialized) {
        s_previous_network_online = candidate->network.online;
        s_previous_network_credentials = candidate->network.credentials_active;
        s_previous_storage_ready = candidate->storage.ready;
        s_previous_storage_result = candidate->storage.last_result;
        s_previous_restart_completion_id = candidate->system.restart_completion_id;
        s_notification_sources_initialized = true;
    } else {
        if (candidate->network.credentials_active && s_previous_network_credentials &&
            s_previous_network_online != candidate->network.online &&
            candidate->notifications.ready &&
            candidate->notifications.general_enabled) {
            notification_center_push(candidate->network.online
                ? APP_NOTIFICATION_KIND_WIFI_RESTORED
                : APP_NOTIFICATION_KIND_WIFI_DISCONNECTED,
                candidate->current_unix_s);
        }

        if (candidate->notifications.ready &&
            candidate->notifications.system_alerts_enabled &&
            candidate->storage.ready && s_previous_storage_ready &&
            candidate->storage.last_result != s_previous_storage_result) {
            if (candidate->storage.last_result != ESP_OK &&
                candidate->storage.last_result != ESP_ERR_INVALID_STATE) {
                notification_center_push(APP_NOTIFICATION_KIND_STORAGE_ERROR,
                                         candidate->current_unix_s);
            } else if (candidate->storage.last_result == ESP_OK &&
                       s_previous_storage_result != ESP_OK) {
                notification_center_push(APP_NOTIFICATION_KIND_STORAGE_RECOVERED,
                                         candidate->current_unix_s);
            }
        }

        if (candidate->notifications.ready &&
            candidate->notifications.system_alerts_enabled &&
            candidate->system.restart_completion_id != 0U &&
            candidate->system.restart_completion_id !=
                s_previous_restart_completion_id &&
            candidate->system.restart_result != ESP_OK) {
            notification_center_push(APP_NOTIFICATION_KIND_RESTART_FAILED,
                                     candidate->current_unix_s);
        }

        s_previous_network_online = candidate->network.online;
        s_previous_network_credentials = candidate->network.credentials_active;
        s_previous_storage_ready = candidate->storage.ready;
        s_previous_storage_result = candidate->storage.last_result;
        s_previous_restart_completion_id = candidate->system.restart_completion_id;
    }
    candidate->notification_center = s_notification_center;
}

static offline_data_snapshot_t project_offline_data(const offline_data_snapshot_t *snapshot,
                                                    offline_data_origin_t origin,
                                                    bool time_trusted,
                                                    uint32_t *out_weather_age_s,
                                                    uint32_t *out_market_age_s,
                                                    uint32_t *out_exchange_age_s)
{
    if (out_weather_age_s != NULL) {
        *out_weather_age_s = UINT32_MAX;
    }
    if (out_market_age_s != NULL) {
        *out_market_age_s = UINT32_MAX;
    }
    if (out_exchange_age_s != NULL) {
        *out_exchange_age_s = UINT32_MAX;
    }
    if (snapshot == NULL || !offline_data_snapshot_is_valid(snapshot)) {
        return (offline_data_snapshot_t){0};
    }
    offline_data_snapshot_t projection = *snapshot;
    projection.origin = origin;
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
    if (projection.exchange.available) {
        const uint32_t age_s =
            snapshot_age_s(projection.exchange.observed_at_unix_s, time_trusted);
        if (out_exchange_age_s != NULL) {
            *out_exchange_age_s = age_s;
        }
        projection.exchange.stale = projection.exchange.stale || age_s == UINT32_MAX ||
                                    age_s > APP_EXCHANGE_STALE_AFTER_S;
    }
#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    if (projection.ibovespa.available) {
        const uint32_t age_s =
            snapshot_age_s(projection.ibovespa.observed_at_unix_s, time_trusted);
        projection.ibovespa.stale = projection.ibovespa.stale || age_s == UINT32_MAX ||
                                    age_s > APP_IBOV_STALE_AFTER_S;
    }
    if (projection.sp500.available) {
        const uint32_t age_s = snapshot_age_s(projection.sp500.observed_at_unix_s,
                                               time_trusted);
        projection.sp500.stale = projection.sp500.stale || age_s == UINT32_MAX ||
                                 age_s > APP_INDEX_STALE_AFTER_S;
    }
    if (projection.nasdaq.available) {
        const uint32_t age_s = snapshot_age_s(projection.nasdaq.observed_at_unix_s,
                                               time_trusted);
        projection.nasdaq.stale = projection.nasdaq.stale || age_s == UINT32_MAX ||
                                  age_s > APP_INDEX_STALE_AFTER_S;
    }
#endif
    return projection;
}

static void refresh_projection(void)
{
    flash_coordinator_status_t storage = {0};
    connectivity_diagnostic_status_t network = {0};
    time_service_status_t time_status = {0};
    onboarding_service_status_t onboarding = {0};
    weather_asset_service_status_t weather_assets = {0};
    notification_service_status_t notifications = {0};
    onvif_discovery_status_t onvif = {0};
    sonoff_lan_status_t sonoff = {0};
    device_control_status_t controls = {0};
    /* Projection snapshots contain the cached market payload and exceed the
     * spare stack budget of app_loop. This routine is only called by app_loop,
     * so keep its single-writer candidate in static storage. */
    static NP2_PSRAM_BSS app_ui_projection_t candidate;
    memset(&candidate, 0, sizeof(candidate));

    flash_coordinator_get_status(&storage);
    connectivity_diagnostic_get_status(&network);
    time_service_get_status(&time_status);
    onboarding_service_refresh();
    refresh_user_profile(&storage);
    onboarding_service_get_status(&onboarding);
    notification_service_get_status(&notifications);
    onvif_discovery_service_get_status(&onvif);
    sonoff_lan_service_get_status(&sonoff);
    device_control_get_status(&controls);

    if (time_status.ready && time_status.timezone_index != onboarding.timezone_index) {
        const esp_err_t timezone_result =
            time_service_set_timezone_index(onboarding.timezone_index);
        if (timezone_result != ESP_OK) {
            ESP_LOGW(TAG, "Timezone update unavailable: %s", esp_err_to_name(timezone_result));
        }
        time_service_get_status(&time_status);
    }

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
        .scan_results_count = network.scan_results_count,
        .reconnect_attempts = network.reconnect_attempts,
        .transport_failures = network.transport_failures,
        /* The UI may know a private station configuration exists so boot can
         * wait for it. It never receives its SSID or password. */
        .credentials_active = network.station_credentials_in_ram ||
                              storage.credential_vault_valid,
        .online = network.online,
        .last_result = network.last_result,
    };
    candidate.onboarding = (app_onboarding_projection_t){
        .required = onboarding.required,
        .completed = onboarding.completed,
        .clock_24h = onboarding.clock_24h,
        .timezone_index = onboarding.timezone_index,
        .timezone_persistence_pending = onboarding.timezone_persistence_pending,
        .stage = (uint8_t)onboarding.stage,
        .last_result = onboarding.last_result,
    };
    candidate.user_profile = (app_user_profile_projection_t){
        .ready = s_profile_restored,
        .configured = s_user_profile_configured,
        .persistence_pending = s_profile_dirty || s_profile_inflight,
        .persisted_generation = s_profile_persisted_generation,
        .last_result = s_profile_result,
        .profile = s_user_profile,
    };
    candidate.notifications = (app_notification_projection_t){
        .ready = notifications.ready,
        .general_enabled = notifications.general_enabled,
        .sound_enabled = notifications.sound_enabled,
        .system_alerts_enabled = notifications.system_alerts_enabled,
        .persistence_pending = notifications.persistence_pending,
        .generation = notifications.generation,
        .persisted_generation = notifications.persisted_generation,
        .last_result = notifications.last_result,
    };
    memcpy(candidate.notifications.melodies,notifications.melodies,sizeof(candidate.notifications.melodies));
    candidate.device_controls = (app_device_control_projection_t){
        .ready = controls.ready,
        .brightness_percent = controls.brightness_percent,
        .volume_percent = controls.volume_percent,
        .effective_brightness_percent = controls.effective_brightness_percent,
        .night_mode_enabled = controls.night_mode_enabled,
        .night_mode_active = controls.night_mode_active,
        .photo_config = controls.photo_config,
        .photo_bitcoin_widget = controls.photo_bitcoin_widget,
        .photo_clock_widget = controls.photo_clock_widget,
        .photo_idle_choice = controls.photo_idle_choice,
        .photo_interval_choice = controls.photo_interval_choice,
        .photo_exit_choice = controls.photo_exit_choice,
        .photo_enabled_mask = controls.photo_enabled_mask,
        .persistence_pending = controls.persistence_pending,
        .save_completion_id = controls.save_completion_id,
        .save_completion_mask = controls.save_completion_mask,
        .save_result = controls.save_result,
    };
    candidate.iot = (app_iot_projection_t){
        .ready = onvif.ready || sonoff.ready,
        .scan_busy = onvif.scan_busy,
        .scan_generation = onvif.scan_generation,
        .camera_count = onvif.camera_count,
        .last_result = onvif.last_result,
    };
    for (uint8_t i = 0U; i < onvif.camera_count &&
         i < APP_IOT_MAX_CAMERAS; ++i) {
        memcpy(candidate.iot.cameras[i].address, onvif.cameras[i].address,
               sizeof(candidate.iot.cameras[i].address));
        memcpy(candidate.iot.cameras[i].model, onvif.cameras[i].model,
               sizeof(candidate.iot.cameras[i].model));
        candidate.iot.cameras[i].online = onvif.cameras[i].online;
    }
    memset(&s_ewelink_public_inventory, 0, sizeof(s_ewelink_public_inventory));
    if (np_ewelink_get_public_inventory(&s_ewelink_public_inventory) == ESP_OK) {
        candidate.iot.ewelink_device_count =
            s_ewelink_public_inventory.count > APP_IOT_MAX_EWELINK_DEVICES
                ? APP_IOT_MAX_EWELINK_DEVICES : s_ewelink_public_inventory.count;
        for (uint8_t i = 0U; i < candidate.iot.ewelink_device_count; ++i) {
            const np_ewelink_public_device_t *const source =
                &s_ewelink_public_inventory.devices[i];
            app_iot_ewelink_device_projection_t *const target =
                &candidate.iot.ewelink_devices[i];
            memcpy(target->device_id, source->device_id, sizeof(target->device_id));
            memcpy(target->name, source->name, sizeof(target->name));
            memcpy(target->product_model, source->product_model,
                   sizeof(target->product_model));
            target->channel_count = source->channel_count;
            memcpy(target->channel_names, source->channel_names,
                   sizeof(target->channel_names));
            target->present_last_sync = source->present_last_sync;
            for (uint8_t local = 0U; local < sonoff.device_count &&
                 local < SONOFF_LAN_MAX_DISCOVERED; ++local) {
                const sonoff_lan_device_t *const lan = &sonoff.devices[local];
                if (strcmp(lan->device_id, source->device_id) != 0) continue;
                target->lan_discovered = true;
                target->lan_online = lan->online;
                for (uint8_t channel = 0U; channel < 3U; ++channel) {
                    target->channel_state_known[channel] = lan->state_known[channel];
                    target->channel_on[channel] = lan->switch_on[channel];
                    target->channel_pending[channel] = lan->channel_pending[channel];
                    target->channel_result[channel] = lan->last_result;
                }
                break;
            }
        }
    }
    memset(&s_ewelink_public_inventory, 0, sizeof(s_ewelink_public_inventory));
    memcpy(candidate.network.scan_results, network.scan_results, sizeof(candidate.network.scan_results));
    memcpy(candidate.system.firmware_version, controls.firmware_version,
           sizeof(candidate.system.firmware_version));
    candidate.system.temperature_available = controls.temperature_available;
    candidate.system.chip_temperature_deci_c = controls.chip_temperature_deci_c;
    candidate.system.resources = controls.resources;
    candidate.system.resources.maintenance = storage.busy || storage.pending || storage.p4_ota_active;
    candidate.system.resources.storage_available = storage.usage_available;
    candidate.system.resources.storage_total_bytes = storage.total_bytes;
    candidate.system.resources.storage_used_bytes = storage.used_bytes;
    candidate.system.restart_pending = storage.restart_pending;
    candidate.system.restart_completion_id = storage.restart_completion_id;
    candidate.system.restart_result = storage.restart_result;
    memcpy(candidate.network.connected_ssid, network.connected_ssid,
           sizeof(candidate.network.connected_ssid));
    memcpy(candidate.network.ip_address, network.ip_address, sizeof(candidate.network.ip_address));
    memcpy(candidate.network.gateway, network.gateway, sizeof(candidate.network.gateway));
    candidate.time_trusted = time_status.trusted;
    const time_t now = time(NULL);
    candidate.current_unix_s = candidate.time_trusted && now >= (time_t)APP_VALID_EPOCH_SECONDS &&
                                       (uint64_t)now <= UINT32_MAX
                                   ? (uint32_t)now
                                   : 0U;
    candidate.last_time_sync_unix_s = time_status.last_sync_unix_s;
    pomodoro_service_tick(&s_pomodoro,
                          (uint64_t)esp_timer_get_time() / UINT64_C(1000),
                          candidate.time_trusted, candidate.current_unix_s);
    if (s_pomodoro.projection.completed &&
        !s_pomodoro_completion_sound_latched) {
        (void)notification_service_request_alarm_sound();
        s_pomodoro_completion_sound_latched = true;
    } else if (!s_pomodoro.projection.completed) {
        s_pomodoro_completion_sound_latched = false;
    }
    pomodoro_service_copy(&s_pomodoro, &candidate.pomodoro);
    const offline_data_snapshot_t *const source = s_live_offline_data_valid
                                                       ? &s_live_offline_data
                                                       : (storage.offline_data_valid
                                                              ? &storage.offline_data
                                                              : NULL);
    candidate.offline_data = project_offline_data(
        source, s_live_offline_data_valid ? OFFLINE_DATA_ORIGIN_LIVE : OFFLINE_DATA_ORIGIN_CACHE,
        candidate.time_trusted, &candidate.weather_age_s, &candidate.market_age_s,
        &candidate.exchange_age_s);
    /* Log only transitions of the indicators, not prices, profile or network identity. */
    static unsigned int previous_data_flags = UINT32_MAX;
    const unsigned int data_flags =
        (candidate.offline_data.market.stale ? 1U : 0U) |
        (candidate.offline_data.weather.stale ? 2U : 0U) |
        (candidate.offline_data.exchange.stale ? 4U : 0U) |
        (candidate.offline_data.ibovespa.stale ? 8U : 0U) |
        (candidate.time_trusted ? 16U : 0U);
    if (data_flags != previous_data_flags) {
        ESP_LOGI("np2_data", "projection flags=0x%x trusted=%u market_age=%lu weather_age=%lu exchange_age=%lu",
                 data_flags, (unsigned int)candidate.time_trusted,
                 (unsigned long)candidate.market_age_s, (unsigned long)candidate.weather_age_s,
                 (unsigned long)candidate.exchange_age_s);
        previous_data_flags = data_flags;
    }
    if (!s_home_restored && storage.home_widget_loaded) {
        if (!s_home_dirty) s_home_widget = storage.home_widget;
        s_home_restored = true;
    }
    uint64_t home_now = (uint64_t)esp_timer_get_time()/1000U;
    if (s_home_saving && storage.home_widget_completion != s_home_completion) {
        s_home_saving = false;
        if (storage.home_widget_result == ESP_OK && s_home_widget == s_home_submitted) s_home_dirty = false;
    }
    if (s_home_restored && s_home_dirty && !s_home_saving && home_now-s_home_changed>=5000U &&
        (!s_home_last_write || home_now-s_home_last_write>=60000U) &&
        flash_coordinator_request_home_widget_write(s_home_widget)==ESP_OK) {
        s_home_saving=true; s_home_submitted=s_home_widget;
        s_home_completion=storage.home_widget_completion; s_home_last_write=home_now;
    }
    candidate.device_controls.home_widget = s_home_widget;
    refresh_agenda(&storage, &candidate);
    spotify_remote_tick(&s_spotify, candidate.network.online, candidate.time_trusted, (uint64_t)esp_timer_get_time()/1000U);
    collect_notifications(&candidate);

    /* app_loop only posts a desired visual to the SD worker. It never reads a
     * file itself, and the LVGL task consumes only the published descriptor. */
    struct tm local = {0};
    /* Sem NTP, use estritamente a última seleção persistida. Sem seleção
     * confirmada, o card continua neutro em vez de inferir dia ou noite. */
    uint8_t weather_visual_hour = 0U;
    bool has_weather_visual = candidate.offline_data.weather.available &&
                              candidate.offline_data.weather_visual.available;
    if (candidate.time_trusted && candidate.current_unix_s != 0U) {
        const time_t local_now = (time_t)candidate.current_unix_s;
        if (localtime_r(&local_now, &local) != NULL) {
            weather_visual_hour = (uint8_t)local.tm_hour;
            has_weather_visual = candidate.offline_data.weather.available;
        }
    } else if (has_weather_visual && candidate.offline_data.weather_visual.is_day) {
        weather_visual_hour = 12U;
    }
    if (has_weather_visual) {
        (void)weather_asset_service_request(weather_visual_hour,
                                            candidate.offline_data.weather.weather_code);
    }
    weather_asset_service_get_status(&weather_assets);
    if (controls.ready) {
        photo_frame_service_configure(controls.photo_config);
        photo_frame_service_set_options(controls.photo_interval_choice, controls.photo_enabled_mask);
    }
    photo_frame_service_get_status(&candidate.photos);
    candidate.system.resources.sd_mounted = weather_assets.mounted;
    candidate.system.resources.sd_available = weather_assets.usage_available;
    candidate.system.resources.sd_total_bytes = weather_assets.total_bytes;
    candidate.system.resources.sd_free_bytes = weather_assets.free_bytes;
    candidate.weather_assets = (app_weather_asset_projection_t){
        .ready = weather_assets.ready,
        .mounted = weather_assets.mounted,
        .pending = weather_assets.pending,
        .icon_source = NULL,
        .generation = weather_assets.generation,
        .last_result = weather_assets.last_result,
    };
    if (has_weather_visual && weather_assets.icon_available &&
        weather_assets.icon_is_day == weather_condition_is_day(weather_visual_hour) &&
        weather_assets.icon_condition ==
            weather_condition_from_code(candidate.offline_data.weather.weather_code)) {
        candidate.weather_assets.icon_source = weather_assets.icon_source;
    }

    portENTER_CRITICAL(&s_state_lock);
    const uint32_t prior_revision = s_projection.revision;
    if (memcmp(&candidate.ready, &s_projection.ready,
               sizeof(candidate) - offsetof(app_ui_projection_t, ready)) != 0) {
        candidate.revision = prior_revision + 1U;
        s_projection = candidate;
    }
    portEXIT_CRITICAL(&s_state_lock);

    portENTER_CRITICAL(&s_state_lock);
    s_agenda_projection.count = 0U;
    memset(s_agenda_projection.items, 0, sizeof(s_agenda_projection.items));
    for (uint8_t i = 0U; i < s_agenda->count; ++i) {
        if (!s_agenda->remote[i].deleted || s_agenda->remote[i].reserved[0]) {
            agenda_item_t *item = &s_agenda_projection.items[s_agenda_projection.count++];
            *item = s_agenda->items[i];
            item->reserved[1] = s_agenda->remote[i].reserved[0]
                ? (s_agenda->remote[i].reserved[1] ? 2U : 1U) : 0U;
        }
    }
    *s_agenda_published = s_agenda_projection;
    s_spotify_published = s_spotify;
    s_health.ready = true;
    s_health.last_progress_ms = (uint64_t)esp_timer_get_time() / 1000U;
    portEXIT_CRITICAL(&s_state_lock);
}

static void app_loop_task(void *arg)
{
    (void)arg;
    refresh_projection();
    for (;;) {
        /* The bounded event snapshot is normalized, non-secret data. Keep it
         * in explicitly allocated PSRAM instead of consuming the 8 KiB stack. */
        app_event_t *const event = s_loop_event;
        const esp_err_t result =
            app_event_bus_receive(event, APP_STATE_REFRESH_PERIOD_MS);
        if (result == ESP_OK && event->type == APP_EVENT_PRODUCT_DATA_UPDATED) {
            s_live_offline_data = event->offline_data;
            s_live_offline_data_valid = true;
        } else if (result == ESP_OK && event->type == APP_EVENT_USER_PROFILE_UPDATED) {
            if (!s_user_profile_configured ||
                !user_profiles_equal(&s_user_profile, &event->user_profile) ||
                s_profile_result != ESP_OK || s_profile_persisted_generation == 0U) {
                s_user_profile = event->user_profile;
                s_user_profile_configured = true;
                s_profile_dirty = true;
                s_profile_result = ESP_OK;
            }
        } else if (result == ESP_OK && event->type == APP_EVENT_HOME_WIDGET) {
            if (event->home_widget <= 1U && event->home_widget != s_home_widget) {
                s_home_widget = event->home_widget; s_home_dirty = true;
                s_home_changed = (uint64_t)esp_timer_get_time()/1000U;
            }
        } else if (result == ESP_OK && event->type == APP_EVENT_SPOTIFY_REMOTE) {
            spotify_remote_command(&s_spotify, &event->spotify_command);
        } else if (result == ESP_OK && event->type == APP_EVENT_AGENDA_COMMAND) {
            s_agenda_projection.completion_id = event->agenda_command.request_id;
            s_agenda_projection.command_result = !s_agenda_projection.ready ||
                s_agenda_projection.sync_busy ? AGENDA_NOT_READY :
                agenda_apply(s_agenda, &event->agenda_command);
            if (s_agenda_projection.command_result == AGENDA_OK) {
                if (!google_agenda_linked()) {
                    for (uint8_t i = 0U; i < s_agenda->count;) {
                        if (!s_agenda->remote[i].deleted || s_agenda->remote[i].id[0] != 0) { ++i; continue; }
                        --s_agenda->count;
                        memmove(&s_agenda->items[i], &s_agenda->items[i+1U], (s_agenda->count-i)*sizeof(s_agenda->items[0]));
                        memmove(&s_agenda->remote[i], &s_agenda->remote[i+1U], (s_agenda->count-i)*sizeof(s_agenda->remote[0]));
                        memset(&s_agenda->items[s_agenda->count],0,sizeof(s_agenda->items[0]));
                        memset(&s_agenda->remote[s_agenda->count],0,sizeof(s_agenda->remote[0]));
                    }
                }
                agenda_changed();
            }
        } else if (result == ESP_OK && event->type == APP_EVENT_AGENDA_SYNC) {
            esp_err_t sync = ESP_ERR_INVALID_STATE;
            if (s_agenda_projection.ready && !s_agenda_projection.sync_busy &&
                !s_agenda_dirty && !s_agenda_inflight)
                sync = google_agenda_request_sync(s_agenda);
            if (sync == ESP_OK) s_agenda_projection.sync_busy = true;
            s_agenda_next_sync_ms = (uint64_t)esp_timer_get_time()/1000U + 300000U;
            s_agenda_projection.sync_result = sync;
        } else if (result == ESP_OK && event->type == APP_EVENT_POMODORO_COMMAND) {
            pomodoro_service_apply(&s_pomodoro, event->pomodoro_command,
                event->pomodoro_value,
                (uint64_t)esp_timer_get_time() / UINT64_C(1000));
        } else if (result == ESP_OK && event->type == APP_EVENT_ONVIF_SCAN_REQUEST) {
            const esp_err_t scan_result = onvif_discovery_service_request_scan();
            if (scan_result != ESP_OK) {
                ESP_LOGW(TAG, "ONVIF scan request unavailable: %s",
                         esp_err_to_name(scan_result));
            }
        } else if (result == ESP_OK &&
                   event->type == APP_EVENT_ONVIF_ADDRESS_REQUEST) {
            const esp_err_t scan_result = onvif_discovery_service_request_address(
                event->onvif_address);
            if (scan_result != ESP_OK) {
                ESP_LOGW(TAG, "ONVIF address check unavailable: %s",
                         esp_err_to_name(scan_result));
            }
        } else if (result == ESP_OK &&
                   event->type == APP_EVENT_SONOFF_SWITCH_REQUEST) {
            const esp_err_t command_result = sonoff_lan_service_request_switch(
                event->sonoff_device_id, event->sonoff_channel,
                event->sonoff_enabled);
            if (command_result != ESP_OK) {
                ESP_LOGW(TAG, "Sonoff LAN switch request rejected: %s",
                         esp_err_to_name(command_result));
            }
        } else if (result == ESP_OK &&
                   event->type == APP_EVENT_NOTIFICATION_MARK_READ) {
            notification_center_mark_read(event->notification_id, false);
        } else if (result == ESP_OK &&
                   event->type == APP_EVENT_NOTIFICATION_MARK_ALL_READ) {
            notification_center_mark_read(0U, true);
        } else if (result == ESP_OK && event->type != APP_EVENT_REFRESH_PLATFORM) {
            ESP_LOGW(TAG, "discarded unknown app event %u", (unsigned int)event->type);
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
    s_agenda = heap_caps_calloc(1U, sizeof(*s_agenda), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_agenda_published = heap_caps_calloc(1U, sizeof(*s_agenda_published), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_loop_event = heap_caps_calloc(1U, sizeof(*s_loop_event), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_agenda == NULL || s_agenda_published == NULL || s_loop_event == NULL) {
        heap_caps_free(s_agenda); heap_caps_free(s_agenda_published);
        heap_caps_free(s_loop_event); s_loop_event = NULL;
        s_agenda = NULL; s_agenda_published = NULL;
        portENTER_CRITICAL(&s_state_lock); s_started = false; portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_NO_MEM;
    }
    agenda_init(s_agenda);
    s_agenda->instance_id = esp_random();
    if (s_agenda->instance_id == 0U) s_agenda->instance_id = 1U;
    pomodoro_service_init(&s_pomodoro);
    if (xTaskCreate(app_loop_task, "app_loop", APP_LOOP_STACK_BYTES, NULL,
                    APP_LOOP_PRIORITY, NULL) != pdPASS) {
        portENTER_CRITICAL(&s_state_lock);
        s_started = false;
        portEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_state_get_agenda_projection(agenda_projection_t *out_projection)
{
    if (out_projection == NULL) return;
    portENTER_CRITICAL(&s_state_lock);
    if (s_agenda_published != NULL) *out_projection = *s_agenda_published;
    else memset(out_projection, 0, sizeof(*out_projection));
    portEXIT_CRITICAL(&s_state_lock);
}

esp_err_t app_state_request_refresh(void)
{
    const app_event_t event = {.type = APP_EVENT_REFRESH_PLATFORM};
    return app_event_bus_post(&event);
}

esp_err_t app_state_request_user_profile_update(const user_profile_t *profile)
{
    if (!user_profile_is_valid(profile)) return ESP_ERR_INVALID_ARG;
    const app_event_t event = {
        .type = APP_EVENT_USER_PROFILE_UPDATED,
        .user_profile = *profile,
    };
    return app_event_bus_post(&event);
}

esp_err_t app_state_request_notification_mark_read(uint32_t id)
{
    return app_event_bus_post_notification_mark_read(id);
}

esp_err_t app_state_request_notification_mark_all_read(void)
{
    return app_event_bus_post_notification_mark_all_read();
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

void app_state_get_device_control_projection(
    app_device_control_projection_t *out_projection)
{
    if (out_projection == NULL) return;
    portENTER_CRITICAL(&s_state_lock);
    *out_projection = s_projection.device_controls;
    portEXIT_CRITICAL(&s_state_lock);
}

void app_state_get_health(app_state_health_t *out_health)
{
    if (out_health == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_state_lock);
    *out_health = s_health;
    portEXIT_CRITICAL(&s_state_lock);
}

void app_state_get_spotify_projection(spotify_remote_projection_t *out)
{
    if (out == NULL) return;
    portENTER_CRITICAL(&s_state_lock); *out = s_spotify_published; portEXIT_CRITICAL(&s_state_lock);
}
