#pragma once
#include "esp_codec_dev.h"
#include "esp_err.h"
#include <stddef.h>

/* device_controls only. Same codec path and format, bounded RX wait. */
/* ESP_ERR_TIMEOUT with zero bytes is transient. A timeout with an even PCM16
 * prefix returns ESP_OK and its actual byte count; never read an unwritten tail. */
esp_err_t audio_capture_io_read(esp_codec_dev_handle_t codec, void *pcm, size_t capacity, size_t *received);
