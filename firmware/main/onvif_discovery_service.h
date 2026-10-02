#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ONVIF_DISCOVERY_MAX_CAMERAS 4U

typedef struct {
    char address[16];
    char model[32];
    bool online;
} onvif_camera_t;

typedef struct {
    bool ready;
    bool scan_busy;
    uint32_t scan_generation;
    uint8_t camera_count;
    esp_err_t last_result;
    onvif_camera_t cameras[ONVIF_DISCOVERY_MAX_CAMERAS];
} onvif_discovery_status_t;

/* Sends bounded WS-Discovery probes from a worker; snapshots contain only
 * public model/address data and never camera credentials. */
esp_err_t onvif_discovery_service_start(void);
esp_err_t onvif_discovery_service_request_scan(void);
/* Adds an IPv4 endpoint entered by the user and queues a port-2020 check. */
esp_err_t onvif_discovery_service_request_address(const char *address);
void onvif_discovery_service_get_status(onvif_discovery_status_t *out_status);

#ifdef __cplusplus
}
#endif
