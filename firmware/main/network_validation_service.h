#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "update_keyring.h"
#include "update_policy.h"
#include "update_replay_policy.h"
#include "update_https_policy.h"

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
    NETWORK_VALIDATION_MODE_P4_UPDATE_PREFLIGHT,
    NETWORK_VALIDATION_MODE_P4_UPDATE_APPLY,
    NETWORK_VALIDATION_MODE_EWELINK_SYNC,
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

/* Starts the single-flight network executor. After the station obtains an IP,
 * it serializes BTC (5 min), Brasília weather (2 h), and USD/BRL PTAX (24 h).
 * The worker never overlaps DNS/TLS/provider work; maintenance shares it. */
esp_err_t network_validation_service_start(void);

/* Enqueues the fixed DNS -> NTP -> HTTPS validation sequence. The one-slot
 * executor rejects overlap, so the firmware has at most one TLS handshake. */
esp_err_t network_validation_service_request_check(network_validation_mode_t mode);

/*
 * Queues one explicit refresh of the Brasília weather, BTC/USD and USD/BRL
 * PTAX snapshot.
 * It shares the diagnostic HTTPS worker, so it cannot create a second TLS
 * handshake or start a background polling loop.
 */
esp_err_t network_validation_service_request_offline_data_refresh(void);

/* Queues one explicit eWeLink inventory sync on this same single-flight HTTPS
 * worker. Credentials remain owned by the eWeLink service and are consumed
 * once by the worker. */
esp_err_t network_validation_service_request_ewelink_sync(void);

/* Development-only OTA preflight on the existing one-request HTTPS worker. */
esp_err_t network_validation_service_request_p4_update_preflight(
    const update_https_endpoints_t *endpoints);

/* Development OTA request. Trust is supplied by the platform as immutable
 * public DER entries; no private key, credential or URL is persisted. */
#define NETWORK_P4_UPDATE_MAX_TRUSTED_KEYS 4U
typedef struct {
    update_https_endpoints_t endpoints;
    const update_keyring_entry_t *keyring_entries;
    size_t keyring_entries_count;
    update_environment_t environment;
} network_p4_update_request_t;

esp_err_t network_validation_service_request_p4_update_apply(
    const network_p4_update_request_t *request);

void network_validation_service_get_status(network_validation_status_t *out_status);

#ifdef __cplusplus
}
#endif
