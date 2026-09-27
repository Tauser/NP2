#include "device_control_service.h"

#include "bsp/display.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEVICE_CONTROL_DEFAULT_BRIGHTNESS 60U
#define DEVICE_CONTROL_DEFAULT_VOLUME     65U
#define DEVICE_CONTROL_STACK_BYTES        4096U
#define DEVICE_CONTROL_PRIORITY           2U

static const char *const TAG = "device_controls";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_task;
static esp_codec_dev_handle_t s_speaker;
static bool s_started;
static bool s_brightness_pending;
static bool s_volume_pending;
static device_control_status_t s_status = {
    .brightness_percent = DEVICE_CONTROL_DEFAULT_BRIGHTNESS,
    .volume_percent = DEVICE_CONTROL_DEFAULT_VOLUME,
    .brightness_result = ESP_OK,
    .volume_result = ESP_OK,
};

static esp_err_t codec_result(int result)
{
    return result == ESP_CODEC_DEV_OK ? ESP_OK : (esp_err_t)result;
}

static void apply_brightness(uint8_t percent)
{
    const esp_err_t result = bsp_display_brightness_set(percent);

    portENTER_CRITICAL(&s_lock);
    s_status.brightness_result = result;
    portEXIT_CRITICAL(&s_lock);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Brightness update failed: %s", esp_err_to_name(result));
    }
}

static void apply_volume(uint8_t percent)
{
    esp_err_t result = ESP_OK;

    if (s_speaker == NULL) {
        s_speaker = bsp_audio_codec_speaker_init();
        if (s_speaker == NULL) {
            result = ESP_FAIL;
        } else {
            const esp_codec_dev_sample_info_t format = {
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

    portENTER_CRITICAL(&s_lock);
    s_status.audio_ready = result == ESP_OK;
    s_status.volume_result = result;
    portEXIT_CRITICAL(&s_lock);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Volume update failed: %s", esp_err_to_name(result));
    }
}

static void device_control_task(void *arg)
{
    (void)arg;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint8_t brightness = 0U;
        uint8_t volume = 0U;
        bool update_brightness = false;
        bool update_volume = false;

        portENTER_CRITICAL(&s_lock);
        update_brightness = s_brightness_pending;
        update_volume = s_volume_pending;
        brightness = s_status.brightness_percent;
        volume = s_status.volume_percent;
        s_brightness_pending = false;
        s_volume_pending = false;
        portEXIT_CRITICAL(&s_lock);

        if (update_brightness) {
            apply_brightness(brightness);
        }
        if (update_volume) {
            apply_volume(volume);
        }
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
    if (percent > 100U) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_lock);
    if (!s_started || s_task == NULL) {
        portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (brightness) {
        s_status.brightness_percent = percent;
        s_brightness_pending = true;
    } else {
        s_status.volume_percent = percent;
        s_volume_pending = true;
    }
    TaskHandle_t task = s_task;
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

void device_control_get_status(device_control_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_lock);
}
