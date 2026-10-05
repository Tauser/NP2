#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NP_EWELINK_MAX_DEVICES 8U
#define NP_EWELINK_MAX_CHANNELS 3U
#define NP_EWELINK_DEVICE_ID_BYTES 32U
#define NP_EWELINK_DEVICE_KEY_BYTES 64U
#define NP_EWELINK_API_KEY_BYTES 64U
#define NP_EWELINK_PARAMS_JSON_BYTES 1024U
#define NP_EWELINK_LOCAL_SETTINGS_BYTES 64U

typedef enum {
    NP_EWELINK_UNKNOWN = 0,
    NP_EWELINK_TX1C,
    NP_EWELINK_TX2C,
    NP_EWELINK_TX3C,
} np_ewelink_model_t;

typedef struct {
    char device_id[NP_EWELINK_DEVICE_ID_BYTES];
    char device_key[NP_EWELINK_DEVICE_KEY_BYTES];
    char api_key[NP_EWELINK_API_KEY_BYTES];
    char name[48];
    char brand_name[32];
    char product_model[24];
    char internal_model[32];
    char sta_mac[24];
    int32_t uiid;
    bool online;
    bool present_last_sync;
    np_ewelink_model_t model;
    uint8_t channel_count;
    char channel_names[NP_EWELINK_MAX_CHANNELS][32];
    char params_json[NP_EWELINK_PARAMS_JSON_BYTES];
    /* Opaque NovaPanel-only settings survive cloud reconciliation. */
    uint8_t local_settings[NP_EWELINK_LOCAL_SETTINGS_BYTES];
} np_ewelink_device_t;

typedef struct {
    uint8_t count;
    np_ewelink_device_t devices[NP_EWELINK_MAX_DEVICES];
} np_ewelink_inventory_t;

/* UI/app-state view of the LAN inventory. Intentionally excludes devicekey,
 * apikey, params and NovaPanel private settings. */
typedef struct {
    char device_id[NP_EWELINK_DEVICE_ID_BYTES];
    char name[48];
    char product_model[24];
    uint8_t channel_count;
    char channel_names[NP_EWELINK_MAX_CHANNELS][32];
    bool present_last_sync;
} np_ewelink_public_device_t;

typedef struct {
    uint8_t count;
    np_ewelink_public_device_t devices[NP_EWELINK_MAX_DEVICES];
} np_ewelink_public_inventory_t;

typedef enum {
    NP_EWELINK_SYNC_IDLE = 0,
    NP_EWELINK_SYNC_AUTHENTICATING,
    NP_EWELINK_SYNC_FETCHING,
    NP_EWELINK_SYNC_RECONCILING,
    NP_EWELINK_SYNC_SAVING,
    NP_EWELINK_SYNC_COMPLETE,
    NP_EWELINK_SYNC_FAILED,
} np_ewelink_sync_stage_t;

typedef struct {
    bool ready;
    bool busy;
    uint8_t device_count;
    uint32_t generation;
    uint32_t completed_syncs;
    esp_err_t last_result;
    np_ewelink_sync_stage_t sync_stage;
} np_ewelink_status_t;

/* Initializes the permanent eWeLink service and loads its LAN inventory. */
esp_err_t np_ewelink_init(void);

/* Reloads the persisted LAN inventory without contacting eWeLink. */
esp_err_t np_ewelink_load_inventory(void);

/* Copies the current reconciled inventory to a caller-owned snapshot. The
 * snapshot includes device keys and must be scrubbed when no longer needed. */
esp_err_t np_ewelink_get_inventory(np_ewelink_inventory_t *out_inventory);
/* Sanitized projection for AppState/UI. No cryptographic keys are copied. */
esp_err_t np_ewelink_get_public_inventory(
    np_ewelink_public_inventory_t *out_inventory);

/* Queue a one-shot account synchronization on the sole HTTPS executor.
 * Password is copied only into temporary RAM and cleared at completion. The
 * caller must clear its input buffers immediately after this call returns. */
esp_err_t np_ewelink_sync_begin(const char *username, const char *password);

/* Reads the inventory already loaded from persistent storage. This status
 * snapshot never contains credentials or device keys. */
void np_ewelink_get_status(np_ewelink_status_t *out_status);

/* Internal network-worker entry point; not for UI or arbitrary tasks. */
esp_err_t np_ewelink_service_process_sync(void);

/* Internal LAN handoff. Callers must clear their output buffer after use. */
esp_err_t np_ewelink_copy_device_key(const char *device_id,
                                     char *out_key, size_t out_key_size);

#ifdef __cplusplus
}
#endif
