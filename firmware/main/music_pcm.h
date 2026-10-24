#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MUSIC_PCM_CHUNK 256U
#define MUSIC_PCM_QUEUE_SAMPLES (128U * 1024U)
#define MUSIC_PCM_FIR_TAPS 127U
typedef struct {
    int16_t history[MUSIC_PCM_FIR_TAPS];
    uint16_t next;
    uint8_t phase;
} music_pcm_converter_t;
/* PCM16 little-endian 44.1 kHz stereo -> 22.05 kHz mono. Returns consumed
 * bytes, reports produced samples; state belongs to exactly one producer. */
size_t music_pcm_convert(music_pcm_converter_t *, const uint8_t *, size_t bytes,
    int16_t *out, size_t capacity, size_t *produced);

typedef struct {
    int16_t *samples; size_t capacity, head, count;
    uint32_t generation;
    uint8_t gain;
    bool active, paused;
} music_pcm_queue_t;
/* Plain domain policy; callers serialize access. Storage is borrowed. */
uint32_t music_pcm_begin(music_pcm_queue_t *);
void music_pcm_stop(music_pcm_queue_t *);
size_t music_pcm_push(music_pcm_queue_t *, uint32_t generation, const int16_t *, size_t count);
size_t music_pcm_read(music_pcm_queue_t *, int16_t *, size_t capacity);
#ifdef __cplusplus
}
#endif
