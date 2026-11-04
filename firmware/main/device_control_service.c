#include "device_control_service.h"
#include "music_audio_service.h"

#include "flash_coordinator.h"
#include "night_mode_policy.h"
#include "time_service.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/temperature_sensor.h"

#include "bsp/display.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEVICE_CONTROL_DEFAULT_BRIGHTNESS 60U
#define DEVICE_CONTROL_DEFAULT_VOLUME     65U
/* Codec initialization and the coordinator's generic request both use stack. */
#define DEVICE_CONTROL_STACK_BYTES        12288U
#define DEVICE_CONTROL_PRIORITY           2U
#define DEVICE_CONTROL_POLL_MS            100U
#define DEVICE_CONTROL_SAVE_DELAY_MS      500U
#define DEVICE_CONTROL_SOUND_CHUNK_SAMPLES 256U
#define DEVICE_CONTROL_NOTIFICATION_TONE_MIN_INTERVAL_MS 350U

static const char *const TAG = "device_controls";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static esp_codec_dev_handle_t s_speaker;
static temperature_sensor_handle_t s_temperature_sensor;
static bool s_started;
static bool s_brightness_pending;
static bool s_volume_pending;
static uint8_t s_sound_pending, s_sound_cancel;
static uint8_t s_sound_variants[SOUND_CATEGORY_COUNT];
static TickType_t s_sound_ticks[SOUND_CATEGORY_COUNT];
static bool s_sound_queued[SOUND_CATEGORY_COUNT];
static sound_player_t s_player;
static int s_active_sound = -1;
static uint8_t s_requested_brightness;
static uint8_t s_requested_volume;
static uint8_t s_dirty_mask;
static bool s_save_enqueued;
static uint32_t s_pending_flash_sequence;
static uint8_t s_submitted_mask;
static device_control_profile_t s_submitted_profile;
static TickType_t s_last_change_tick;
static device_control_status_t s_status = {
    .brightness_percent = DEVICE_CONTROL_DEFAULT_BRIGHTNESS,
    .volume_percent = DEVICE_CONTROL_DEFAULT_VOLUME,
    .effective_brightness_percent = DEVICE_CONTROL_DEFAULT_BRIGHTNESS,
    .brightness_result = ESP_OK,
    .volume_result = ESP_OK,
    .save_result = ESP_OK,
};

static esp_err_t codec_result(int result)
{
    return result == ESP_CODEC_DEV_OK ? ESP_OK : (esp_err_t)result;
}

static esp_err_t apply_brightness(uint8_t percent)
{
    const esp_err_t result = bsp_display_brightness_set(percent);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Brightness update failed: %s", esp_err_to_name(result));
    }
    return result;
}

static esp_err_t apply_volume(uint8_t percent)
{
    esp_err_t result = ESP_OK;
    if (s_speaker == NULL) {
        s_speaker = bsp_audio_codec_speaker_init();
        if (s_speaker == NULL) {
            result = ESP_FAIL;
        } else {
            esp_codec_dev_sample_info_t format = {
                .bits_per_sample = 16,
                .channel = 1,
                .channel_mask = 0,
                .sample_rate = 22050,
                .mclk_multiple = 0,
            };
            result = codec_result(esp_codec_dev_open(s_speaker, &format));
        }
    }
    if (result == ESP_OK) {
        result = codec_result(esp_codec_dev_set_out_vol(s_speaker, percent));
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Volume update failed: %s", esp_err_to_name(result));
    }
    return result;
}

static void report_control_failure(uint8_t mask, esp_err_t result)
{
    portENTER_CRITICAL(&s_lock);
    s_status.save_result = result;
    s_status.save_completion_mask = mask;
    ++s_status.save_completion_id;
    portEXIT_CRITICAL(&s_lock);
}

