/* Checks the optimized converter against the reference formula it replaced.
 * gcc -std=c11 -O2 -Ifirmware/main tools/i420_rgb565_host_test.c firmware/main/i420_rgb565.c */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i420_rgb565.h"

static uint16_t reference(int y, int u, int v)
{
    const int c = y - 16, d = u - 128, e = v - 128;
    const int r = (298 * c + 409 * e + 128) >> 8;
    const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    const int b = (298 * c + 516 * d + 128) >> 8;
    const unsigned rr = (unsigned)(r < 0 ? 0 : r > 255 ? 255 : r) >> 3;
    const unsigned gg = (unsigned)(g < 0 ? 0 : g > 255 ? 255 : g) >> 2;
    const unsigned bb = (unsigned)(b < 0 ? 0 : b > 255 ? 255 : b) >> 3;
    return (uint16_t)((rr << 11) | (gg << 5) | bb);
}

static int check_all_values(void)
{
    for (int y = 0; y < 256; ++y) {
        for (int u = 0; u < 256; ++u) {
            for (int v = 0; v < 256; ++v) {
                const uint8_t ys[4] = {(uint8_t)y, (uint8_t)y, (uint8_t)y, (uint8_t)y};
                const uint8_t us = (uint8_t)u, vs = (uint8_t)v;
                uint16_t out[4] = {0};
                const i420_rgb565_source_t source = {ys, &us, &vs, 2U, 1U, 2U, 2U};
                if (!i420_rgb565_convert(&source, out) || out[3] != reference(y, u, v)) {
                    printf("FAIL value y=%d u=%d v=%d\n", y, u, v);
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Padded strides with a cropped origin must not read or write outside the rectangle. */
static int check_crop_and_stride(void)
{
    enum { W = 16, H = 8, YS = 32, UVS = 20, OX = 4, OY = 2, ROWS = 12 };
    uint8_t y[YS * ROWS], u[UVS * ROWS], v[UVS * ROWS];
    for (size_t i = 0; i < sizeof(y); ++i) y[i] = (uint8_t)rand();
    for (size_t i = 0; i < sizeof(u); ++i) {
        u[i] = (uint8_t)rand();
        v[i] = (uint8_t)rand();
    }
    uint16_t out[W * H + 2];
    out[W * H] = 0xABCD;
    out[W * H + 1] = 0xABCD;
    const i420_rgb565_source_t source = {y + OY * YS + OX, u + (OY / 2) * UVS + OX / 2,
                                         v + (OY / 2) * UVS + OX / 2, YS, UVS, W, H};
    if (!i420_rgb565_convert(&source, out) || out[W * H] != 0xABCD || out[W * H + 1] != 0xABCD) {
        puts("FAIL crop bounds");
        return 1;
    }
    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            const int yy = y[(row + OY) * YS + col + OX];
            const int uu = u[((row + OY) / 2) * UVS + (col + OX) / 2];
            const int vv = v[((row + OY) / 2) * UVS + (col + OX) / 2];
            if (out[row * W + col] != reference(yy, uu, vv)) {
                printf("FAIL pixel %d,%d\n", col, row);
                return 1;
            }
        }
    }
    return 0;
}

static int check_rejections(void)
{
    const uint8_t plane[4] = {0};
    uint16_t out[4] = {0x1234, 0x1234, 0x1234, 0x1234};
    const i420_rgb565_source_t odd = {plane, plane, plane, 2U, 1U, 1U, 2U};
    const i420_rgb565_source_t empty = {plane, plane, plane, 2U, 1U, 0U, 0U};
    if (i420_rgb565_convert(&odd, out) || i420_rgb565_convert(&empty, out) ||
        i420_rgb565_convert(NULL, out) || out[0] != 0x1234) {
        puts("FAIL rejections");
        return 1;
    }
    return 0;
}

int main(void)
{
    const int failures = check_all_values() + check_crop_and_stride() + check_rejections();
    if (failures == 0) puts("PASS i420_rgb565");
    return failures;
}
