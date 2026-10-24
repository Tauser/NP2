#pragma once
#include "music_pcm.h"
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    bool active, paused, ready;
    uint32_t generation, buffered_samples;
    uint8_t gain;
    esp_err_t result;
} music_audio_status_t;
/* Requests only. No codec/storage I/O. Begin/stop invalidate old producers. */
uint32_t music_audio_begin(void);
void music_audio_stop(void);
void music_audio_pause(bool paused);
esp_err_t music_audio_set_gain(uint8_t percent);
size_t music_audio_push(uint32_t generation, const int16_t *, size_t count);
void music_audio_get_status(music_audio_status_t *);
/* Only device_control's owner task may call these. Maintenance allocates or
 * frees the bounded PSRAM queue, read returns already decoded mono samples. */
void music_audio_owner_maintain(void);
size_t music_audio_owner_read(int16_t *, size_t capacity);
void music_audio_owner_fail(esp_err_t);
/* Bind once at owner startup, before any producer starts. */
void music_audio_bind_wake(void (*wake)(void));
#ifdef __cplusplus
}
#endif
