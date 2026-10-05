#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t np_ewelink_auth_sign_json(const uint8_t *json, size_t json_size,
                                    char *out_signature, size_t out_size);
const char *np_ewelink_auth_app_id(void);
const char *np_ewelink_auth_region_for_country(const char *country_code);
const char *np_ewelink_auth_api_host(const char *region);
