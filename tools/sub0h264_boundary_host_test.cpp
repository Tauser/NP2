// Feeds an Annex B file through the C boundary used by the camera service and prints
// how each NAL was classified. Host-only; not part of any firmware image.
#include <cstdint>
#include <cstdio>
#include <vector>

#include "np2_sub0h264.h"

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *const input = std::fopen(argv[1], "rb");
    if (input == nullptr) return 2;
    std::vector<uint8_t> data;
    uint8_t chunk[4096];
    for (size_t got; (got = std::fread(chunk, 1U, sizeof(chunk), input)) > 0U;) {
        data.insert(data.end(), chunk, chunk + got);
    }
    std::fclose(input);

    np2_sub0h264_t *const decoder = np2_sub0h264_create();
    if (decoder == nullptr) return 2;
    int frames = 0, errors = 0, unsupported = 0;
    np2_sub0h264_frame_t last = {};
    size_t start = data.size();
    const auto feed = [&](size_t begin, size_t end) {
        if (begin >= end) return;
        const int result = np2_sub0h264_decode_nal(decoder, data.data() + begin, end - begin);
        if (result == NP2_SUB0H264_FRAME) {
            ++frames;
            (void)np2_sub0h264_get_frame(decoder, &last);
        } else if (result == NP2_SUB0H264_ERROR) {
            ++errors;
        } else if (result == NP2_SUB0H264_UNSUPPORTED) {
            ++unsupported;
        }
    };
    for (size_t i = 0U; i + 3U <= data.size(); ++i) {
        if (data[i] == 0U && data[i + 1U] == 0U && data[i + 2U] == 1U) {
            if (start < i) {
                size_t end = i;
                while (end > start && data[end - 1U] == 0U) --end;
                feed(start, end);
            }
            start = i + 3U;
            i += 2U;
        }
    }
    feed(start, data.size());
    std::printf("frames=%d errors=%d unsupported=%d coded=%ux%u visible=%ux%u crop=%u,%u\n", frames, errors,
                unsupported, last.width, last.height, last.visible_width, last.visible_height, last.crop_x,
                last.crop_y);
    np2_sub0h264_destroy(decoder);
    return 0;
}
