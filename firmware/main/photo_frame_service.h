#pragma once
#include "photo_frame_model.h"
#include "esp_err.h"

/* The existing SD owner calls this once per iteration, after mounting. One
 * 32 KiB read maximum per call; no new task, mount, LVGL or filesystem writer. */
void photo_frame_service_step(bool mounted);
void photo_frame_service_configure(uint8_t config);
void photo_frame_service_set_options(uint8_t interval_choice, uint64_t enabled_mask);
void photo_frame_service_get_status(photo_frame_status_t *out);
/* Single UI consumer. A lease pins immutable pixels until release. */
const void *photo_frame_service_acquire(uint32_t *generation, int8_t *lease);
void photo_frame_service_release(int8_t lease);
void photo_frame_service_set_visible(bool visible);
