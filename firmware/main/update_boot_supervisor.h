#pragma once

#include "esp_err.h"

/* Starts health confirmation only when the running P4 image is pending verify. */
esp_err_t update_boot_supervisor_start(void);
