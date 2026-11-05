/* Checks the word-at-a-time packer against a byte-by-byte reference of the layout, with padded
 * strides, a cropped origin and widths that are not a multiple of eight.
 * gcc -std=c11 -O2 -Ifirmware/main tools/i420_uyy_host_test.c firmware/main/i420_uyy.c */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i420_uyy.h"

static void reference(const i420_rgb565_source_t *s, uint8_t *out, int swap)
{
    size_t index = 0U;
    for (uint16_t row = 0U; row < s->height; row += 2U) {
        for (int line = 0; line < 2; ++line) {
            const uint8_t *const y = s->y + (size_t)(row + line) * s->y_stride;
            const uint8_t *const chroma = ((line == 0) != (swap != 0) ? s->u : s->v) + (size_t)(row / 2U) * s->uv_stride;
            for (uint16_t x = 0U; x < s->width; x += 2U) {
                out[index++] = chroma[x / 2U];
                out[index++] = y[x];
                out[index++] = y[x + 1U];
            }
        }
    }
}

static int check(uint16_t width, uint16_t height, uint16_t y_stride, uint16_t uv_stride, uint16_t ox, uint16_t oy)
{
    const size_t rows = (size_t)height + oy + 2U;
    uint8_t *y = malloc((size_t)y_stride * rows), *u = malloc((size_t)uv_stride * rows), *v = malloc((size_t)uv_stride * rows);
    for (size_t i = 0U; i < (size_t)y_stride * rows; ++i) y[i] = (uint8_t)rand();
    for (size_t i = 0U; i < (size_t)uv_stride * rows; ++i) {
        u[i] = (uint8_t)rand();
        v[i] = (uint8_t)rand();
    }
    const i420_rgb565_source_t source = {y + (size_t)oy * y_stride + ox, u + (size_t)(oy / 2U) * uv_stride + ox / 2U,
                                         v + (size_t)(oy / 2U) * uv_stride + ox / 2U, y_stride, uv_stride, width, height};
    const size_t bytes = i420_uyy_bytes(width, height);
    uint8_t *got = malloc(bytes + 16U), *want = malloc(bytes);
    memset(got, 0xA5, bytes + 16U);
    int failures = 0;
    for (int swap = 0; swap < 2; ++swap) {
        reference(&source, want, swap);
        if (!i420_pack_uyy_vyy(&source, got, swap != 0) || memcmp(got, want, bytes) != 0) {
            printf("FAIL %ux%u stride=%u/%u crop=%u,%u swap=%d\n", width, height, y_stride, uv_stride, ox, oy, swap);
            ++failures;
        }
        for (size_t i = bytes; i < bytes + 16U; ++i) {
            if (got[i] != 0xA5U) {
                printf("FAIL wrote past the end %ux%u\n", width, height);
                ++failures;
                break;
            }
        }
    }
    free(y), free(u), free(v), free(got), free(want);
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += check(640, 360, 640, 320, 0, 0);   /* The camera case. */
    failures += check(640, 360, 704, 352, 8, 4);   /* Padded strides, cropped origin. */
    failures += check(16, 2, 16, 8, 0, 0);         /* Smallest whole rows of eight. */
    failures += check(18, 4, 24, 12, 2, 2);        /* Tail of two pixels after eight. */
    failures += check(26, 6, 32, 16, 4, 0);        /* Tail of two, then of two again. */
    failures += check(2, 2, 4, 2, 0, 0);           /* Tail only. */
    const i420_rgb565_source_t odd = {(const uint8_t *)"", (const uint8_t *)"", (const uint8_t *)"", 1, 1, 3, 2};
    uint8_t out[8];
    if (i420_pack_uyy_vyy(&odd, out, false) || i420_pack_uyy_vyy(NULL, out, false)) {
        puts("FAIL rejections");
        ++failures;
    }
    if (failures == 0) puts("PASS i420_uyy");
    return failures;
}