static void restore_preferences(void)
{
    flash_coordinator_status_t flash = {0};
    flash_coordinator_get_status(&flash);
    if (!flash.ready) return;

    uint8_t brightness = DEVICE_CONTROL_DEFAULT_BRIGHTNESS;
    uint8_t volume = DEVICE_CONTROL_DEFAULT_VOLUME;
    if (flash.device_control_profile_valid) {
        brightness = flash.device_control_profile.brightness_percent;
        volume = flash.device_control_profile.volume_percent;
    } else if (flash.device_control_profile_result != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Controls profile unavailable: %s",
                 esp_err_to_name(flash.device_control_profile_result));
    }

    const esp_err_t brightness_result = apply_brightness(brightness);
    /* Audio is opened by this worker, away from the LVGL touch callback. */
    const esp_err_t volume_result = apply_volume(volume);
    char version[32] = {0};
    (void)snprintf(version, sizeof(version), "%s", esp_app_get_description()->version);
    portENTER_CRITICAL(&s_lock);
    s_status.brightness_percent = brightness;
    s_status.volume_percent = volume;
    s_status.effective_brightness_percent = brightness;
    s_status.night_mode_enabled = flash.device_control_profile_valid &&
                                 flash.device_control_profile.night_mode_enabled != 0U;
    s_status.photo_config = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_config : PHOTO_CONFIG_DEFAULT;
    s_status.photo_bitcoin_widget = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_bitcoin_widget : PHOTO_BITCOIN_WIDGET_DEFAULT;
    s_status.photo_clock_widget = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_clock_widget : PHOTO_CLOCK_WIDGET_DEFAULT;
    s_status.photo_idle_choice = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_idle_choice : PHOTO_IDLE_CHOICE_DEFAULT;
    s_status.photo_interval_choice = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_interval_choice : PHOTO_INTERVAL_CHOICE_DEFAULT;
    s_status.photo_exit_choice = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_exit_choice : PHOTO_EXIT_CHOICE_DEFAULT;
    s_status.photo_enabled_mask = flash.device_control_profile_valid
        ? flash.device_control_profile.photo_enabled_mask : UINT64_MAX;
    memcpy(s_status.firmware_version, version, sizeof(version));
    s_status.brightness_result = brightness_result;
    s_status.volume_result = volume_result;
    s_status.audio_ready = volume_result == ESP_OK;
    s_status.ready = true;
    const uint32_t idle_ms = photo_idle_choice_ms(s_status.photo_idle_choice);
    const uint32_t interval_ms = photo_interval_choice_ms(s_status.photo_interval_choice);
    const uint32_t exit_ms = photo_exit_choice_ms(s_status.photo_exit_choice);
    const uint8_t selected = photo_mask_count(s_status.photo_enabled_mask);
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "Photo preferences restored idle_ms=%lu interval_ms=%lu selected=%u exit_ms=%lu",
        (unsigned long)idle_ms, (unsigned long)interval_ms, selected, (unsigned long)exit_ms);
    ESP_LOGI(TAG, "Controls restored: brightness=%u volume=%u",
             (unsigned int)brightness, (unsigned int)volume);
}

static void apply_pending_controls(void)
{
    bool brightness_pending;
    bool volume_pending;
    uint8_t brightness;
    uint8_t volume;
    portENTER_CRITICAL(&s_lock);
    brightness_pending = s_brightness_pending;
    volume_pending = s_volume_pending;
    brightness = s_requested_brightness;
    volume = s_requested_volume;
    s_brightness_pending = false;
    s_volume_pending = false;
    portEXIT_CRITICAL(&s_lock);

    if (brightness_pending) {
        portENTER_CRITICAL(&s_lock);
        const uint8_t effective = night_mode_brightness(brightness, s_status.night_mode_active);
        portEXIT_CRITICAL(&s_lock);
        const esp_err_t result = apply_brightness(effective);
        portENTER_CRITICAL(&s_lock);
        s_status.brightness_result = result;
        if (result == ESP_OK) {
            s_status.brightness_percent = brightness;
            s_status.effective_brightness_percent = effective;
            s_dirty_mask |= DEVICE_CONTROL_BRIGHTNESS_MASK;
            s_status.persistence_pending = true;
            s_last_change_tick = xTaskGetTickCount();
        }
        portEXIT_CRITICAL(&s_lock);
        if (result != ESP_OK) report_control_failure(DEVICE_CONTROL_BRIGHTNESS_MASK, result);
    }
    if (volume_pending) {
        const esp_err_t result = apply_volume(volume);
        portENTER_CRITICAL(&s_lock);
        s_status.volume_result = result;
        s_status.audio_ready = result == ESP_OK;
        if (result == ESP_OK) {
            s_status.volume_percent = volume;
            s_dirty_mask |= DEVICE_CONTROL_VOLUME_MASK;
            s_status.persistence_pending = true;
            s_last_change_tick = xTaskGetTickCount();
        }
        portEXIT_CRITICAL(&s_lock);
        if (result != ESP_OK) report_control_failure(DEVICE_CONTROL_VOLUME_MASK, result);
    }
}

