#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * Phase 3 Hosted/Wi-Fi owner. It runs outside LVGL, retains station settings
 * only in RAM and exposes a private-copy request API for a future credential
 * mailbox. It does not persist, perform DNS, NTP or HTTPS, or call LVGL.
 */
typedef enum {
    CONNECTIVITY_DIAGNOSTIC_STATE_IDLE = 0,
    CONNECTIVITY_DIAGNOSTIC_STATE_STARTING,
    CONNECTIVITY_DIAGNOSTIC_STATE_LINK_UP,
    CONNECTIVITY_DIAGNOSTIC_STATE_WIFI_READY,
    CONNECTIVITY_DIAGNOSTIC_STATE_SCANNING,
    CONNECTIVITY_DIAGNOSTIC_STATE_SCAN_COMPLETE,
    CONNECTIVITY_DIAGNOSTIC_STATE_ASSOCIATING,
    CONNECTIVITY_DIAGNOSTIC_STATE_WAITING_FOR_IP,
    CONNECTIVITY_DIAGNOSTIC_STATE_ONLINE,
    CONNECTIVITY_DIAGNOSTIC_STATE_BACKOFF,
    CONNECTIVITY_DIAGNOSTIC_STATE_LINK_DOWN,
    CONNECTIVITY_DIAGNOSTIC_STATE_RECOVERING_LINK,
    CONNECTIVITY_DIAGNOSTIC_STATE_FAILED,
} connectivity_diagnostic_state_t;

typedef struct {
    connectivity_diagnostic_state_t state;
    esp_err_t last_result;
    uint16_t access_points_found;
    uint32_t reconnect_attempts;
    uint32_t consecutive_dns_failures;
    uint32_t transport_failures;
    uint32_t recovery_cycles;
    uint8_t last_disconnect_reason;
    bool link_up;
    bool wifi_ready;
    bool station_credentials_in_ram;
    bool online;
} connectivity_diagnostic_status_t;

/* Starts the worker and returns without waiting for SDIO, Wi-Fi or scan work. */
esp_err_t connectivity_diagnostic_start(void);

/*
 * Copies one station request to the worker's one-entry private mailbox. The
 * caller must clear its own source buffers immediately after this call.
 */
esp_err_t connectivity_diagnostic_request_join(const char *ssid, const char *password);

/* Removes the RAM-only station configuration and cancels future reconnects. */
esp_err_t connectivity_diagnostic_request_forget(void);

/*
 * Requests one physically authorized Hosted/Wi-Fi lifecycle recovery. The
 * worker owns all driver calls and limits recovery to three cycles per ten
 * minutes before a five-minute cooldown. It never drives reset54 directly.
 */
esp_err_t connectivity_diagnostic_request_hosted_recovery(void);

/* Runs three Hosted recoveries with IP confirmation, then verifies that the
 * fourth request enters the five-minute cooldown. Physical maintenance only. */
esp_err_t connectivity_diagnostic_request_hosted_recovery_cooldown_campaign(void);

/* Physical test only: additionally waits through the five-minute cooldown
 * and verifies that a fifth Hosted recovery is allowed and recovers IP. */
esp_err_t connectivity_diagnostic_request_hosted_recovery_full_cooldown_campaign(void);

/* Physical test only: locally suppresses DHCP once, verifies the 20 s
 * deadline, restores the client and relies on normal bounded reassociation. */
esp_err_t connectivity_diagnostic_request_dhcp_silence_injection(void);

/* Records a bounded DNS result. Two consecutive failures can request one
 * worker-owned reassociation per minute; this API never performs Wi-Fi I/O. */
void connectivity_diagnostic_report_dns_result(esp_err_t result);

/* Reads a lock-protected snapshot; safe from the LVGL task. */
void connectivity_diagnostic_get_status(connectivity_diagnostic_status_t *out_status);
