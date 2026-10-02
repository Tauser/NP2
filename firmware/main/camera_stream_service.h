#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_STREAM_MAX_WIDTH 640U
#define CAMERA_STREAM_MAX_HEIGHT 360U
#define CAMERA_STREAM_RGB565_BYTES \
    (CAMERA_STREAM_MAX_WIDTH * CAMERA_STREAM_MAX_HEIGHT * 2U)

typedef enum {
    CAMERA_STREAM_STOPPED = 0,
    CAMERA_STREAM_WAITING_CREDENTIALS,
    CAMERA_STREAM_CONNECTING,
    CAMERA_STREAM_AUTHENTICATING,
    CAMERA_STREAM_PLAYING,
    CAMERA_STREAM_ERROR,
} camera_stream_state_t;

typedef struct {
    camera_stream_state_t state;
    esp_err_t last_error;
    uint16_t last_rtsp_status;
    bool decoder_unsupported_profile;
    uint16_t width;
    uint16_t height;
    uint32_t frame_sequence;
} camera_stream_status_t;

esp_err_t camera_stream_service_start(void);
esp_err_t camera_stream_service_play(const char *ipv4, const char *username,
                                     const char *password);
esp_err_t camera_stream_service_stop(void);
void camera_stream_service_get_status(camera_stream_status_t *out_status);
esp_err_t camera_stream_service_copy_frame(uint8_t *dst, size_t capacity,
                                           uint16_t *width, uint16_t *height,
                                           uint32_t *sequence);

#ifdef __cplusplus
}
#endif