static void apply_night_schedule(void)
{
    time_service_status_t clock = {0};
    time_service_get_status(&clock);
    const time_t now = time(NULL);
    struct tm local = {0};
    const bool trusted = clock.trusted && localtime_r(&now, &local) != NULL;
    portENTER_CRITICAL(&s_lock);
    const bool active = night_mode_active(s_status.night_mode_enabled, trusted,
                                          (unsigned)local.tm_hour);
    const uint8_t brightness = night_mode_brightness(s_status.brightness_percent, active);
    const bool changed = brightness != s_status.effective_brightness_percent;
    s_status.night_mode_active = active;
    portEXIT_CRITICAL(&s_lock);
    if (!changed) return;
    const esp_err_t result = apply_brightness(brightness);
    portENTER_CRITICAL(&s_lock);
    s_status.brightness_result = result;
    if (result == ESP_OK) s_status.effective_brightness_percent = brightness;
    portEXIT_CRITICAL(&s_lock);
}

static bool play_pending_sound_chunk(void)
{
    static const sound_category_t priority[]={SOUND_ALARM,SOUND_NOTIFICATION,SOUND_STARTUP};
    portENTER_CRITICAL(&s_lock);
    const uint8_t cancel=s_sound_cancel; s_sound_cancel=0U;
    s_sound_pending &= (uint8_t)~cancel;
    const bool muted=s_status.volume_percent == 0U || !s_status.audio_ready;
    if (muted) s_sound_pending=0U;
    if (muted || (s_active_sound>=0 && (cancel&(1U<<s_active_sound)))) {
        s_player=(sound_player_t){0}; s_active_sound=-1;
    }
    if ((s_sound_pending&(1U<<SOUND_ALARM)) && s_active_sound != SOUND_ALARM) {
        s_player=(sound_player_t){0}; s_active_sound=-1;
    }
    if (s_active_sound<0 && !muted) {
        for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) {
            const sound_category_t category=priority[i];
            if (!(s_sound_pending&(1U<<category))) continue;
            s_sound_pending &= (uint8_t)~(1U<<category);
            if (sound_player_start(&s_player,category,s_sound_variants[category])) { s_active_sound=(int)category; break; }
        }
    }
    portEXIT_CRITICAL(&s_lock);
    if (s_active_sound<0 || s_speaker == NULL) return false;
    int16_t samples[DEVICE_CONTROL_SOUND_CHUNK_SAMPLES];
    const size_t count=sound_player_read(&s_player,samples,DEVICE_CONTROL_SOUND_CHUNK_SAMPLES);
    const esp_err_t result=count ? codec_result(esp_codec_dev_write(s_speaker,samples,(int)(count*sizeof(samples[0])))) : ESP_OK;
    if (result != ESP_OK) { ESP_LOGW(TAG,"Melody playback failed: %s",esp_err_to_name(result)); s_player=(sound_player_t){0}; }
    if (s_player.melody == NULL) { ESP_LOGI(TAG,"Melody complete category=%d result=%ld",s_active_sound,(long)result); s_active_sound=-1; }
    return s_active_sound>=0;
}

