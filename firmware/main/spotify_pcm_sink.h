#pragma once
#include "music_pcm.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SPOTIFY_PCM_ID_BYTES 65U
#define SPOTIFY_PCM_MARKERS 4U
typedef struct { char id[SPOTIFY_PCM_ID_BYTES]; uint32_t sample; } spotify_pcm_marker_t;
typedef struct {
    music_pcm_converter_t converter;
    int16_t pending[MUSIC_PCM_CHUNK];
    size_t pending_count, pending_offset, pending_ack;
    uint32_t generation, queued;
    char id[SPOTIFY_PCM_ID_BYTES];
    spotify_pcm_marker_t markers[SPOTIFY_PCM_MARKERS];
    uint8_t marker_count;
    bool failed;
} spotify_pcm_sink_t;
typedef size_t (*spotify_pcm_push_t)(void *,uint32_t,const int16_t *,size_t);
/* Caller serializes the producer and marker consumer. Conversion advances
 * once; a retry acknowledges the same input only after pending PCM is queued. */
void spotify_pcm_sink_reset(spotify_pcm_sink_t *,uint32_t generation);
size_t spotify_pcm_sink_write(spotify_pcm_sink_t *,const char *id,size_t id_size,
    const uint8_t *pcm,size_t bytes,spotify_pcm_push_t,void *);
bool spotify_pcm_sink_reached(spotify_pcm_sink_t *,uint32_t consumed,char id[SPOTIFY_PCM_ID_BYTES]);
#ifdef __cplusplus
}
#endif
