/* Saved local camera accounts, one per camera and bound to its address, so a password is
 * never offered to a different host. Nothing here reaches logs, AppState or view-models. */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAMERA_CREDENTIALS_MAX 4U

typedef struct {
    char ipv4[16];
    char username[48];
    char password[64];
} camera_credentials_t;

/* The only query the UI needs: it never reads an account itself. */
bool camera_credentials_saved_for(const char *ipv4);

/* Consumer function for the camera stream service; deliberately not for UI use. */
esp_err_t camera_credentials_copy_private(const char *ipv4, camera_credentials_t *out);

/* Called only by the flash coordinator task. The write adds or replaces one camera's account. */
esp_err_t camera_credentials_owner_load(void);
esp_err_t camera_credentials_owner_write(const camera_credentials_t *credentials);

#ifdef __cplusplus
}
#endif
