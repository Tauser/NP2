#pragma once
#include "esp_err.h"
#include "spotify_http_transport.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Blocking library-side call. One slot, copied input, bounded wait. Caller must
 * release (and scrub) the successful body; failures transfer no allocation. */
int spotify_http_bridge_exchange(const spotify_http_request_t *, np_spotify_http_reply_t *);
/* Only the existing HTTPS executor calls this, once per accepted request,
 * including failures before HTTP (DNS/NTP/offline). */
esp_err_t spotify_http_bridge_process(esp_err_t prerequisite_result);
/* Numeric, credential-free laboratory telemetry. No URLs/headers/body. */
typedef struct {
    uint32_t count, kind, status, duration_ms, bytes, free_before, minimum_internal, network_stack;
    esp_err_t result;
} spotify_http_diagnostics_t;
void spotify_http_bridge_get_diagnostics(spotify_http_diagnostics_t *);
/* Implemented by the sole HTTPS executor; private bridge-to-worker contract. */
esp_err_t network_validation_service_request_spotify_http(void);

#ifdef __cplusplus
}
#endif
