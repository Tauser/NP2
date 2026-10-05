#pragma once

#include "np_ewelink.h"

typedef void (*np_ewelink_cloud_progress_cb_t)(np_ewelink_sync_stage_t stage,
                                                void *context);

esp_err_t np_ewelink_cloud_login_and_fetch(const char *username,
                                          const char *password,
                                          np_ewelink_cloud_progress_cb_t progress,
                                          void *progress_context,
                                          np_ewelink_inventory_t *out_inventory);
