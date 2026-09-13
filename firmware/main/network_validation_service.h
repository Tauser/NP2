#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_VALIDATION_MODE_NORMAL = 0,
    NETWORK_VALIDATION_MODE_DNS_NXDOMAIN,
    NETWORK_VALIDATION_MODE_DNS_TIMEOUT,
    NETWORK_VALIDATION_MODE_TLS_REJECT,
    NETWORK_VALIDATION_MODE_HTTPS_TIMEOUT,
    NETWORK_VALIDATION_MODE_HTTPS_OVERSIZE,
    NETWORK_VALIDATION_MODE_OFFLINE_DATA_REFRESH,
} network_validation_mode_t;

typedef struct {
    bool busy;
    bool time_synced;
    uint32_t completed_checks;
    uint32_t last_duration_ms;
    esp_err_t dns_result;
    esp_err_t ntp_result;
    esp_err_t https_result;
} network_validation_status_t;

/* Starts the one-request diagnostic executor. It performs no network I/O
 * until a physical-maintenance request is received. */
esp_err_t network_validation_service_start(void);

/* Enqueues the fixed DNS -> NTP -> HTTPS validation sequence. The one-slot
 * executor rejects overlap, so the firmware has at most one TLS handshake. */
esp_err_t network_validation_service_request_check(network_validation_mode_t mode);

/*
 * Queues one explicit refresh of the Brasília weather and BTC/USD snapshot.
 * It shares the diagnostic HTTPS worker, so it cannot create a second TLS
 * handshake or start a background polling loop.
 */
esp_err_t network_validation_service_request_offline_data_refresh(void);

void network_validation_service_get_status(network_validation_status_t *out_status);

#ifdef __cplusplus
}
#endif
