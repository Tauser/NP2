#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SONOFF_LAN_MAX_DISCOVERED 8U

typedef struct {
    char device_id[11];
    char address[16];
    char local_type[16];
    bool encrypted;
} sonoff_lan_device_t;

typedef struct {
    bool ready;
    bool scan_busy;
    uint32_t scan_generation;
    uint8_t device_count;
    esp_err_t last_result;
    sonoff_lan_device_t devices[SONOFF_LAN_MAX_DISCOVERED];
} sonoff_lan_status_t;

/* Owns local service discovery on a worker task; no API performs mDNS on the
 * caller. The public snapshot intentionally contains no account tokens or keys. */
esp_err_t sonoff_lan_service_start(void);
esp_err_t sonoff_lan_service_request_scan(void);
void sonoff_lan_service_get_status(sonoff_lan_status_t *out_status);

#ifdef __cplusplus
}
#endif
