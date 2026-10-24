#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SPOTIFY_HTTP_URL_MAX 2048U
#define SPOTIFY_HTTP_AUTH_MAX 2048U
#define SPOTIFY_HTTP_BODY_MAX 512U
#define SPOTIFY_HTTP_JSON_MAX 8192U
#define SPOTIFY_HTTP_AUDIO_MAX 14336U
#define SPOTIFY_HTTP_RANGE_MAX 64U
typedef enum { SPOTIFY_HTTP_RESOLVE, SPOTIFY_HTTP_TOKEN, SPOTIFY_HTTP_STORAGE, SPOTIFY_HTTP_AUDIO } spotify_http_kind_t;
/* Private call arguments, never events/AppState/log data. */
typedef struct {
    const char *url, *authorization, *content_type, *range;
    const uint8_t *body;
    size_t body_size;
    bool post;
} spotify_http_request_t;
typedef struct {
    spotify_http_kind_t kind;
    size_t body_limit;
    uint64_t first, last;
    bool suffix;
} spotify_http_policy_t;
bool spotify_http_validate_request(const spotify_http_request_t *request, spotify_http_policy_t *policy);
/* Validate headers before allocation/read, and actual size again at EOF. */
bool spotify_http_validate_response(const spotify_http_policy_t *policy, int status,
    int64_t content_length, const char *content_range, size_t actual_size, bool complete,
    uint64_t *file_size);
#ifdef __cplusplus
}
#endif
