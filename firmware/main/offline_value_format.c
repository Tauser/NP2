#include "offline_value_format.h"

#include <stdio.h>

static bool format_signed_fixed(int16_t value, uint16_t divisor, unsigned int decimals,
                                char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) {
        return false;
    }
    const int32_t widened = value;
    const uint32_t magnitude = (uint32_t)(widened < 0 ? -widened : widened);
    const int written = snprintf(out, out_size, "%s%lu.%0*lu", widened < 0 ? "-" : "",
                                 (unsigned long)(magnitude / divisor), (int)decimals,
                                 (unsigned long)(magnitude % divisor));
    return written >= 0 && (size_t)written < out_size;
}

bool offline_format_temperature(int16_t temperature_deci_c, char *out, size_t out_size)
{
    return format_signed_fixed(temperature_deci_c, 10U, 1U, out, out_size);
}

bool offline_format_market_change(int16_t change_basis_points, char *out, size_t out_size)
{
    return format_signed_fixed(change_basis_points, 100U, 2U, out, out_size);
}
