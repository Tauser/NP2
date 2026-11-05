#include "voice_resampler.h"
#include <math.h>
#include <string.h>

#define HALF (VOICE_RESAMPLER_TAPS / 2U)
#define PI_F 3.14159265358979f

static float sinc(float x) { return fabsf(x) < 1e-6f ? 1.0f : sinf(PI_F * x) / (PI_F * x); }

bool voice_resampler_init(voice_resampler_t *r, uint32_t in_rate, uint32_t out_rate)
{
    if (!r || in_rate < 8000U || in_rate > 48000U || out_rate < 8000U || out_rate > 48000U) return false;
    memset(r, 0, sizeof(*r));
    r->in_rate = in_rate; r->out_rate = out_rate;
    r->step_q32 = ((uint64_t)in_rate << 32U) / out_rate;
    /* Cut below the lower Nyquist so decimation does not alias speech. */
    const float cutoff = (in_rate > out_rate ? (float)out_rate / (float)in_rate : 1.0f) * 0.92f;
    for (unsigned p = 0U; p <= VOICE_RESAMPLER_PHASES; ++p) {
        const float frac = (float)p / (float)VOICE_RESAMPLER_PHASES;
        float sum = 0.0f;
        for (unsigned k = 0U; k < VOICE_RESAMPLER_TAPS; ++k) {
            const float x = (float)k - (float)(HALF - 1U) - frac;
            const float w = 0.42f + 0.5f * cosf(PI_F * x / (float)HALF) + 0.08f * cosf(2.0f * PI_F * x / (float)HALF);
            const float h = fabsf(x) >= (float)HALF ? 0.0f : cutoff * sinc(cutoff * x) * w;
            r->table[p][k] = h; sum += h;
        }
        for (unsigned k = 0U; k < VOICE_RESAMPLER_TAPS; ++k) r->table[p][k] /= sum;
    }
    return true;
}

void voice_resampler_reset(voice_resampler_t *r)
{
    if (!r) return;
    memset(r->ring, 0, sizeof(r->ring));
    r->received = 0U; r->next_q32 = 0U;
}

size_t voice_resampler_max_output(const voice_resampler_t *r, size_t count)
{
    return r ? (size_t)(((uint64_t)count * r->out_rate) / r->in_rate) + 2U : 0U;
}

size_t voice_resampler_process(voice_resampler_t *r, const int16_t *in, size_t count,
                               int16_t *out, size_t capacity)
{
    if (!r || !r->step_q32 || (!in && count)) return 0U;
    size_t produced = 0U;
    for (size_t i = 0U; i < count; ++i) {
        r->ring[r->received % VOICE_RESAMPLER_TAPS] = in[i];
        ++r->received;
        /* Emit every output whose window ends at the newest sample. */
        while ((r->next_q32 >> 32U) + HALF + 1U <= r->received) {
            const uint64_t base = r->next_q32 >> 32U;
            const uint32_t phase = (uint32_t)((((r->next_q32 & 0xFFFFFFFFULL) * VOICE_RESAMPLER_PHASES) + 0x80000000ULL) >> 32U);
            const float *h = r->table[phase];
            float acc = 0.0f;
            for (unsigned k = 0U; k < VOICE_RESAMPLER_TAPS; ++k) {
                /* Indices before the stream start read the zeroed ring. */
                const uint64_t index = base + k - (HALF - 1U);
                acc += h[k] * (float)r->ring[index % VOICE_RESAMPLER_TAPS];
            }
            const int32_t sample = (int32_t)lrintf(acc);
            if (produced < capacity && out)
                out[produced++] = (int16_t)(sample > 32767 ? 32767 : sample < -32768 ? -32768 : sample);
            r->next_q32 += r->step_q32;
        }
    }
    return produced;
}
