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
/* The PPA scales 640x360 in steps of 1/16: at most 21/16, which is 840x472. A scaled row is padded to the
 * 64 bytes LVGL requires (864 pixels), otherwise LVGL copies every picture into a pool too small for it. */
#define CAMERA_STREAM_OUT_MAX_WIDTH 840U
#define CAMERA_STREAM_OUT_MAX_HEIGHT 472U
#define CAMERA_STREAM_OUT_MAX_PITCH 864U
#define CAMERA_STREAM_FRAME_BYTES (CAMERA_STREAM_OUT_MAX_PITCH * CAMERA_STREAM_OUT_MAX_HEIGHT * 2U)
/* Slack after each frame buffer: the PPA works in 32 x 32 tiles and 472 rows is not a whole number of them. */
#define CAMERA_STREAM_FRAME_GUARD_BYTES 32768U
#define CAMERA_STREAM_SCALE_MIN 8U   /* Sixteenths. */
#define CAMERA_STREAM_SCALE_MAX 21U

typedef enum {
    CAMERA_STREAM_STOPPED = 0,
    CAMERA_STREAM_WAITING_CREDENTIALS,
    CAMERA_STREAM_CONNECTING,
    CAMERA_STREAM_AUTHENTICATING,
    CAMERA_STREAM_PLAYING,
    CAMERA_STREAM_ERROR,
} camera_stream_state_t;

/* Outcome of registering a camera: the address and account are checked against the real
 * RTSP server before anything is saved. */
typedef enum {
    CAMERA_REGISTER_IDLE = 0,
    CAMERA_REGISTER_CHECKING,
    CAMERA_REGISTER_OK,
    CAMERA_REGISTER_AUTH_REJECTED,
    CAMERA_REGISTER_UNREACHABLE,
    CAMERA_REGISTER_STREAM_REFUSED,
    CAMERA_REGISTER_LIST_FULL,
    CAMERA_REGISTER_SAVE_FAILED,
} camera_register_state_t;

/* Result of asking a camera for its stream without any account, to learn whether one is needed. */
typedef enum {
    CAMERA_PROBE_IDLE = 0,
    CAMERA_PROBE_CHECKING,
    CAMERA_PROBE_OPEN,          /* Streams without an account. */
    CAMERA_PROBE_AUTH_REQUIRED, /* Reachable, and it asks for the camera account. */
    CAMERA_PROBE_UNREACHABLE,
    CAMERA_PROBE_NOT_A_CAMERA,  /* Answers, but offers no RTSP stream at the expected path. */
} camera_probe_state_t;

typedef struct {
    camera_stream_state_t state;
    camera_register_state_t register_state;
    char register_ipv4[16];
    camera_probe_state_t probe_state;
    char probe_ipv4[16];
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
/* Plays with the account saved for this address. ESP_ERR_NOT_FOUND when there is none:
 * the caller asks for it, and a typed account is saved once the camera accepts it. */
esp_err_t camera_stream_service_play_saved(const char *ipv4);
/* Asks the camera for its stream with no account: the result in probe_state says whether the user
 * has to provide one. ESP_ERR_INVALID_ARG for anything but a private IPv4 address. */
esp_err_t camera_stream_service_probe(const char *ipv4);
/* Opens an RTSP session without decoding to prove the address and account work. On success the
 * account and the camera are saved; the result is in register_state. Empty username and password
 * register a camera that needs no account (after a probe said so). */
esp_err_t camera_stream_service_register(const char *ipv4, const char *username,
                                         const char *password);
esp_err_t camera_stream_service_stop(void);
void camera_stream_service_get_status(camera_stream_status_t *out_status);
/* The picture is published already scaled, at the size set here (sixteenths of the camera's 640x360). The
 * viewer that is on screen asks for the size that fits it exactly so LVGL never has to scale. */
esp_err_t camera_stream_service_set_output_scale(uint8_t sixteenths);
/* Copies the newest picture. stride_bytes is the distance between rows, at least width * 2. */
esp_err_t camera_stream_service_copy_frame(uint8_t *dst, size_t capacity,
                                           uint16_t *width, uint16_t *height,
                                           uint16_t *stride_bytes, uint32_t *sequence);

#ifdef __cplusplus
}
#endif
