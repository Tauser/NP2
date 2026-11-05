#include "i420_rgb565.h"

/* Table driven BT.601 conversion. Per pixel: one luma lookup, three adds, three shifts and three
 * lookups that already clamp and pack each channel, so there are no branches. The tables are
 * built once and live in internal RAM. They reproduce the original integer formula exactly:
 *   r = (298 * (y - 16) + 409 * (v - 128) + 128) >> 8, and likewise g and b, clamped to 0..255.
 * Index range of (luma + chroma) >> 8 is -277..534, hence the offset and the 1024 entries. */
#define CLAMP_OFFSET 320
#define CLAMP_ENTRIES 1024

static int32_t s_luma[256];
static int32_t s_red_v[256];
static int32_t s_green_u[256];
static int32_t s_green_v[256];
static int32_t s_blue_u[256];
static uint16_t s_red[CLAMP_ENTRIES];
static uint16_t s_green[CLAMP_ENTRIES];
static uint16_t s_blue[CLAMP_ENTRIES];
static bool s_tables_ready;

static void build_tables(void)
{
    for (int i = 0; i < 256; ++i) {
        s_luma[i] = 298 * (i - 16);
        s_red_v[i] = 409 * (i - 128) + 128;
        s_green_u[i] = -100 * (i - 128) + 128;
        s_green_v[i] = -208 * (i - 128);
        s_blue_u[i] = 516 * (i - 128) + 128;
    }
    for (int i = 0; i < CLAMP_ENTRIES; ++i) {
        int value = i - CLAMP_OFFSET;
        value = value < 0 ? 0 : (value > 255 ? 255 : value);
        s_red[i] = (uint16_t)((unsigned)(value >> 3) << 11U);
        s_green[i] = (uint16_t)((unsigned)(value >> 2) << 5U);
        s_blue[i] = (uint16_t)(value >> 3);
    }
    s_tables_ready = true;
}

#define PIXEL(luma, red, green, blue)                                           \
    ((uint32_t)s_red[(((luma) + (red)) >> 8) + CLAMP_OFFSET] |                  \
     (uint32_t)s_green[(((luma) + (green)) >> 8) + CLAMP_OFFSET] |              \
     (uint32_t)s_blue[(((luma) + (blue)) >> 8) + CLAMP_OFFSET])

bool i420_rgb565_convert(const i420_rgb565_source_t *source, uint16_t *dst)
{
    if (source == NULL || dst == NULL || source->y == NULL || source->u == NULL || source->v == NULL ||
        source->width == 0U || source->height == 0U || (source->width & 1U) != 0U ||
        (source->height & 1U) != 0U) {
        return false;
    }
    if (!s_tables_ready) build_tables();
    const size_t width = source->width;
    const size_t half_width = width / 2U;
    /* Two pixels go out in one 32-bit store when the destination allows it. Rows are an even number of
     * pixels, so every row stays 4-byte aligned if the first one is. */
    const bool wide_store = (((uintptr_t)dst) & 3U) == 0U;
    /* One chroma sample covers a 2x2 block, so its terms are computed once per block. */
    for (size_t row = 0U; row < source->height; row += 2U) {
        const uint8_t *const y0 = source->y + row * source->y_stride;
        const uint8_t *const y1 = y0 + source->y_stride;
        const uint8_t *const u = source->u + (row / 2U) * source->uv_stride;
        const uint8_t *const v = source->v + (row / 2U) * source->uv_stride;
        uint16_t *const out0 = dst + row * width;
        uint16_t *const out1 = out0 + width;
        for (size_t block = 0U; block < half_width; ++block) {
            const int32_t red = s_red_v[v[block]];
            const int32_t green = s_green_u[u[block]] + s_green_v[v[block]];
            const int32_t blue = s_blue_u[u[block]];
            const size_t left = block * 2U;
            const uint32_t top = PIXEL(s_luma[y0[left]], red, green, blue) |
                                 (PIXEL(s_luma[y0[left + 1U]], red, green, blue) << 16U);
            const uint32_t bottom = PIXEL(s_luma[y1[left]], red, green, blue) |
                                    (PIXEL(s_luma[y1[left + 1U]], red, green, blue) << 16U);
            if (wide_store) {
                *(uint32_t *)(void *)(out0 + left) = top;
                *(uint32_t *)(void *)(out1 + left) = bottom;
            } else {
                out0[left] = (uint16_t)top;
                out0[left + 1U] = (uint16_t)(top >> 16U);
                out1[left] = (uint16_t)bottom;
                out1[left + 1U] = (uint16_t)(bottom >> 16U);
            }
        }
    }
    return true;
}
