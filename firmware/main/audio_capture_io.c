#include "audio_capture_io.h"
#ifdef NP2_AUDIO_CAPTURE_HOST_TEST
#include "audio_capture_io_host_platform.h"
#else
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include <stdatomic.h>

/* The pinned codec's public read API has no timeout parameter. Link wrapping
 * limits only the synchronous read issued by this adapter, in its owner task.
 * All other callers retain their original timeout, return value and buffers.
 * No dependency source or I2S configuration is changed (ADR-117). */
static _Atomic(TaskHandle_t) s_reader;
/* Written only by the synchronous owner invocation of the wrapped I2S read.
 * Keep the driver's result/count, which esp_codec_dev otherwise flattens to
 * ESP_FAIL and discards on a partial timeout. */
static esp_err_t s_read_result;
static size_t s_read_bytes;
esp_err_t __real_i2s_channel_read(i2s_chan_handle_t, void *, size_t, size_t *, uint32_t);
esp_err_t __wrap_i2s_channel_read(i2s_chan_handle_t channel, void *data, size_t bytes,
                                 size_t *received, uint32_t timeout_ms)
{
    const bool scoped = atomic_load(&s_reader) == xTaskGetCurrentTaskHandle();
    if (scoped && timeout_ms > 20U) timeout_ms = 20U;
    const esp_err_t result = __real_i2s_channel_read(channel, data, bytes, received, timeout_ms);
    if (scoped) { s_read_result = result; s_read_bytes = received ? *received : 0U; }
    return result;
}

esp_err_t audio_capture_io_read(esp_codec_dev_handle_t codec, void *pcm, size_t capacity, size_t *received)
{
    if (!codec || !pcm || !received || !capacity || capacity % sizeof(int16_t)) return ESP_ERR_INVALID_ARG;
    *received = 0U;
    s_read_result = ESP_FAIL; s_read_bytes = 0U;
    atomic_store(&s_reader, xTaskGetCurrentTaskHandle());
    const int result = esp_codec_dev_read(codec, pcm, (int)capacity);
    atomic_store(&s_reader, NULL);
    if (s_read_bytes > capacity || s_read_bytes % sizeof(int16_t)) return ESP_ERR_INVALID_SIZE;
    if (s_read_result == ESP_ERR_TIMEOUT || s_read_result == ESP_OK) {
        *received = s_read_bytes;
        return s_read_bytes ? ESP_OK : ESP_ERR_TIMEOUT;
    }
    return result == ESP_CODEC_DEV_OK ? ESP_OK : (esp_err_t)result;
}
