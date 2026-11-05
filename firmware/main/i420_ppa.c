#include "i420_ppa.h"

#include <math.h>

#include "driver/ppa.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static ppa_client_handle_t s_client;
static SemaphoreHandle_t s_done;
static volatile bool s_in_flight;

/* Runs in the PPA interrupt. */
static bool on_transaction_done(ppa_client_handle_t client, ppa_event_data_t *event, void *user_data)
{
    (void)client;
    (void)event;
    (void)user_data;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

size_t i420_ppa_align(size_t bytes)
{
    return (bytes + I420_PPA_ALIGN - 1U) & ~((size_t)I420_PPA_ALIGN - 1U);
}

size_t i420_ppa_packed_bytes(uint16_t width, uint16_t height)
{
    return i420_ppa_align((size_t)width * height * 3U / 2U);
}

uint16_t i420_ppa_row_pitch(uint16_t width)
{
    const uint32_t stride_bytes = ((uint32_t)width * 2U + 63U) & ~63U;
    return (uint16_t)(stride_bytes / 2U);
}

void i420_ppa_output_size(uint16_t width, uint16_t height, float scale, uint16_t *out_w, uint16_t *out_h)
{
    /* The scale is applied in steps of 1/16. */
    const float step = floorf(scale * 16.0f) / 16.0f;
    if (out_w != NULL) *out_w = (uint16_t)floorf((float)width * step);
    if (out_h != NULL) *out_h = (uint16_t)floorf((float)height * step);
}

esp_err_t i420_ppa_init(void)
{
    if (s_client != NULL) return ESP_OK;
    s_done = xSemaphoreCreateBinary();
    if (s_done == NULL) return ESP_ERR_NO_MEM;
    const ppa_client_config_t config = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
        .data_burst_length = PPA_DATA_BURST_LENGTH_128,
    };
    esp_err_t result = ppa_register_client(&config, &s_client);
    if (result == ESP_OK) {
        const ppa_event_callbacks_t callbacks = {.on_trans_done = on_transaction_done};
        result = ppa_client_register_event_callbacks(s_client, &callbacks);
        if (result != ESP_OK) {
            (void)ppa_unregister_client(s_client);
            s_client = NULL;
        }
    }
    if (result != ESP_OK) {
        vSemaphoreDelete(s_done);
        s_done = NULL;
    }
    return result;
}

bool i420_ppa_ready(void)
{
    return s_client != NULL;
}

esp_err_t i420_ppa_start(const uint8_t *packed, uint16_t width, uint16_t height, float scale,
                         uint16_t *out, size_t out_bytes, uint16_t pitch_pixels)
{
    if (s_client == NULL || s_in_flight) return ESP_ERR_INVALID_STATE;
    if (packed == NULL || out == NULL || (width & 1U) != 0U || (height & 1U) != 0U) return ESP_ERR_INVALID_ARG;
    uint16_t out_w = 0U, out_h = 0U;
    i420_ppa_output_size(width, height, scale, &out_w, &out_h);
    if (out_w == 0U || out_h == 0U || pitch_pixels < out_w ||
        (size_t)pitch_pixels * out_h * 2U > out_bytes) return ESP_ERR_INVALID_SIZE;
    const float step = floorf(scale * 16.0f) / 16.0f;
    const ppa_srm_oper_config_t config = {
        .in = {
            .buffer = packed,
            .pic_w = width,
            .pic_h = height,
            .block_w = width,
            .block_h = height,
            .srm_cm = PPA_SRM_COLOR_MODE_YUV420,
            .yuv_range = COLOR_RANGE_LIMIT,
            .yuv_std = COLOR_CONV_STD_RGB_YUV_BT601,
        },
        .out = {
            .buffer = out,
            .buffer_size = (uint32_t)out_bytes,
            .pic_w = pitch_pixels,
            .pic_h = out_h,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = step,
        .scale_y = step,
        .mode = PPA_TRANS_MODE_NON_BLOCKING,
    };
    (void)xSemaphoreTake(s_done, 0); /* Forget a completion nobody collected. */
    s_in_flight = true;
    const esp_err_t result = ppa_do_scale_rotate_mirror(s_client, &config);
    if (result != ESP_OK) s_in_flight = false;
    return result;
}

esp_err_t i420_ppa_wait(uint32_t timeout_ms)
{
    if (s_client == NULL || !s_in_flight) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return ESP_ERR_TIMEOUT;
    s_in_flight = false;
    return ESP_OK;
}

esp_err_t i420_ppa_convert(const uint8_t *packed, uint16_t width, uint16_t height, float scale,
                           uint16_t *out, size_t out_bytes, uint16_t pitch_pixels)
{
    const esp_err_t started = i420_ppa_start(packed, width, height, scale, out, out_bytes, pitch_pixels);
    return started != ESP_OK ? started : i420_ppa_wait(1000U);
}
