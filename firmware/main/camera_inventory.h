/* Persisted list of known cameras. Only public data: IPv4 address and model.
 * The account lives separately in camera_credentials and is never stored here. */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_INVENTORY_MAX 4U

typedef struct {
    uint8_t count;
    struct {
        char address[16];
        char model[32];
    } cameras[CAMERA_INVENTORY_MAX];
} camera_inventory_t;

/* True once the coordinator has tried to load the saved list, successfully or not. */
bool camera_inventory_load_attempted(void);
/* Copies what is saved in flash, not what the scanner currently sees. */
void camera_inventory_get(camera_inventory_t *out);

/* Called only by the flash coordinator task. */
esp_err_t camera_inventory_owner_load(void);
esp_err_t camera_inventory_owner_write(const camera_inventory_t *inventory);

#ifdef __cplusplus
}
#endif
