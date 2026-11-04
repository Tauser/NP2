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

#define ONVIF_DISCOVERY_MAX_CANDIDATES 8U

/* A host in the panel's subnet that answers like a camera. Not yet registered. */
typedef struct {
    char address[16];
    char label[24];
} onvif_candidate_t;

typedef struct {
    bool busy;
    uint8_t progress_percent;
    uint32_t generation; /* Bumps when a sweep finishes. */
    esp_err_t last_result;
    uint8_t count;
    onvif_candidate_t candidates[ONVIF_DISCOVERY_MAX_CANDIDATES];
} onvif_sweep_status_t;

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

/* Looks for cameras in the panel's own subnet instead of relying on multicast, which many
 * cameras ignore: any host with an RTSP server on port 554 is listed. ESP_ERR_TIMEOUT when a
 * sweep is already running. Results come from onvif_discovery_service_get_sweep(). */
esp_err_t onvif_discovery_service_request_sweep(void);
void onvif_discovery_service_get_sweep(onvif_sweep_status_t *out_status);

/* Registers a camera whose account was just verified. It joins the saved list and starts
 * online. ESP_ERR_NO_MEM when four cameras are already registered. */
esp_err_t onvif_discovery_service_add_camera(const char *address, const char *model);

#ifdef __cplusplus
}
#endif
