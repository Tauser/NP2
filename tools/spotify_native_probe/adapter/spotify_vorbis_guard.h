#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ivorbisfile.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SPOTIFY_VORBIS_BYTES_MAX (2U * 1024U * 1024U)
#define SPOTIFY_VORBIS_BLOCKS_MAX 2048U
#define SPOTIFY_VORBIS_RESOURCE (-10000)
typedef struct spotify_vorbis_guard spotify_vorbis_guard_t;
typedef struct { size_t bytes, peak_bytes, blocks, peak_blocks, attempts, failures; } spotify_vorbis_stats_t;
spotify_vorbis_guard_t *spotify_vorbis_create(size_t byte_limit);
void spotify_vorbis_destroy(spotify_vorbis_guard_t *);
int spotify_vorbis_open(spotify_vorbis_guard_t *, void *, OggVorbis_File *, ov_callbacks);
long spotify_vorbis_read(spotify_vorbis_guard_t *, OggVorbis_File *, char *, int, int *section);
int spotify_vorbis_seek(spotify_vorbis_guard_t *, OggVorbis_File *, int64_t milliseconds);
void spotify_vorbis_close(spotify_vorbis_guard_t *, OggVorbis_File *);
void spotify_vorbis_stats(const spotify_vorbis_guard_t *, spotify_vorbis_stats_t *);
#ifdef NP_SPOTIFY_VORBIS_TEST
void spotify_vorbis_fail_at(spotify_vorbis_guard_t *, size_t attempt);
#endif
/* Private Tremor allocation hooks. Only valid inside a protected C call. */
void *spotify_ogg_malloc(size_t);
void *spotify_ogg_calloc(size_t,size_t);
void *spotify_ogg_realloc(void *,size_t);
void spotify_ogg_free(void *);
#ifdef __cplusplus
}
#endif
