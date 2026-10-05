#pragma once

#include "np_ewelink.h"

esp_err_t np_ewelink_storage_load(np_ewelink_inventory_t *out_inventory);
esp_err_t np_ewelink_storage_save(const np_ewelink_inventory_t *inventory);