static bool play_music_chunk(void)
{
    music_audio_status_t music={0};music_audio_get_status(&music);
    if(!music.active || music.paused || !music.ready)return false;
    if(!s_status.audio_ready || !s_speaker) {
        music_audio_owner_fail(ESP_ERR_INVALID_STATE);return false;
    }
    int16_t samples[MUSIC_PCM_CHUNK]={0};
    const size_t count=music_audio_owner_read(samples,MUSIC_PCM_CHUNK);
    /* Preserve device pacing through a brief underrun. Only this owner
     * writes I2S, including silence; producer/decoder never touches hardware. */
    const size_t frames=count ? count : MUSIC_PCM_CHUNK;
    const esp_err_t result=codec_result(esp_codec_dev_write(s_speaker,samples,(int)(frames*sizeof(samples[0]))));
    if(result!=ESP_OK){music_audio_owner_fail(result);return false;}
    return true;
}

static void wake_audio_owner(void)
{ if(s_task)xTaskNotifyGive(s_task); }

static bool play_audio_chunk(void)
{ return play_pending_sound_chunk() || play_music_chunk(); }

static void finish_save_if_complete(void)
{
    if (!s_save_enqueued) return;
    flash_coordinator_status_t flash = {0};
    flash_coordinator_get_status(&flash);
    if (flash.device_control_profile_completed_sequence != s_pending_flash_sequence) return;

    portENTER_CRITICAL(&s_lock);
    s_save_enqueued = false;
    s_pending_flash_sequence = 0U;
    uint8_t completed_mask = 0U;
    const bool saved = flash.device_control_profile_last_write_result == ESP_OK &&
                       flash.device_control_profile_valid &&
                       flash.device_control_profile.brightness_percent ==
                           s_submitted_profile.brightness_percent &&
                       flash.device_control_profile.volume_percent ==
                           s_submitted_profile.volume_percent &&
                       flash.device_control_profile.night_mode_enabled ==
                           s_submitted_profile.night_mode_enabled &&
                       flash.device_control_profile.photo_config == s_submitted_profile.photo_config &&
                       flash.device_control_profile.photo_bitcoin_widget == s_submitted_profile.photo_bitcoin_widget &&
                       flash.device_control_profile.photo_clock_widget == s_submitted_profile.photo_clock_widget &&
                       flash.device_control_profile.photo_idle_choice == s_submitted_profile.photo_idle_choice &&
                       flash.device_control_profile.photo_interval_choice == s_submitted_profile.photo_interval_choice &&
                       flash.device_control_profile.photo_enabled_mask == s_submitted_profile.photo_enabled_mask &&
                       flash.device_control_profile.photo_exit_choice == s_submitted_profile.photo_exit_choice;
    if (saved) {
        if ((s_submitted_mask & DEVICE_CONTROL_BRIGHTNESS_MASK) != 0U &&
            s_status.brightness_percent == s_submitted_profile.brightness_percent) {
            s_dirty_mask &= (uint8_t)~DEVICE_CONTROL_BRIGHTNESS_MASK;
            completed_mask |= DEVICE_CONTROL_BRIGHTNESS_MASK;
        }
        if ((s_submitted_mask & DEVICE_CONTROL_VOLUME_MASK) != 0U &&
            s_status.volume_percent == s_submitted_profile.volume_percent) {
            s_dirty_mask &= (uint8_t)~DEVICE_CONTROL_VOLUME_MASK;
            completed_mask |= DEVICE_CONTROL_VOLUME_MASK;
        }
        if ((s_submitted_mask & DEVICE_CONTROL_PHOTO_MASK) != 0U &&
            s_status.photo_config == s_submitted_profile.photo_config &&
            s_status.photo_idle_choice == s_submitted_profile.photo_idle_choice &&
            s_status.photo_interval_choice == s_submitted_profile.photo_interval_choice &&
            s_status.photo_enabled_mask == s_submitted_profile.photo_enabled_mask &&
            s_status.photo_exit_choice == s_submitted_profile.photo_exit_choice) {
            s_dirty_mask &= (uint8_t)~DEVICE_CONTROL_PHOTO_MASK;
            completed_mask |= DEVICE_CONTROL_PHOTO_MASK;
        }
        if ((s_submitted_mask & DEVICE_CONTROL_WIDGET_MASK) != 0U &&
            s_status.photo_bitcoin_widget == s_submitted_profile.photo_bitcoin_widget &&
            s_status.photo_clock_widget == s_submitted_profile.photo_clock_widget) {
            s_dirty_mask &= (uint8_t)~DEVICE_CONTROL_WIDGET_MASK;
            completed_mask |= DEVICE_CONTROL_WIDGET_MASK;
        }
        s_status.save_result = ESP_OK;
        if ((s_submitted_mask & DEVICE_CONTROL_NIGHT_MASK) != 0U &&
            s_status.night_mode_enabled == (s_submitted_profile.night_mode_enabled != 0U)) {
            s_dirty_mask &= (uint8_t)~DEVICE_CONTROL_NIGHT_MASK;
            completed_mask |= DEVICE_CONTROL_NIGHT_MASK;
        }
    } else {
        completed_mask = s_submitted_mask;
        s_dirty_mask = 0U;
        s_status.save_result = flash.device_control_profile_last_write_result == ESP_OK
                                   ? ESP_FAIL : flash.device_control_profile_last_write_result;
    }
    s_status.persistence_pending = s_dirty_mask != 0U;
    if (completed_mask != 0U) {
        s_status.save_completion_mask = completed_mask;
        ++s_status.save_completion_id;
    }
    const uint8_t selected = photo_mask_count(s_status.photo_enabled_mask);
    const uint32_t idle_ms = photo_idle_choice_ms(s_status.photo_idle_choice);
    const uint32_t interval_ms = photo_interval_choice_ms(s_status.photo_interval_choice);
    const uint32_t exit_ms = photo_exit_choice_ms(s_status.photo_exit_choice);
    const esp_err_t save_result = s_status.save_result;
    portEXIT_CRITICAL(&s_lock);
    if (completed_mask & DEVICE_CONTROL_PHOTO_MASK)
        ESP_LOGI(TAG, "Photo preferences saved result=%ld selected=%u idle_ms=%lu interval_ms=%lu exit_ms=%lu",
            (long)save_result, selected, (unsigned long)idle_ms, (unsigned long)interval_ms, (unsigned long)exit_ms);
}

