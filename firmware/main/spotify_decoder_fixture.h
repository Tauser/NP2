#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t pcm_bytes, mono_samples, peak_codec_bytes, peak_blocks, peak_sample, zero_crossings;
} spotify_decoder_proof_t;
/* Decode-only gate. Never writes the codec, network, SD or flash. */
bool spotify_decoder_fixture(spotify_decoder_proof_t *);
#ifdef __cplusplus
}
#endif
