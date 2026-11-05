#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Streaming band-limited mono PCM16 resampler (windowed sinc, 16 taps,
 * 64 phases). Plain state, no allocation; one producer per instance. */
#define VOICE_RESAMPLER_TAPS 16U
#define VOICE_RESAMPLER_PHASES 64U
typedef struct {
    float table[VOICE_RESAMPLER_PHASES + 1U][VOICE_RESAMPLER_TAPS];
    int16_t ring[VOICE_RESAMPLER_TAPS];
    uint64_t received, next_q32, step_q32;
    uint32_t in_rate, out_rate;
} voice_resampler_t;

bool voice_resampler_init(voice_resampler_t *, uint32_t in_rate, uint32_t out_rate);
void voice_resampler_reset(voice_resampler_t *);
/* Consumes every input sample; returns produced samples. `capacity` must be
 * at least voice_resampler_max_output(count); excess output is dropped. */
size_t voice_resampler_process(voice_resampler_t *, const int16_t *in, size_t count,
                               int16_t *out, size_t capacity);
size_t voice_resampler_max_output(const voice_resampler_t *, size_t count);
#ifdef __cplusplus
}
#endif