static void submit_dirty_profile(void)
{
    device_control_profile_t profile;
    uint8_t mask;
    portENTER_CRITICAL(&s_lock);
    if (s_save_enqueued || s_dirty_mask == 0U ||
        xTaskGetTickCount() - s_last_change_tick <
            pdMS_TO_TICKS(DEVICE_CONTROL_SAVE_DELAY_MS)) {
        portEXIT_CRITICAL(&s_lock);
        return;
    }
    profile = (device_control_profile_t){
        .brightness_percent = s_status.brightness_percent,
        .volume_percent = s_status.volume_percent,
        .night_mode_enabled = s_status.night_mode_enabled ? 1U : 0U,
        .photo_config = s_status.photo_config,
        .photo_bitcoin_widget = s_status.photo_bitcoin_widget,
        .photo_clock_widget = s_status.photo_clock_widget,
        .photo_idle_choice = s_status.photo_idle_choice,
        .photo_interval_choice = s_status.photo_interval_choice,
        .photo_enabled_mask = s_status.photo_enabled_mask,
        .photo_exit_choice = s_status.photo_exit_choice,
    };
    mask = s_dirty_mask;
    portEXIT_CRITICAL(&s_lock);

    uint32_t sequence = 0U;
    const esp_err_t result = flash_coordinator_request_device_control_profile_write(
        &profile, &sequence);
    if (result == ESP_ERR_INVALID_STATE || result == ESP_ERR_TIMEOUT) {
        /* Another flash owner is active; retry without dropping the change. */
        return;
    }
    portENTER_CRITICAL(&s_lock);
    if (result == ESP_OK) {
        s_save_enqueued = true;
        s_pending_flash_sequence = sequence;
        s_submitted_profile = profile;
        s_submitted_mask = mask;
    } else {
        s_dirty_mask = 0U;
        s_status.persistence_pending = false;
        s_status.save_result = result;
        s_status.save_completion_mask = mask;
        ++s_status.save_completion_id;
    }
    portEXIT_CRITICAL(&s_lock);
}

