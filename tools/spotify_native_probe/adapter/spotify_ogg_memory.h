#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *spotify_ogg_malloc(size_t);
void *spotify_ogg_calloc(size_t,size_t);
void *spotify_ogg_realloc(void *,size_t);
void spotify_ogg_free(void *);
#ifdef __cplusplus
}
#endif
