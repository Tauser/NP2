#pragma once
#include "spotify_http_policy.h"
/* Private transport data; never publish to AppState, EventBus or logs. */
typedef struct {
    uint8_t *body; /* Owned internal malloc-compatible allocation on success. */
    size_t size, capacity;
    int status;
    int64_t content_length;
    char content_range[97];
} np_spotify_http_reply_t;
