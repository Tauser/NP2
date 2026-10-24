#pragma once
#include "spotify_http_transport.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*np_spotify_http_exchange_t)(const spotify_http_request_t *, np_spotify_http_reply_t *);
/* Bind once, before starting library tasks. No mutable std::function global. */
void np_spotify_bind_http(np_spotify_http_exchange_t exchange);
#ifdef __cplusplus
}
#endif
