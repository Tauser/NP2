#include "np2_sub0h264.h"

#include <cstdlib>
#include <new>

#include "annexb.hpp"
#include "decoder.hpp"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace {

constexpr uint32_t kMaxCodedWidth = 640U;
constexpr uint32_t kMaxCodedHeight = 368U;
constexpr uint32_t kMaxReferenceFrames = 4U; /* Bounds the DPB allocation in PSRAM. */

} // namespace

struct np2_sub0h264 {
    sub0h264::H264Decoder decoder;
    sub0h264::NalUnit nal; /* Keeps RBSP capacity between calls, as a stream would. */
    uint16_t crop_x = 0U;
    uint16_t crop_y = 0U;
    uint16_t visible_width = 0U;
    uint16_t visible_height = 0U;
};

static int admit_sps(np2_sub0h264 *ctx)
{
    sub0h264::BitReader reader(ctx->nal.rbspData.data(),
                               static_cast<uint32_t>(ctx->nal.rbspData.size()));
    sub0h264::Sps sps;
    if (sub0h264::parseSps(reader, sps) != sub0h264::Result::Ok) return NP2_SUB0H264_ERROR;
    if (sps.width() > kMaxCodedWidth || sps.height() > kMaxCodedHeight || sps.frameMbsOnly_ != 1U ||
        sps.numRefFrames_ > kMaxReferenceFrames) {
        return NP2_SUB0H264_UNSUPPORTED;
    }
    const uint32_t crop_x = sps.frameCropping_ ? sps.cropLeft_ * 2U : 0U;
    const uint32_t crop_y = sps.frameCropping_ ? sps.cropTop_ * 2U : 0U;
    const uint32_t crop_total_x = sps.frameCropping_ ? (sps.cropLeft_ + sps.cropRight_) * 2U : 0U;
    const uint32_t crop_total_y = sps.frameCropping_ ? (sps.cropTop_ + sps.cropBottom_) * 2U : 0U;
    if (crop_total_x >= sps.width() || crop_total_y >= sps.height()) return NP2_SUB0H264_ERROR;
    ctx->crop_x = static_cast<uint16_t>(crop_x);
    ctx->crop_y = static_cast<uint16_t>(crop_y);
    ctx->visible_width = static_cast<uint16_t>(sps.width() - crop_total_x);
    ctx->visible_height = static_cast<uint16_t>(sps.height() - crop_total_y);
    return NP2_SUB0H264_CONSUMED;
}

/* Reads only the fixed head of the PPS: full parsing needs the decoder's SPS table. */
static int admit_pps(const np2_sub0h264 *ctx)
{
    sub0h264::BitReader reader(ctx->nal.rbspData.data(),
                               static_cast<uint32_t>(ctx->nal.rbspData.size()));
    (void)reader.readUev(); /* pic_parameter_set_id */
    (void)reader.readUev(); /* seq_parameter_set_id */
    (void)reader.readBit(); /* entropy_coding_mode_flag */
    (void)reader.readBit(); /* bottom_field_pic_order_in_frame_present_flag */
    if (reader.readUev() != 0U) return NP2_SUB0H264_UNSUPPORTED; /* num_slice_groups_minus1 */
    (void)reader.readUev(); /* num_ref_idx_l0_default_active_minus1 */
    (void)reader.readUev(); /* num_ref_idx_l1_default_active_minus1 */
    const uint32_t weighted_pred = reader.readBit();
    const uint32_t weighted_bipred = reader.readBits(2U);
    return (weighted_pred != 0U || weighted_bipred != 0U) ? NP2_SUB0H264_UNSUPPORTED
                                                          : NP2_SUB0H264_CONSUMED;
}

/* Each slice would otherwise be returned as a whole picture. */
static int admit_slice(const np2_sub0h264 *ctx)
{
    sub0h264::BitReader reader(ctx->nal.rbspData.data(),
                               static_cast<uint32_t>(ctx->nal.rbspData.size()));
    return reader.readUev() != 0U ? NP2_SUB0H264_UNSUPPORTED : NP2_SUB0H264_CONSUMED;
}

extern "C" np2_sub0h264_t *np2_sub0h264_create(void)
{
#ifdef ESP_PLATFORM
    void *const memory = heap_caps_malloc(sizeof(np2_sub0h264), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    void *const memory = std::malloc(sizeof(np2_sub0h264));
#endif
    return memory != nullptr ? new (memory) np2_sub0h264() : nullptr;
}

extern "C" void np2_sub0h264_destroy(np2_sub0h264_t *decoder)
{
    if (decoder == nullptr) return;
    decoder->~np2_sub0h264();
    std::free(decoder);
}

extern "C" size_t np2_sub0h264_object_bytes(void)
{
    return sizeof(np2_sub0h264);
}

extern "C" int np2_sub0h264_decode_nal(np2_sub0h264_t *decoder, const uint8_t *nal, size_t size)
{
    if (decoder == nullptr || nal == nullptr || size == 0U ||
        !sub0h264::parseNalUnit(nal, static_cast<uint32_t>(size), decoder->nal)) {
        return NP2_SUB0H264_ERROR;
    }
    int admission = NP2_SUB0H264_CONSUMED;
    switch (decoder->nal.type) {
    case sub0h264::NalType::Sps:
        admission = admit_sps(decoder);
        break;
    case sub0h264::NalType::Pps:
        admission = admit_pps(decoder);
        break;
    case sub0h264::NalType::SliceIdr:
    case sub0h264::NalType::SliceNonIdr:
        admission = admit_slice(decoder);
        break;
    default:
        break;
    }
    if (admission != NP2_SUB0H264_CONSUMED) return admission;
    switch (decoder->decoder.processNal(decoder->nal)) {
    case sub0h264::DecodeStatus::FrameDecoded:
        return NP2_SUB0H264_FRAME;
    case sub0h264::DecodeStatus::NeedMoreData:
        return NP2_SUB0H264_CONSUMED;
    default:
        return NP2_SUB0H264_ERROR;
    }
}

extern "C" int np2_sub0h264_get_frame(const np2_sub0h264_t *decoder, np2_sub0h264_frame_t *out)
{
    if (decoder == nullptr || out == nullptr) return 0;
    const sub0h264::Frame *const frame = decoder->decoder.currentFrame();
    if (frame == nullptr) return 0;
    out->y = frame->yData();
    out->u = frame->uData();
    out->v = frame->vData();
    out->width = frame->width();
    out->height = frame->height();
    out->y_stride = frame->yStride();
    out->uv_stride = frame->uvStride();
    out->crop_x = decoder->crop_x;
    out->crop_y = decoder->crop_y;
    out->visible_width = decoder->visible_width;
    out->visible_height = decoder->visible_height;
    return 1;
}
