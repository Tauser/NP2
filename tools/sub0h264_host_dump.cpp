// Decodes an Annex B file with the vendored Sub0h264 and writes cropped I420 frames.
// Host-only helper for tools/compare_sub0h264_host.py; not part of any firmware image.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "annexb.hpp"
#include "decoder.hpp"

int main(int argc, char **argv)
{
    if (argc != 5) return 2;
    const int width = std::atoi(argv[3]);
    const int height = std::atoi(argv[4]);
    FILE *input = std::fopen(argv[1], "rb");
    FILE *output = std::fopen(argv[2], "wb");
    if (input == nullptr || output == nullptr) return 2;
    std::fseek(input, 0, SEEK_END);
    const long size = std::ftell(input);
    std::rewind(input);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (std::fread(data.data(), 1U, data.size(), input) != data.size()) return 2;
    std::fclose(input);

    auto decoder = std::make_unique<sub0h264::H264Decoder>();
    sub0h264::Vector<sub0h264::NalBounds> bounds;
    sub0h264::findNalUnits(data.data(), static_cast<uint32_t>(size), bounds);
    int frames = 0;
    int errors = 0;
    for (const auto &bound : bounds) {
        sub0h264::NalUnit nal;
        if (!sub0h264::parseNalUnit(data.data() + bound.offset, bound.size, nal)) {
            ++errors;
            continue;
        }
        const auto status = decoder->processNal(nal);
        if (status == sub0h264::DecodeStatus::Error) ++errors;
        if (status != sub0h264::DecodeStatus::FrameDecoded) continue;
        const sub0h264::Frame *const frame = decoder->currentFrame();
        for (int row = 0; row < height; ++row) std::fwrite(frame->yRow(row), 1U, width, output);
        for (int row = 0; row < height / 2; ++row) std::fwrite(frame->uRow(row), 1U, width / 2, output);
        for (int row = 0; row < height / 2; ++row) std::fwrite(frame->vRow(row), 1U, width / 2, output);
        ++frames;
    }
    std::fclose(output);
    std::printf("frames=%d errors=%d\n", frames, errors);
    return 0;
}
