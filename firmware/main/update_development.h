#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Physical maintenance only. No URL/key is accepted from the serial command. */
esp_err_t update_development_request(bool apply);
void update_development_log_status(void);
