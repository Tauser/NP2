#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
enum { SPOTIFY_COVER_MAX_EDGE = 300, SPOTIFY_COVER_MAX_JPEG = 32768,
       SPOTIFY_COVER_ALIGNMENT = 64, SPOTIFY_COVER_MAX_STRIDE = 640 };
/* Worker-owned fixed decoder pool and two RGB565 slots. No LVGL I/O here. */
typedef struct {
    uint32_t generation;
    uint16_t width, height, stride;
    alignas(SPOTIFY_COVER_ALIGNMENT) uint8_t pixels[SPOTIFY_COVER_MAX_STRIDE*SPOTIFY_COVER_MAX_EDGE];
} spotify_cover_t;
bool spotify_cover_url_allowed(const char *);
bool spotify_cover_decode(const uint8_t *jpeg, size_t size, uint32_t *generation);
uint32_t spotify_cover_clear(void);
const spotify_cover_t *spotify_cover_acquire(uint32_t generation, int8_t *lease);
void spotify_cover_release(int8_t lease);
