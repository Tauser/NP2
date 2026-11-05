#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Microphone capture for the voice assistant. device_control's owner task is
 * the only ES7210/I2S reader; consumers receive mono PCM16 at 22.05 kHz from
 * a bounded PSRAM ring (four seconds, enough to cover the WebSocket setup that
 * follows a tap). Overflow drops the oldest samples. */
#define VOICE_CAPTURE_SAMPLE_RATE 22050U
#define VOICE_CAPTURE_RING_SAMPLES (4U * 22050U)
#define VOICE_CAPTURE_CHUNK 256U

typedef struct {
    bool active, ready;
    uint32_t generation, buffered_samples, dropped_samples, captured_samples;
    esp_err_t result;
} voice_capture_status_t;

/* Requests only; no codec I/O. Begin resets the ring and returns a generation. */
uint32_t voice_capture_begin(void);
void voice_capture_stop(void);
size_t voice_capture_read(uint32_t generation, int16_t *out, size_t capacity);
void voice_capture_get_status(voice_capture_status_t *);

/* device_control owner task only. */
void voice_capture_bind_wake(void (*wake)(void));
void voice_capture_owner_maintain(void);
bool voice_capture_owner_wanted(void);
void voice_capture_owner_push(uint32_t generation, const int16_t *pcm, size_t count);
void voice_capture_owner_fail(esp_err_t result);
#ifdef __cplusplus
}
#endif