static void device_control_task(void *arg)
{
    (void)arg;
    const temperature_sensor_config_t sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    if (temperature_sensor_install(&sensor_config, &s_temperature_sensor) == ESP_OK &&
        temperature_sensor_enable(s_temperature_sensor) != ESP_OK) {
        (void)temperature_sensor_uninstall(s_temperature_sensor);
        s_temperature_sensor = NULL;
    }
    TickType_t last_temperature_tick = xTaskGetTickCount() - pdMS_TO_TICKS(5000U);
    TickType_t last_resources_tick = xTaskGetTickCount() - pdMS_TO_TICKS(2000U);
    uint32_t prior_cpu_time = 0U;
    uint32_t prior_idle[2] = {0};
    bool cpu_baseline = false;
    TickType_t last_resources_log = xTaskGetTickCount();
    bool restored = false;
    for (;;) {
        bool audio_active=false;
        if (!restored) {
            flash_coordinator_status_t flash = {0};
            flash_coordinator_get_status(&flash);
            if (flash.ready) {
                restore_preferences();
                restored = true;
            }
        } else {
            apply_pending_controls();
            apply_night_schedule();
            music_audio_owner_maintain();
            audio_active=play_audio_chunk();
            finish_save_if_complete();
            submit_dirty_profile();
        }
        if (s_temperature_sensor != NULL &&
            xTaskGetTickCount() - last_temperature_tick >= pdMS_TO_TICKS(5000U)) {
            last_temperature_tick = xTaskGetTickCount();
            float celsius = 0;
            const bool available = temperature_sensor_get_celsius(s_temperature_sensor, &celsius) == ESP_OK &&
                                   celsius >= -40.0f && celsius <= 125.0f;
            portENTER_CRITICAL(&s_lock);
            s_status.temperature_available = available;
            s_status.chip_temperature_deci_c = available
                ? (int16_t)(celsius * 10.0f + (celsius >= 0 ? 0.5f : -0.5f)) : 0;
            portEXIT_CRITICAL(&s_lock);
        }
        if (xTaskGetTickCount() - last_resources_tick >= pdMS_TO_TICKS(2000U)) {
            last_resources_tick = xTaskGetTickCount();
            system_resource_snapshot_t metrics = {0};
            const uint64_t now_us = (uint64_t)esp_timer_get_time();
            const uint32_t cpu_time = (uint32_t)now_us;
            metrics.uptime_s = (uint32_t)(now_us / 1000000U);
            metrics.internal_total_bytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            metrics.internal_free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            metrics.internal_largest_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            metrics.psram_total_bytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            metrics.psram_free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            metrics.available = metrics.internal_total_bytes != 0U;
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS && !CONFIG_FREERTOS_SMP
            metrics.cpu_available = cpu_baseline;
            for (uint8_t core = 0U; core < 2U; ++core) {
                const uint32_t idle = (uint32_t)ulTaskGetIdleRunTimeCounterForCore(core);
                if (!system_cpu_window(cpu_time, prior_cpu_time, idle, prior_idle[core],
                                       &metrics.cpu_core_percent[core])) metrics.cpu_available = false;
                prior_idle[core] = idle;
            }
            metrics.cpu_percent = (uint8_t)((metrics.cpu_core_percent[0] +
                                            metrics.cpu_core_percent[1] + 1U) / 2U);
#endif
            prior_cpu_time = cpu_time;
            cpu_baseline = true;
            portENTER_CRITICAL(&s_lock);
            metrics.sequence = s_status.resources.sequence + 1U;
            s_status.resources = metrics;
            portEXIT_CRITICAL(&s_lock);
            if (xTaskGetTickCount() - last_resources_log >= pdMS_TO_TICKS(30000U)) {
                last_resources_log = xTaskGetTickCount();
                /* The LVGL task holds every screen refresh, so its stack is the margin that matters. */
                const TaskHandle_t lvgl_task = xTaskGetHandle("lvgl");
                const unsigned lvgl_stack_min = lvgl_task != NULL
                    ? (unsigned)uxTaskGetStackHighWaterMark(lvgl_task) : 0U;
                ESP_LOGI(TAG, "resources cpu=%u%% cores=%u/%u internal_free=%lu largest=%lu psram_free=%lu uptime=%lus lvgl_stack_min=%u",
                    (unsigned)metrics.cpu_percent, (unsigned)metrics.cpu_core_percent[0],
                    (unsigned)metrics.cpu_core_percent[1], (unsigned long)metrics.internal_free_bytes,
                    (unsigned long)metrics.internal_largest_bytes, (unsigned long)metrics.psram_free_bytes,
                    (unsigned long)metrics.uptime_s, lvgl_stack_min);
            }
        }
        (void)ulTaskNotifyTake(pdTRUE, s_active_sound>=0 || audio_active ? 0U : pdMS_TO_TICKS(DEVICE_CONTROL_POLL_MS));
    }
}

