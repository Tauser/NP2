#include "i420_rgb565.h"

/* Packs one pixel. Out-of-range channels are rare, so the branch predicts well. */
static inline uint16_t pack_pixel(int32_t luma, int32_t red, int32_t green, int32_t blue)
{
    int32_t r = (luma + red) >> 8;
    int32_t g = (luma + green) >> 8;
    int32_t b = (luma + blue) >> 8;
    if ((uint32_t)r > 255U) r = r < 0 ? 0 : 255;
    if ((uint32_t)g > 255U) g = g < 0 ? 0 : 255;
    if ((uint32_t)b > 255U) b = b < 0 ? 0 : 255;
    return (uint16_t)(((uint32_t)(r >> 3) << 11U) | ((uint32_t)(g >> 2) << 5U) | (uint32_t)(b >> 3));
}

bool i420_rgb565_convert(const i420_rgb565_source_t *source, uint16_t *dst)
{
    if (source == NULL || dst == NULL || source->y == NULL || source->u == NULL || source->v == NULL ||
        source->width == 0U || source->height == 0U || (source->width & 1U) != 0U ||
        (source->height & 1U) != 0U) {
        return false;
    }
    const size_t width = source->width;
    const size_t half_width = width / 2U;
    /* One chroma sample covers a 2x2 block, so its terms are computed once per block. */
    for (size_t row = 0U; row < source->height; row += 2U) {
        const uint8_t *const y0 = source->y + row * source->y_stride;
        const uint8_t *const y1 = y0 + source->y_stride;
        const uint8_t *const u = source->u + (row / 2U) * source->uv_stride;
        const uint8_t *const v = source->v + (row / 2U) * source->uv_stride;
        uint16_t *const out0 = dst + row * width;
        uint16_t *const out1 = out0 + width;
        for (size_t block = 0U; block < half_width; ++block) {
            const int32_t d = (int32_t)u[block] - 128;
            const int32_t e = (int32_t)v[block] - 128;
            const int32_t red = 409 * e + 128;
            const int32_t green = -100 * d - 208 * e + 128;
            const int32_t blue = 516 * d + 128;
            const size_t left = block * 2U;
            const size_t right = left + 1U;
            out0[left] = pack_pixel(298 * ((int32_t)y0[left] - 16), red, green, blue);
            out0[right] = pack_pixel(298 * ((int32_t)y0[right] - 16), red, green, blue);
            out1[left] = pack_pixel(298 * ((int32_t)y1[left] - 16), red, green, blue);
            out1[right] = pack_pixel(298 * ((int32_t)y1[right] - 16), red, green, blue);
        }
    }
    return true;
}
