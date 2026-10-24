#pragma once
#include <stdint.h>
#include "esp_err.h"
/* Separate record: older firmware keeps its display/photo profile intact. */
esp_err_t home_widget_owner_load(uint8_t *widget);
esp_err_t home_widget_owner_write(uint8_t widget);