esp_err_t device_control_service_start(void)
{
    portENTER_CRITICAL(&s_lock);
    if (s_started) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    portEXIT_CRITICAL(&s_lock);

    music_audio_bind_wake(wake_audio_owner);

    if (xTaskCreate(device_control_task, "device_controls",
                    DEVICE_CONTROL_STACK_BYTES, NULL,
                    DEVICE_CONTROL_PRIORITY, &s_task) != pdPASS) {
        portENTER_CRITICAL(&s_lock);
        s_started = false;
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t request_update(uint8_t percent, bool brightness)
{
    if (percent > 100U) return ESP_ERR_INVALID_ARG;

    portENTER_CRITICAL(&s_lock);
    if (!s_started || !s_status.ready || s_task == NULL) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (brightness ? (!s_brightness_pending &&
                      percent == s_status.brightness_percent)
                   : (!s_volume_pending &&
                      percent == s_status.volume_percent)) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_OK;
    }
    if (brightness) {
        s_requested_brightness = percent;
        s_brightness_pending = true;
    } else {
        s_requested_volume = percent;
        s_volume_pending = true;
    }
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);

    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t device_control_set_brightness(uint8_t percent)
{
    return request_update(percent, true);
}

esp_err_t device_control_set_volume(uint8_t percent)
{
    return request_update(percent, false);
}

esp_err_t device_control_set_photo_widgets(uint8_t bitcoin, uint8_t clock)
{
    if (!photo_widget_valid(bitcoin) || !photo_widget_valid(clock)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    if (!s_started || !s_status.ready || s_task == NULL) {
        portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE;
    }
    if (s_status.photo_bitcoin_widget != bitcoin || s_status.photo_clock_widget != clock) {
        s_status.photo_bitcoin_widget = bitcoin;
        s_status.photo_clock_widget = clock;
        s_dirty_mask |= DEVICE_CONTROL_WIDGET_MASK;
        s_status.persistence_pending = true;
        s_last_change_tick = xTaskGetTickCount();
    }
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t device_control_set_photo_settings(bool enabled, uint8_t idle, uint8_t interval, uint8_t exit_choice)
{
    if (idle >= PHOTO_IDLE_CHOICE_COUNT || interval >= PHOTO_INTERVAL_CHOICE_COUNT ||
        exit_choice >= PHOTO_EXIT_CHOICE_COUNT)
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    if (!s_status.ready) { portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE; }
    const uint8_t config = enabled ? PHOTO_FRAME_ENABLED : 0U;
    if (s_status.photo_config != config || s_status.photo_idle_choice != idle ||
        s_status.photo_interval_choice != interval || s_status.photo_exit_choice != exit_choice) {
        s_status.photo_config = config;
        s_status.photo_idle_choice = idle;
        s_status.photo_interval_choice = interval;
        s_status.photo_exit_choice = exit_choice;
        s_dirty_mask |= DEVICE_CONTROL_PHOTO_MASK;
        s_status.persistence_pending = true;
        s_last_change_tick = xTaskGetTickCount();
    }
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t device_control_set_photo_selection(uint64_t enabled_mask)
{
    portENTER_CRITICAL(&s_lock);
    if (!s_status.ready) { portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE; }
    if (s_status.photo_enabled_mask != enabled_mask) {
        s_status.photo_enabled_mask = enabled_mask;
        s_dirty_mask |= DEVICE_CONTROL_PHOTO_MASK;
        s_status.persistence_pending = true;
        s_last_change_tick = xTaskGetTickCount();
    }
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t device_control_set_photo_config(uint8_t config)
{
    if (!photo_config_valid(config)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    if (!s_started || !s_status.ready || s_task == NULL) {
        portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE;
    }
    if (s_status.photo_config != config) {
        s_status.photo_config = config;
        s_dirty_mask |= DEVICE_CONTROL_PHOTO_MASK;
        s_status.persistence_pending = true;
        s_last_change_tick = xTaskGetTickCount();
    }
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t device_control_set_night_mode(bool enabled)
{
    portENTER_CRITICAL(&s_lock);
    if (!s_started || !s_status.ready || s_task == NULL) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_status.night_mode_enabled != enabled) {
        s_status.night_mode_enabled = enabled;
        s_dirty_mask |= DEVICE_CONTROL_NIGHT_MASK;
        s_status.persistence_pending = true;
        s_last_change_tick = xTaskGetTickCount();
    }
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t device_control_play_melody(sound_category_t category,uint8_t variant)
{
    if (!sound_choice_valid(category,variant)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    const TickType_t now=xTaskGetTickCount();
    const uint8_t volume=s_volume_pending ? s_requested_volume : s_status.volume_percent;
    if (!s_started || !s_status.ready || !s_status.audio_ready || s_task == NULL ||
        (variant != 0U && (volume == 0U || (s_sound_queued[category] &&
         now-s_sound_ticks[category]<pdMS_TO_TICKS(DEVICE_CONTROL_NOTIFICATION_TONE_MIN_INTERVAL_MS))))) {
        portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE;
    }
    if (variant == 0U) { s_sound_cancel |= (uint8_t)(1U<<category); s_sound_pending &= (uint8_t)~(1U<<category); }
    else { s_sound_variants[category]=variant; s_sound_pending |= (uint8_t)(1U<<category); s_sound_ticks[category]=now;s_sound_queued[category]=true; }
    const TaskHandle_t task=s_task;
    portEXIT_CRITICAL(&s_lock);
    xTaskNotifyGive(task);return ESP_OK;
}
void device_control_stop_sounds(void)
{
    portENTER_CRITICAL(&s_lock);
    s_sound_cancel=(1U<<SOUND_CATEGORY_COUNT)-1U; s_sound_pending=0U;
    const TaskHandle_t task=s_task;
    portEXIT_CRITICAL(&s_lock);
    if (task != NULL) xTaskNotifyGive(task);
}
esp_err_t device_control_play_notification_tone(void) { return device_control_play_melody(SOUND_NOTIFICATION,1U); }
esp_err_t device_control_play_pomodoro_alarm(void) { return device_control_play_melody(SOUND_ALARM,1U); }

void device_control_get_status(device_control_status_t *out_status)
{
    if (out_status == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    out_status->persistence_pending = s_status.persistence_pending ||
                                      s_brightness_pending || s_volume_pending;
    portEXIT_CRITICAL(&s_lock);
}
