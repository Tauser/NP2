#pragma once
#include "google_agenda_codec.h"
#include "spotify_http_policy.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Reuse the existing bounded tokenizer (512 tokens, depth 8). Caller owns
 * an INTERNAL workspace; no recursive DOM or global cJSON allocator hook. */
typedef struct {
    google_json_t json;
    char text[SPOTIFY_HTTP_URL_MAX + 1U];
    uint32_t expires_s;
} spotify_response_workspace_t;
bool spotify_response_decode(spotify_response_workspace_t *, spotify_http_kind_t,
    const char *body, size_t size);
#ifdef __cplusplus
}
#endif
