/* I420 to RGB565 conversion for one decoded camera picture. No I/O or allocation. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *y; /* First visible sample of each plane. */
    const uint8_t *u;
    const uint8_t *v;
    uint16_t y_stride;
    uint16_t uv_stride;
    uint16_t width;   /* Both even and non-zero. */
    uint16_t height;
} i420_rgb565_source_t;

/* BT.601 limited range. dst holds width*height pixels with no row padding.
 * Returns false, writing nothing, on a null pointer or odd/zero dimension. */
bool i420_rgb565_convert(const i420_rgb565_source_t *source, uint16_t *dst);

#ifdef __cplusplus
}
#endif
