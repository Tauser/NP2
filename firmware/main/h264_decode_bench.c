/*
 * Laboratory-only H.264 decoder comparison (ADR-121). OFF in normal builds.
 *
 * Decodes one embedded Annex B sample with Sub0h264 and edge264, on the running
 * panel with LVGL and network active. No network, camera, credential or
 * persistence is involved; only counters and hashes are logged. The MD5 covers
 * the cropped I420 planes, so it can be compared with FFmpeg on the PC.
 */
#include "h264_decode_bench.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "camera_stream_service.h"
#include "edge264.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i420_rgb565.h"
#include "np2_sub0h264.h"

#define BENCH_SETTLE_MS 45000U
#define BENCH_STACK_BYTES (32U * 1024U) /* Measured peak: about 21 KiB. */
/* edge264 measured 1.58 fps on the 512 kbit/s sample (ADR-121); raise to 2 to repeat it. */
#define BENCH_DECODER_COUNT 1U
#define BENCH_PRIORITY 3U /* Below LVGL (4); the panel keeps its normal priority. */
#define BENCH_MAX_SLICES 2048U
#define BENCH_PASS_DEADLINE_US (180LL * 1000000LL)
/* Below the 80 KiB product floor on purpose: this bounded run measures how much
 * SRAM the decoder needs, and min_internal reports any violation of that floor. */
#define BENCH_INTERNAL_FLOOR_BYTES (64U * 1024U)
#define BENCH_PSRAM_MALLOC_THRESHOLD 1024U
#define BENCH_INTERNAL_ADMISSION_BYTES (96U * 1024U + BENCH_STACK_BYTES)
#define BENCH_FRAME_BUDGET_BYTES (12U * 1024U * 1024U)
#define BENCH_SAMPLE_PADDING 64U
#define BENCH_RETRY_LIMIT 4U
#define BENCH_FLUSH_LIMIT 64U

extern const uint8_t s_sample_start[] asm("_binary_h264_bench_sample_h264_start");
extern const uint8_t s_sample_end[] asm("_binary_h264_bench_sample_h264_end");

static const char *const TAG = "h264_bench";

typedef struct {
    const uint8_t *y, *u, *v;
    uint16_t width, height, y_stride, uv_stride;
} bench_frame_t;

enum { DECODE_OK = 0, DECODE_NEEDS_DRAIN = 1, DECODE_ERROR = -1, DECODE_UNSUPPORTED = -2 };

typedef struct {
    const char *name;
    void *(*create)(void);
    void (*destroy)(void *ctx);
    int (*decode)(void *ctx, const uint8_t *nal, size_t size);
    bool (*next_frame)(void *ctx, bench_frame_t *out);
    bool (*flush)(void *ctx); /* True while more frames may be released. */
} bench_decoder_t;

typedef struct {
    uint8_t *sample;
    size_t sample_size;
    uint16_t *rgb;
    uint32_t *slice_us;
    uint32_t *convert_us;
} bench_buffers_t;

typedef struct {
    const char *stop;
    uint32_t nals, slices, frames, errors, unsupported, retries;
    uint32_t width, height;
    uint64_t decode_us, convert_us;
    uint32_t min_internal;
    uint8_t md5[ESP_ROM_MD5_DIGEST_LEN];
} bench_result_t;

/* ---- Sub0h264 ---- */
typedef struct {
    np2_sub0h264_t *decoder;
    bool frame_pending;
} sub0_ctx_t;

static void *sub0_create(void)
{
    sub0_ctx_t *const ctx = heap_caps_calloc(1U, sizeof(*ctx), MALLOC_CAP_8BIT);
    if (ctx == NULL) return NULL;
    ctx->decoder = np2_sub0h264_create();
    if (ctx->decoder == NULL) {
        heap_caps_free(ctx);
        return NULL;
    }
    return ctx;
}

static void sub0_destroy(void *opaque)
{
    sub0_ctx_t *const ctx = opaque;
    np2_sub0h264_destroy(ctx->decoder);
    heap_caps_free(ctx);
}

static int sub0_decode(void *opaque, const uint8_t *nal, size_t size)
{
    sub0_ctx_t *const ctx = opaque;
    const int result = np2_sub0h264_decode_nal(ctx->decoder, nal, size);
    if (result < 0) return DECODE_ERROR;
    if (result > 0) ctx->frame_pending = true;
    return DECODE_OK;
}

