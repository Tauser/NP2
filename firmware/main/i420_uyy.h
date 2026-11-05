/* Repacks planar I420 into the YUV420 layout the P4 PPA reads ("O_UYY_E_VYY"): the first line of
 * each pair carries "U Y Y" per two pixels and the second "V Y Y". Pure C, no I/O or allocation. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i420_rgb565.h"

#ifdef __cplusplus
extern "C" {
#endif

/* width * height * 3 / 2 bytes, both dimensions even. */
size_t i420_uyy_bytes(uint16_t width, uint16_t height);

/* source honours strides and a cropped origin. dst needs i420_uyy_bytes() bytes. swap_uv exists for
 * the self-test that found the layout. Returns false on a null pointer or odd/zero dimension. */
bool i420_pack_uyy_vyy(const i420_rgb565_source_t *source, uint8_t *dst, bool swap_uv);

#ifdef __cplusplus
}
#endif
