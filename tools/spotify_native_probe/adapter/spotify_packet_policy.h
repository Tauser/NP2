#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SPOTIFY_PACKET_MAX 8192U
#define SPOTIFY_MERCURY_PARTS_MAX 16U
typedef struct { size_t offset, size; } spotify_packet_slice_t;
typedef struct {
    uint64_t sequence;
    spotify_packet_slice_t header;
    spotify_packet_slice_t parts[SPOTIFY_MERCURY_PARTS_MAX];
    size_t count;
} spotify_mercury_layout_t;
bool spotify_packet_size(uint32_t total);
bool spotify_packet_mac(const uint8_t *a, const uint8_t *b, size_t size);
bool spotify_mercury_layout(const uint8_t *data, size_t size, spotify_mercury_layout_t *out);
bool spotify_mercury_status(const uint8_t *header,size_t size,int32_t *status);
#ifdef __cplusplus
}
#endif
