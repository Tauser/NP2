/* C boundary for the Sub0h264 decoder used by the camera stream service. */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct np2_sub0h264 np2_sub0h264_t;

#define NP2_SUB0H264_FRAME 1
#define NP2_SUB0H264_CONSUMED 0
#define NP2_SUB0H264_ERROR (-1)
#define NP2_SUB0H264_UNSUPPORTED (-2)

typedef struct {
    const uint8_t *y;
    const uint8_t *u;
    const uint8_t *v;
    uint16_t width;   /* Coded size; plane pointers address the coded picture. */
    uint16_t height;
    uint16_t y_stride;
    uint16_t uv_stride;
    uint16_t crop_x;  /* Visible rectangle from the SPS, in luma samples (even). */
    uint16_t crop_y;
    uint16_t visible_width;
    uint16_t visible_height;
} np2_sub0h264_frame_t;

np2_sub0h264_t *np2_sub0h264_create(void);
void np2_sub0h264_destroy(np2_sub0h264_t *decoder);
size_t np2_sub0h264_object_bytes(void);
/* NAL without start code. NP2_SUB0H264_UNSUPPORTED means the stream uses a
 * feature this decoder would corrupt or could not bound in memory: more than
 * 640x368, interlace, over 4 references, slice groups, weighted prediction or
 * several slices per picture. It is rejected before reaching the decoder. */
int np2_sub0h264_decode_nal(np2_sub0h264_t *decoder, const uint8_t *nal, size_t size);
/* Valid until the next decode call. */
int np2_sub0h264_get_frame(const np2_sub0h264_t *decoder, np2_sub0h264_frame_t *out);

#ifdef __cplusplus
}
#endif