static bool sub0_next_frame(void *opaque, bench_frame_t *out)
{
    sub0_ctx_t *const ctx = opaque;
    if (!ctx->frame_pending) return false;
    ctx->frame_pending = false;
    np2_sub0h264_frame_t frame;
    if (np2_sub0h264_get_frame(ctx->decoder, &frame) == 0) return false;
    *out = (bench_frame_t){
        .y = frame.y, .u = frame.u, .v = frame.v,
        .width = frame.width, .height = frame.height,
        .y_stride = frame.y_stride, .uv_stride = frame.uv_stride,
    };
    return true;
}

static bool sub0_flush(void *opaque)
{
    (void)opaque;
    return false; /* Outputs every frame on decode; holds nothing back. */
}

/* ---- edge264 ---- */
/* ESP-IDF has no sysconf. edge264 only asks it for the CPU count when n_threads is
 * negative; the bench always passes 0 (single-thread mode), so this never runs. */
long sysconf(int name)
{
    (void)name;
    errno = ENOSYS;
    return -1;
}

static size_t s_edge_frame_bytes;
static uint32_t s_edge_frame_allocations;
static uint32_t s_edge_frame_failures;

static void edge_alloc(void **samples, unsigned samples_size, void **mbs, unsigned mbs_size,
                       int errno_on_fail, void *arg)
{
    (void)errno_on_fail;
    (void)arg;
    *samples = NULL;
    *mbs = NULL;
    const size_t bytes = (size_t)samples_size + mbs_size;
    if (s_edge_frame_bytes + bytes <= BENCH_FRAME_BUDGET_BYTES) {
        *samples = heap_caps_aligned_alloc(64U, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (*samples == NULL) {
        ++s_edge_frame_failures;
        return;
    }
    *mbs = (uint8_t *)*samples + samples_size;
    s_edge_frame_bytes += bytes;
    ++s_edge_frame_allocations;
}

static void edge_free(void *samples, void *mbs, void *arg)
{
    (void)mbs;
    (void)arg;
    heap_caps_free(samples);
}

static void *edge_create(void)
{
    s_edge_frame_bytes = 0U;
    s_edge_frame_allocations = 0U;
    s_edge_frame_failures = 0U;
    return edge264_alloc(0, NULL, NULL, 0, edge_alloc, edge_free, NULL);
}

static void edge_destroy(void *ctx)
{
    Edge264Decoder *decoder = ctx;
    edge264_free(&decoder);
}

static int edge_decode(void *ctx, const uint8_t *nal, size_t size)
{
    const int result = edge264_decode_NAL(ctx, nal, nal + size, NULL, NULL);
    if (result == 0) return DECODE_OK;
    if (result == ENOBUFS) return DECODE_NEEDS_DRAIN;
    return result == ENOTSUP ? DECODE_UNSUPPORTED : DECODE_ERROR;
}

static bool edge_next_frame(void *ctx, bench_frame_t *out)
{
    Edge264Frame frame;
    if (edge264_get_frame(ctx, &frame, 0) != 0) return false;
    *out = (bench_frame_t){
        .y = frame.samples[0], .u = frame.samples[1], .v = frame.samples[2],
        .width = (uint16_t)frame.width_Y, .height = (uint16_t)frame.height_Y,
        .y_stride = (uint16_t)frame.stride_Y, .uv_stride = (uint16_t)frame.stride_C,
    };
    return true;
}

static bool edge_flush(void *ctx)
{
    static const uint8_t empty;
    return edge264_decode_NAL(ctx, &empty, &empty, NULL, NULL) != ENODATA;
}

static bool s_psram_vectors;

static const bench_decoder_t s_decoders[] = {
    {"sub0h264", sub0_create, sub0_destroy, sub0_decode, sub0_next_frame, sub0_flush},
    {"edge264", edge_create, edge_destroy, edge_decode, edge_next_frame, edge_flush},
};

/* ---- Frame work outside the decode timer ---- */
/* The same converter and build flags as the camera service. */
static void convert_frame(const bench_frame_t *frame, uint16_t width, uint16_t height, uint16_t *rgb)
{
    const i420_rgb565_source_t source = {
        .y = frame->y, .u = frame->u, .v = frame->v,
        .y_stride = frame->y_stride, .uv_stride = frame->uv_stride,
        .width = width, .height = height,
    };
    (void)i420_rgb565_convert(&source, rgb);
}

static void hash_plane(md5_context_t *md5, const uint8_t *plane, uint16_t stride,
                       uint16_t width, uint16_t height)
{
    for (uint16_t row = 0U; row < height; ++row) {
        esp_rom_md5_update(md5, plane + (size_t)row * stride, width);
    }
}

static const uint8_t *find_start_code(const uint8_t *cursor, const uint8_t *end)
{
    for (; cursor + 3 <= end; ++cursor) {
        if (cursor[0] == 0U && cursor[1] == 0U && cursor[2] == 1U) return cursor;
    }
    return end;
}

static void take_frames(const bench_decoder_t *decoder, void *ctx, bool measured,
                        md5_context_t *md5, const bench_buffers_t *buffers,
                        bench_result_t *result)
{
    bench_frame_t frame;
    while (decoder->next_frame(ctx, &frame)) {
        /* Hashing the panel's frame size also crops the 368-line coded picture. */
        const uint16_t width = frame.width < CAMERA_STREAM_MAX_WIDTH ?
                               frame.width : (uint16_t)CAMERA_STREAM_MAX_WIDTH;
        const uint16_t height = frame.height < CAMERA_STREAM_MAX_HEIGHT ?
                                frame.height : (uint16_t)CAMERA_STREAM_MAX_HEIGHT;
        if (measured) {
            const int64_t begin = esp_timer_get_time();
            convert_frame(&frame, width, height, buffers->rgb);
            const uint32_t elapsed = (uint32_t)(esp_timer_get_time() - begin);
            result->convert_us += elapsed;
            if (result->frames < BENCH_MAX_SLICES) buffers->convert_us[result->frames] = elapsed;
            hash_plane(md5, frame.y, frame.y_stride, width, height);
            hash_plane(md5, frame.u, frame.uv_stride, width / 2U, height / 2U);
            hash_plane(md5, frame.v, frame.uv_stride, width / 2U, height / 2U);
        }
        result->width = width;
        result->height = height;
        ++result->frames;
    }
}

static void run_pass(const bench_decoder_t *decoder, void *ctx, bool measured,
                     const bench_buffers_t *buffers, bench_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->stop = "complete";
    result->min_internal = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    md5_context_t md5;
    esp_rom_md5_init(&md5);
    const uint8_t *const end = buffers->sample + buffers->sample_size;
    const int64_t deadline = esp_timer_get_time() + BENCH_PASS_DEADLINE_US;
    const uint8_t *code = find_start_code(buffers->sample, end);
    while (code < end) {
        const uint8_t *const nal = code + 3;
        const uint8_t *const next = find_start_code(nal, end);
        const uint8_t *nal_end = next;
        while (nal_end > nal && nal_end[-1] == 0U) --nal_end; /* trailing_zero_8bits */
        code = next;
        if (nal_end == nal) continue;

        const uint8_t type = nal[0] & 0x1FU;
        const bool is_slice = type == 1U || type == 5U;
        uint32_t elapsed = 0U;
        int status = DECODE_OK;
        for (uint32_t attempt = 0U; attempt < BENCH_RETRY_LIMIT; ++attempt) {
            const int64_t begin = esp_timer_get_time();
            status = decoder->decode(ctx, nal, (size_t)(nal_end - nal));
            elapsed += (uint32_t)(esp_timer_get_time() - begin);
            if (status != DECODE_NEEDS_DRAIN) break;
            ++result->retries;
            take_frames(decoder, ctx, measured, &md5, buffers, result);
        }
        ++result->nals;
        result->decode_us += elapsed;
        if (status == DECODE_UNSUPPORTED) ++result->unsupported;
        else if (status != DECODE_OK) ++result->errors;
        if (is_slice) {
            if (result->slices < BENCH_MAX_SLICES) buffers->slice_us[result->slices] = elapsed;
            ++result->slices;
        }
        take_frames(decoder, ctx, measured, &md5, buffers, result);

        const uint32_t internal =
            (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (internal < result->min_internal) result->min_internal = internal;
        if (internal < BENCH_INTERNAL_FLOOR_BYTES) {
            result->stop = "internal-floor";
            break;
        }
        if (esp_timer_get_time() >= deadline) {
            result->stop = "deadline";
            break;
        }
        if (is_slice) vTaskDelay(1U); /* Never starve the idle tasks and their watchdog. */
    }
    for (uint32_t i = 0U; i < BENCH_FLUSH_LIMIT && decoder->flush(ctx); ++i) {
        take_frames(decoder, ctx, measured, &md5, buffers, result);
    }
    take_frames(decoder, ctx, measured, &md5, buffers, result);
    esp_rom_md5_final(result->md5, &md5);
}

static int compare_u32(const void *a, const void *b)
{
    const uint32_t left = *(const uint32_t *)a;
    const uint32_t right = *(const uint32_t *)b;
    return left < right ? -1 : left > right;
}

static uint32_t percentile(const uint32_t *sorted, uint32_t count, uint32_t percent)
{
    return count == 0U ? 0U : sorted[(uint64_t)(count - 1U) * percent / 100U];
}

static void report(const bench_decoder_t *decoder, const bench_buffers_t *buffers,
                   const bench_result_t *result, size_t psram_used)
{
    const uint32_t slices = result->slices < BENCH_MAX_SLICES ? result->slices : BENCH_MAX_SLICES;
    const uint32_t frames = result->frames < BENCH_MAX_SLICES ? result->frames : BENCH_MAX_SLICES;
    qsort(buffers->slice_us, slices, sizeof(uint32_t), compare_u32);
    qsort(buffers->convert_us, frames, sizeof(uint32_t), compare_u32);
    const uint64_t total = result->decode_us + result->convert_us;
    const uint32_t decode_fps_x100 = result->decode_us == 0U ? 0U :
        (uint32_t)((uint64_t)result->frames * 100000000ULL / result->decode_us);
    const uint32_t total_fps_x100 = total == 0U ? 0U :
        (uint32_t)((uint64_t)result->frames * 100000000ULL / total);
    char digest[ESP_ROM_MD5_DIGEST_LEN * 2U + 1U];
    for (size_t i = 0U; i < ESP_ROM_MD5_DIGEST_LEN; ++i) {
        static const char hex[] = "0123456789abcdef";
        digest[i * 2U] = hex[result->md5[i] >> 4U];
        digest[i * 2U + 1U] = hex[result->md5[i] & 0x0FU];
    }
    digest[ESP_ROM_MD5_DIGEST_LEN * 2U] = '\0';
    ESP_LOGI(TAG, "result decoder=%s stop=%s size=%" PRIu32 "x%" PRIu32 " nals=%" PRIu32
             " slices=%" PRIu32 " frames=%" PRIu32 " errors=%" PRIu32 " unsupported=%" PRIu32
             " retries=%" PRIu32, decoder->name, result->stop, result->width, result->height,
             result->nals, result->slices, result->frames, result->errors,
             result->unsupported, result->retries);
    ESP_LOGI(TAG, "timing decoder=%s decode_fps_x100=%" PRIu32 " with_rgb565_fps_x100=%" PRIu32
             " slice_us_p50=%" PRIu32 " p95=%" PRIu32 " p99=%" PRIu32 " max=%" PRIu32
             " rgb565_us_p50=%" PRIu32 " p95=%" PRIu32, decoder->name, decode_fps_x100,
             total_fps_x100, percentile(buffers->slice_us, slices, 50U),
             percentile(buffers->slice_us, slices, 95U), percentile(buffers->slice_us, slices, 99U),
             percentile(buffers->slice_us, slices, 100U),
             percentile(buffers->convert_us, frames, 50U),
             percentile(buffers->convert_us, frames, 95U));
    ESP_LOGI(TAG, "memory decoder=%s psram_vectors=%d md5=%s min_internal=%" PRIu32 " psram_used=%u stack_min_bytes=%u",
             decoder->name, s_psram_vectors ? 1 : 0, digest, result->min_internal, (unsigned)psram_used,
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static void bench_worker(void *arg)
{
    (void)arg;
    const size_t sample_size = (size_t)(s_sample_end - s_sample_start);
    bench_buffers_t buffers = {
        .sample = heap_caps_calloc(1U, sample_size + BENCH_SAMPLE_PADDING,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .sample_size = sample_size,
        .rgb = heap_caps_malloc(CAMERA_STREAM_RGB565_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .slice_us = heap_caps_calloc(BENCH_MAX_SLICES, sizeof(uint32_t),
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
        .convert_us = heap_caps_calloc(BENCH_MAX_SLICES, sizeof(uint32_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    };
    bench_result_t *const result = heap_caps_malloc(sizeof(*result), MALLOC_CAP_8BIT);
    if (buffers.sample == NULL || buffers.rgb == NULL || buffers.slice_us == NULL ||
        buffers.convert_us == NULL || result == NULL) {
        ESP_LOGE(TAG, "buffers unavailable");
    } else {
        /* A camera stream arrives in PSRAM, not through the flash cache. */
        memcpy(buffers.sample, s_sample_start, sample_size);
        ESP_LOGI(TAG, "sample bytes=%u sha256=%s sub0h264_object_bytes=%u",
                 (unsigned)sample_size, NP2_H264_BENCH_SAMPLE_SHA256,
                 (unsigned)np2_sub0h264_object_bytes());
        /* Default malloc keeps the decoder's small vectors in SRAM; the second round
         * moves them to PSRAM, the placement that would respect the product floor. */
        for (int variant = 0; variant < 2; ++variant) {
            s_psram_vectors = variant == 1;
            heap_caps_malloc_extmem_enable(s_psram_vectors ? BENCH_PSRAM_MALLOC_THRESHOLD :
                                           CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
            for (size_t i = 0U; i < BENCH_DECODER_COUNT; ++i) {
                const bench_decoder_t *const decoder = &s_decoders[i];
                const size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                void *const warm = decoder->create();
                if (warm == NULL) {
                    ESP_LOGE(TAG, "create failed decoder=%s", decoder->name);
                    continue;
                }
                /* The warm pass primes the caches, then a fresh decoder is timed. */
                run_pass(decoder, warm, false, &buffers, result);
                decoder->destroy(warm);
                void *const ctx = decoder->create();
                if (ctx == NULL) {
                    ESP_LOGE(TAG, "create failed decoder=%s", decoder->name);
                    continue;
                }
                run_pass(decoder, ctx, true, &buffers, result);
                const size_t psram_used = psram_before - heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                report(decoder, &buffers, result, psram_used);
                decoder->destroy(ctx);
                if (decoder->create == edge_create) {
                    ESP_LOGI(TAG, "edge264_frames allocations=%" PRIu32 " bytes=%u failures=%" PRIu32,
                             s_edge_frame_allocations, (unsigned)s_edge_frame_bytes,
                             s_edge_frame_failures);
                }
            }
        }
        heap_caps_malloc_extmem_enable(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL);
    }
    const bool integrity = heap_caps_check_integrity_all(false);
    heap_caps_free(result);
    heap_caps_free(buffers.convert_us);
    heap_caps_free(buffers.slice_us);
    heap_caps_free(buffers.rgb);
    heap_caps_free(buffers.sample);
    ESP_LOGI(TAG, "done integrity=%d stack_min_bytes=%u internal_free=%u", integrity ? 1 : 0,
             (unsigned)uxTaskGetStackHighWaterMark(NULL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    vTaskDelete(NULL);
}

static void bench_starter(void *arg)
{
    (void)arg;
    /* The stack is only reserved after boot, so it cannot disturb start-up TLS. */
    vTaskDelay(pdMS_TO_TICKS(BENCH_SETTLE_MS));
    const size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (internal < BENCH_INTERNAL_ADMISSION_BYTES || largest < BENCH_STACK_BYTES ||
        xTaskCreate(bench_worker, "h264_bench", BENCH_STACK_BYTES, NULL, BENCH_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "not admitted internal_free=%u largest=%u", (unsigned)internal, (unsigned)largest);
    }
    vTaskDelete(NULL);
}

void h264_decode_bench_start(void)
{
    if (xTaskCreate(bench_starter, "h264_bench_wait", 3072U, NULL, 2U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "starter task unavailable");
    }
}
