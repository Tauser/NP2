#include "i420_uyy.h"

#include <string.h>

size_t i420_uyy_bytes(uint16_t width, uint16_t height)
{
    return (size_t)width * height * 3U / 2U;
}

static inline uint32_t load32(const uint8_t *p)
{
    uint32_t value;
    memcpy(&value, p, sizeof(value));
    return value;
}

static inline void store32(uint8_t *p, uint32_t value)
{
    memcpy(p, &value, sizeof(value));
}

/* Eight pixels of one line: chroma bytes c0..c3 and luma y0..y7 become
 * c0 y0 y1 c1 | y2 y3 c2 y4 | y5 c3 y6 y7, three little-endian words. */
static inline void pack_eight(uint8_t *out, uint32_t luma_low, uint32_t luma_high, uint32_t chroma)
{
    store32(out, (chroma & 0xFFU) | ((luma_low & 0xFFFFU) << 8U) | ((chroma & 0xFF00U) << 16U));
    store32(out + 4, (luma_low >> 16U) | (chroma & 0x00FF0000U) | ((luma_high & 0xFFU) << 24U));
    store32(out + 8, ((luma_high >> 8U) & 0xFFU) | ((chroma >> 16U) & 0xFF00U) | (luma_high & 0xFFFF0000U));
}

bool i420_pack_uyy_vyy(const i420_rgb565_source_t *source, uint8_t *dst, bool swap_uv)
{
    if (source == NULL || dst == NULL || source->y == NULL || source->u == NULL || source->v == NULL ||
        source->width == 0U || source->height == 0U || (source->width & 1U) != 0U ||
        (source->height & 1U) != 0U) {
        return false;
    }
    const size_t width = source->width;
    const size_t line_bytes = width * 3U / 2U;
    for (size_t row = 0U; row < source->height; row += 2U) {
        const uint8_t *const y0 = source->y + row * source->y_stride;
        const uint8_t *const y1 = y0 + source->y_stride;
        const uint8_t *const first = (swap_uv ? source->v : source->u) + (row / 2U) * source->uv_stride;
        const uint8_t *const second = (swap_uv ? source->u : source->v) + (row / 2U) * source->uv_stride;
        uint8_t *odd = dst + row * line_bytes;
        uint8_t *even = odd + line_bytes;
        size_t x = 0U;
        for (; x + 8U <= width; x += 8U) {
            pack_eight(odd, load32(y0 + x), load32(y0 + x + 4U), load32(first + x / 2U));
            pack_eight(even, load32(y1 + x), load32(y1 + x + 4U), load32(second + x / 2U));
            odd += 12;
            even += 12;
        }
        for (; x < width; x += 2U) {
            odd[0] = first[x / 2U];
            odd[1] = y0[x];
            odd[2] = y0[x + 1U];
            even[0] = second[x / 2U];
            even[1] = y1[x];
            even[2] = y1[x + 1U];
            odd += 3;
            even += 3;
        }
    }
    return true;
}
