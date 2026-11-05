/* YUV420 to scaled RGB565 on the P4's PPA. The input is the P4 layout built by i420_pack_uyy_vyy(). */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I420_PPA_ALIGN 128U

size_t i420_ppa_align(size_t bytes);
/* Bytes of a packed picture, already rounded to the buffer alignment the PPA needs. */
size_t i420_ppa_packed_bytes(uint16_t width, uint16_t height);
/* Pixels per output row that LVGL accepts without copying the image: a row of width * 2 bytes
 * rounded up to LV_DRAW_BUF_STRIDE_ALIGN (64). A different stride makes LVGL duplicate every picture
 * into a buffer its small pool cannot provide, and nothing is drawn. */
uint16_t i420_ppa_row_pitch(uint16_t width);
/* Output size for a scale, in whole pixels (the hardware truncates to 1/16 steps). */
void i420_ppa_output_size(uint16_t width, uint16_t height, float scale, uint16_t *out_w, uint16_t *out_h);

esp_err_t i420_ppa_init(void);
bool i420_ppa_ready(void);

/* packed holds i420_ppa_packed_bytes() bytes aligned to I420_PPA_ALIGN. out is aligned to
 * I420_PPA_ALIGN, rows are pitch_pixels apart and out_bytes is a multiple of the alignment.
 * ESP_ERR_INVALID_SIZE when the scaled picture does not fit that pitch and size.
 *
 * i420_ppa_start() returns as soon as the work is queued; neither buffer may be touched until
 * i420_ppa_wait() reports it done. Only one operation is in flight: ESP_ERR_INVALID_STATE otherwise. */
esp_err_t i420_ppa_start(const uint8_t *packed, uint16_t width, uint16_t height, float scale,
                         uint16_t *out, size_t out_bytes, uint16_t pitch_pixels);
/* ESP_OK when the operation started earlier has finished; ESP_ERR_TIMEOUT if it has not;
 * ESP_ERR_INVALID_STATE when none is in flight. */
esp_err_t i420_ppa_wait(uint32_t timeout_ms);
/* Start and wait. */
esp_err_t i420_ppa_convert(const uint8_t *packed, uint16_t width, uint16_t height, float scale,
                           uint16_t *out, size_t out_bytes, uint16_t pitch_pixels);

#ifdef __cplusplus
}
#endif
