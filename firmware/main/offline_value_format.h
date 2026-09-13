/* Portable signed fixed-point formatting for the offline dashboard. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool offline_format_temperature(int16_t temperature_deci_c, char *out, size_t out_size);
bool offline_format_market_change(int16_t change_basis_points, char *out, size_t out_size);
