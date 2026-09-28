#include "device_control_service.h"

#include "flash_coordinator.h"
#include "night_mode_policy.h"
#include "time_service.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
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
#define DEVICE_CONTROL_NOTIFICATION_TONE_SAMPLES 768U
#define DEVICE_CONTROL_NOTIFICATION_TONE_MIN_INTERVAL_MS 350U

static const char *const TAG = "device_controls";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static esp_codec_dev_handle_t s_speaker;
static temperature_sensor_handle_t s_temperature_sensor;
static bool s_started;
static bool s_brightness_pending;
static bool s_volume_pending;
static bool s_notification_tone_pending;
static uint8_t s_requested_brightness;
static uint8_t s_requested_volume;
static uint8_t s_dirty_mask;
static bool s_save_enqueued;
static uint32_t s_pending_flash_sequence;
static uint8_t s_submitted_mask;
static device_control_profile_t s_submitted_profile;
static TickType_t s_last_change_tick;
static TickType_t s_last_notification_tone_tick;
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
    memcpy(s_status.firmware_version, version, sizeof(version));
    s_status.brightness_result = brightness_result;
    s_status.volume_result = volume_result;
    s_status.audio_ready = volume_result == ESP_OK;
    s_status.ready = true;
    portEXIT_CRITICAL(&s_lock);
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

static void fill_notification_tone(int16_t *samples, uint8_t phase_step)
{
    uint8_t phase = 0U;
    for (uint32_t i = 0U; i < DEVICE_CONTROL_NOTIFICATION_TONE_SAMPLES; ++i) {
        const int32_t triangle =
            ((phase < 50U ? (int32_t)phase : 100 - (int32_t)phase) * 2) - 50;
        const uint32_t remaining = DEVICE_CONTROL_NOTIFICATION_TONE_SAMPLES - 1U - i;
        const int32_t envelope = i < 40U ? (int32_t)i :
                                 remaining < 40U ? (int32_t)remaining : 40;
        samples[i] = (int16_t)((triangle * 360 * envelope) / 40);
        phase = (uint8_t)((phase + phase_step) % 100U);
    }
}

static void play_pending_notification_tone(void)
{
    bool pending = false;
    portENTER_CRITICAL(&s_lock);
    pending = s_notification_tone_pending;
    s_notification_tone_pending = false;
    portEXIT_CRITICAL(&s_lock);
    if (!pending || s_speaker == NULL) return;

    int16_t samples[DEVICE_CONTROL_NOTIFICATION_TONE_SAMPLES];
    const uint8_t phase_steps[] = {3U, 4U};
    for (uint32_t i = 0U; i < sizeof(phase_steps) / sizeof(phase_steps[0]); ++i) {
        fill_notification_tone(samples, phase_steps[i]);
        const esp_err_t result = codec_result(esp_codec_dev_write(
            s_speaker, (void *)samples, sizeof(samples)));
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "Notification tone unavailable: %s", esp_err_to_name(result));
            return;
        }
    }
}

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
                           s_submitted_profile.night_mode_enabled;
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
    portEXIT_CRITICAL(&s_lock);
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
    bool restored = false;
    for (;;) {
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
            play_pending_notification_tone();
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
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(DEVICE_CONTROL_POLL_MS));
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

esp_err_t device_control_play_notification_tone(void)
{
    portENTER_CRITICAL(&s_lock);
    const TickType_t now = xTaskGetTickCount();
    const uint8_t effective_volume =
        s_volume_pending ? s_requested_volume : s_status.volume_percent;
    if (!s_started || !s_status.ready || !s_status.audio_ready || s_task == NULL ||
        effective_volume == 0U ||
        now - s_last_notification_tone_tick <
            pdMS_TO_TICKS(DEVICE_CONTROL_NOTIFICATION_TONE_MIN_INTERVAL_MS)) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_notification_tone_pending = true;
    s_last_notification_tone_tick = now;
    const TaskHandle_t task = s_task;
    portEXIT_CRITICAL(&s_lock);

    xTaskNotifyGive(task);
    return ESP_OK;
}

void device_control_get_status(device_control_status_t *out_status)
{
    if (out_status == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    out_status->persistence_pending = s_status.persistence_pending ||
                                      s_brightness_pending || s_volume_pending;
    portEXIT_CRITICAL(&s_lock);
}
