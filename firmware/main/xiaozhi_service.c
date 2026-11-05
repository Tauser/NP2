#include "xiaozhi_service.h"
#ifdef NP2_XIAOZHI_HOST_TEST
#include "xiaozhi_host_platform.h"
#else
#include "app_state.h"
#include "google_agenda_codec.h"
#include "music_audio_service.h"
#include "network_validation_service.h"
#include "voice_capture.h"
#include "voice_resampler.h"
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "encoder/impl/esp_opus_enc.h"
#include "decoder/impl/esp_opus_dec.h"
#endif
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>

static const char *const TAG = "np2_xiaozhi";

#define BODY_CAP 4096U
#define INBOX_SLOTS 4U
#define INBOX_BYTES 640U
#define DOWN_SLOTS 96U
#define DOWN_BYTES 640U
/* Holds the whole 4 s pre-hello capture (~67 Opus frames) in one burst. */
#define UP_SLOTS 80U
#define UP_BYTES 512U
#define UPLINK_FRAME_SAMPLES (XIAOZHI_UPLINK_SAMPLE_RATE * XIAOZHI_FRAME_MS / 1000U)
#define DOWNLINK_MAX_SAMPLES 5760U /* 120 ms at 48 kHz */
#define CODEC_STACK_BYTES (40U * 1024U)
#define WS_STACK_BYTES 8192
#define ACTIVATION_WINDOW_US (5LL * 60LL * 1000000LL)
#define ACTIVATION_POLL_US (5LL * 1000000LL)
#define LISTEN_IDLE_US (20LL * 1000000LL)
#define CONTINUE_IDLE_US (5LL * 1000000LL)
#define SERVER_SILENCE_US (45LL * 1000000LL)
#define SESSION_LIMIT_US (90LL * 1000000LL)
#define DRAIN_LIMIT_US (20LL * 1000000LL)
#define INTERNAL_FLOOR (80U * 1024U)
/* Measured WSS/TLS cost is checked live; this is only the admission guess. */
#define TLS_ESTIMATE (24U * 1024U)
#define INTERNAL_EMERGENCY (INTERNAL_FLOOR + 8U * 1024U)
#define BIT_CONNECTED BIT0
#define BIT_CLOSED BIT1

/* Bounded packet ring. Owner and saturation are fixed per instance: the
 * WebSocket task produces downlink/text, the codec task produces uplink. A
 * full ring drops the newest packet and counts it; nothing blocks I/O. */
typedef struct {
    uint8_t *data;
    uint16_t length[DOWN_SLOTS];
    uint16_t slot_bytes, slots, head, count;
    uint32_t dropped;
} packet_ring_t;

typedef struct {
    google_json_t json;
    xiaozhi_ota_result_t ota;
    char *body; /* OTA/activation phase only; freed before the WebSocket */
    char request[640], headers[512], out[192];
    char url[XIAOZHI_URL_BYTES + 16U];
    char mac[18], uuid[37];
    size_t received;
    int status;
} http_work_t; /* sole worker, explicitly internal and transient */

typedef struct {
    voice_resampler_t up, down;
    int16_t capture[VOICE_CAPTURE_CHUNK], up_pcm[UPLINK_FRAME_SAMPLES * 2U];
    int16_t decoded[DOWNLINK_MAX_SAMPLES], resampled[DOWNLINK_MAX_SAMPLES];
    uint8_t packet[DOWN_BYTES], encoded[UP_BYTES];
    size_t up_fill;
} codec_work_t; /* decoded/encoded audio only: PSRAM */

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static http_work_t *s_http;
static codec_work_t *s_codec;
static char (*s_inbox)[INBOX_BYTES];
static uint16_t s_inbox_length[INBOX_SLOTS], s_inbox_head, s_inbox_count;
static char *s_assembly; /* WebSocket task only, internal */
static size_t s_assembly_length;
static bool s_assembly_overflow;
static uint8_t s_assembly_opcode;
static atomic_bool s_control_overflow;
static packet_ring_t s_down, s_up;
static EventGroupHandle_t s_events;
static esp_websocket_client_handle_t s_ws;

/* app_loop <-> worker mailbox. */
static xiaozhi_projection_t s_live;
static bool s_busy, s_stop, s_interrupt, s_start_wanted;
static int64_t s_start_deadline;
static bool s_activation_pending, s_activation_challenge, s_yield;
static int64_t s_activation_deadline, s_activation_next;

/* Worker <-> codec task. */
static atomic_bool s_codec_exit, s_capture_enabled, s_decode_busy;
static atomic_uint s_flush_generation, s_music_generation, s_capture_generation;
static uint32_t s_downlink_rate = 24000U;
static atomic_int s_codec_result;
static atomic_bool s_codec_ready;
static atomic_llong s_voice_activity;
/* Accepted application frames, including audio, are server progress. */
static atomic_llong s_server_activity;
static atomic_bool s_ws_fault;
static TaskHandle_t s_codec_task;
/* Worker only. The WebSocket endpoint/token from the last OTA check stay in RAM
 * (never flash, logs or projections) so a new tap skips that round trip. */
static bool s_cached, s_greeted;
static char s_cached_url[XIAOZHI_URL_BYTES], s_cached_token[XIAOZHI_TOKEN_BYTES];
static atomic_bool s_codec_done;
static bool s_codec_quarantined;

/* esp_audio_codec allocator hooks: Opus state is codec data, kept in PSRAM so
 * the internal floor is reserved for TLS, JSON and stacks (ADR-116). */
void *media_lib_module_malloc(const char *module, size_t size)
{ (void)module; return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void *media_lib_module_calloc(const char *module, size_t count, size_t size)
{ (void)module; return heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void *media_lib_module_realloc(const char *module, void *buffer, size_t size)
{ (void)module; return heap_caps_realloc(buffer, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void media_lib_free(void *buffer) { if (buffer) heap_caps_free(buffer); }

static void scrub(void *data, size_t n) { volatile uint8_t *p = data; while (n--) *p++ = 0; }
static int64_t now_us(void) { return esp_timer_get_time(); }

static void publish(void (*change)(xiaozhi_projection_t *, const void *), const void *arg)
{
    portENTER_CRITICAL(&s_lock);
    change(&s_live, arg); ++s_live.revision;
    portEXIT_CRITICAL(&s_lock);
    (void)app_state_request_refresh();
}
static void set_state(xiaozhi_projection_t *p, const void *arg) { p->state = *(const xiaozhi_state_t *)arg; }
static void state(xiaozhi_state_t value)
{ publish(set_state, &value); ESP_LOGI(TAG, "stage state=%u", (unsigned)value); }
static void set_user(xiaozhi_projection_t *p, const void *arg)
{ xiaozhi_copy_utf8(p->user_text, sizeof(p->user_text), arg); p->assistant_text[0] = '\0'; }
static void set_assistant(xiaozhi_projection_t *p, const void *arg)
{ xiaozhi_copy_utf8(p->assistant_text, sizeof(p->assistant_text), arg); }
static void set_emotion(xiaozhi_projection_t *p, const void *arg)
{ xiaozhi_copy_utf8(p->emotion, sizeof(p->emotion), arg); }
static void set_code(xiaozhi_projection_t *p, const void *arg)
{ xiaozhi_copy_utf8(p->activation_code, sizeof(p->activation_code), arg); }
static bool flag(bool *value, bool clear)
{
    portENTER_CRITICAL(&s_lock);
    const bool result = *value; if (clear) *value = false;
    portEXIT_CRITICAL(&s_lock);
    return result;
}

static bool ring_init(packet_ring_t *r, uint16_t slots, uint16_t bytes)
{
    memset(r, 0, sizeof(*r));
    r->data = heap_caps_calloc(slots, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    r->slots = slots; r->slot_bytes = bytes;
    return r->data != NULL;
}
static void ring_free(packet_ring_t *r)
{ portENTER_CRITICAL(&s_lock); uint8_t *d = r->data; r->data = NULL; r->count = 0; portEXIT_CRITICAL(&s_lock); heap_caps_free(d); }
static bool ring_push(packet_ring_t *r, const void *data, size_t length)
{
    bool ok = false;
    portENTER_CRITICAL(&s_lock);
    if (r->data && length && length <= r->slot_bytes && r->count < r->slots) {
        const uint16_t slot = (uint16_t)((r->head + r->count) % r->slots);
        memcpy(r->data + (size_t)slot * r->slot_bytes, data, length);
        r->length[slot] = (uint16_t)length; ++r->count; ok = true;
    } else if (r->data) ++r->dropped;
    portEXIT_CRITICAL(&s_lock);
    return ok;
}
static size_t ring_pop(packet_ring_t *r, void *out, size_t capacity)
{
    size_t n = 0U;
    portENTER_CRITICAL(&s_lock);
    if (r->data && r->count) {
        n = r->length[r->head];
        if (n <= capacity) memcpy(out, r->data + (size_t)r->head * r->slot_bytes, n); else n = 0U;
        r->head = (uint16_t)((r->head + 1U) % r->slots); --r->count;
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}
static void ring_clear(packet_ring_t *r) { portENTER_CRITICAL(&s_lock); r->count = 0U; portEXIT_CRITICAL(&s_lock); }
static uint16_t ring_count(packet_ring_t *r) { portENTER_CRITICAL(&s_lock); uint16_t n = r->count; portEXIT_CRITICAL(&s_lock); return n; }

/* ---------------- OTA check and activation (plain HTTPS) ---------------- */

static esp_err_t http_post(const char *url, const char *body)
{
    if (flag(&s_stop, false) || flag(&s_yield, false)) return ESP_ERR_INVALID_STATE;
    const int64_t deadline = now_us() + 8000000LL;
    if (!s_http->body) s_http->body = heap_caps_malloc(BODY_CAP + 1U, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_http->body) return ESP_ERR_NO_MEM;
    s_http->received = 0U; s_http->status = 0; s_http->body[0] = '\0';
    const esp_http_client_config_t config = {.url = url, .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
        .max_redirection_count = 0, .timeout_ms = 2000, .buffer_size = 1024, .buffer_size_tx = 1024,
#if CONFIG_MBEDTLS_DYNAMIC_BUFFER
        .tls_dyn_buf_strategy = HTTP_TLS_DYN_BUF_RX_STATIC,
#endif
    };
    esp_http_client_handle_t c = esp_http_client_init(&config);
    if (!c) return ESP_ERR_NO_MEM;
    const char *version = "1";
    esp_err_t r = esp_http_client_set_header(c, "Content-Type", "application/json");
    if (r == ESP_OK) r = esp_http_client_set_header(c, "Activation-Version", version);
    if (r == ESP_OK) r = esp_http_client_set_header(c, "Device-Id", s_http->mac);
    if (r == ESP_OK) r = esp_http_client_set_header(c, "Client-Id", s_http->uuid);
    if (r == ESP_OK) r = esp_http_client_set_header(c, "Accept-Language", "pt-BR");
    if (r == ESP_OK) r = esp_http_client_set_header(c, "User-Agent", "novapanel-p4-7b/np2");
    const size_t length = strlen(body);
    size_t sent = 0U;
    if (r == ESP_OK) r = esp_http_client_open(c, (int)length);
    while (r == ESP_OK && sent < length) {
        if (now_us() >= deadline || flag(&s_stop, false) || flag(&s_yield, false)) { r = ESP_ERR_TIMEOUT; break; }
        const int n = esp_http_client_write(c, body + sent, (int)(length - sent));
        if (n <= 0) r = ESP_FAIL; else sent += (size_t)n;
    }
    if (r == ESP_OK) {
        const int64_t n = esp_http_client_fetch_headers(c);
        s_http->status = esp_http_client_get_status_code(c);
        if (n < 0) r = ESP_FAIL; else if ((uint64_t)n > BODY_CAP) r = ESP_ERR_INVALID_SIZE;
    }
    while (r == ESP_OK) {
        if (now_us() >= deadline || flag(&s_stop, false) || flag(&s_yield, false)) { r = ESP_ERR_TIMEOUT; break; }
        char extra;
        const bool full = s_http->received == BODY_CAP;
        const int n = esp_http_client_read(c, full ? &extra : s_http->body + s_http->received,
                                           full ? 1 : (int)(BODY_CAP - s_http->received));
        if (n < 0) r = ESP_FAIL;
        else if (n == 0) { if (!esp_http_client_is_complete_data_received(c)) r = ESP_ERR_INVALID_RESPONSE; break; }
        else if (full) r = ESP_ERR_INVALID_SIZE;
        else { s_http->received += (size_t)n; s_http->body[s_http->received] = '\0'; }
    }
    esp_http_client_close(c); esp_http_client_cleanup(c);
    /* Never log URLs, headers or bodies: they carry the device identity/token. */
    ESP_LOGI(TAG, "http status=%d result=%s bytes=%u internal=%u", s_http->status, esp_err_to_name(r),
             (unsigned)s_http->received, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return r;
}

static esp_err_t check_version(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char elf[65] = {0}, version[32];
    (void)esp_app_get_elf_sha256(elf, sizeof(elf));
    xiaozhi_copy_utf8(version, sizeof(version), app->version);
    uint32_t flash = 0U;
    (void)esp_flash_get_size(NULL, &flash);
    if (!xiaozhi_build_system_info(s_http->request, sizeof(s_http->request), s_http->mac, s_http->uuid,
            version, elf, flash, (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)) &&
        !xiaozhi_build_system_info(s_http->request, sizeof(s_http->request), s_http->mac, s_http->uuid,
            "1.0.0", elf, flash, 0U)) return ESP_ERR_INVALID_SIZE;
    esp_err_t r = http_post(XIAOZHI_DEFAULT_OTA_URL, s_http->request);
    if (r != ESP_OK) return r;
    if (s_http->status != 200) return ESP_ERR_INVALID_RESPONSE;
    if (!xiaozhi_parse_ota(&s_http->json, s_http->body, s_http->received, &s_http->ota)) return ESP_ERR_INVALID_RESPONSE;
    ESP_LOGI(TAG, "ota activation=%u code=%u challenge=%u websocket=%u", (unsigned)s_http->ota.activation,
             (unsigned)(s_http->ota.code[0] != 0), (unsigned)(s_http->ota.challenge[0] != 0),
             (unsigned)s_http->ota.websocket);
    return ESP_OK;
}

/* ---------------- WebSocket session ---------------- */

static void inbox_push(const char *text, size_t length)
{
    portENTER_CRITICAL(&s_lock);
    if (s_inbox && length < INBOX_BYTES && s_inbox_count < INBOX_SLOTS) {
        const uint16_t slot = (uint16_t)((s_inbox_head + s_inbox_count) % INBOX_SLOTS);
        memcpy(s_inbox[slot], text, length); s_inbox[slot][length] = '\0';
        s_inbox_length[slot] = (uint16_t)length; ++s_inbox_count;
    } else s_control_overflow = true;
    portEXIT_CRITICAL(&s_lock);
}
static size_t inbox_pop(char *out)
{
    size_t n = 0U;
    portENTER_CRITICAL(&s_lock);
    if (s_inbox && s_inbox_count) {
        n = s_inbox_length[s_inbox_head];
        memcpy(out, s_inbox[s_inbox_head], n + 1U);
        s_inbox_head = (uint16_t)((s_inbox_head + 1U) % INBOX_SLOTS); --s_inbox_count;
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    const esp_websocket_event_data_t *e = data;
    if (id == WEBSOCKET_EVENT_CONNECTED) { xEventGroupSetBits(s_events, BIT_CONNECTED); return; }
    if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_CLOSED || id == WEBSOCKET_EVENT_ERROR) {
        if (id != WEBSOCKET_EVENT_CLOSED ||
            (e && e->close_status_code != 0 && e->close_status_code != 1000 && e->close_status_code != 1001))
            s_ws_fault = true;
        ESP_LOGI(TAG, "ws closed event=%ld code=%d", (long)id, e ? e->close_status_code : -1);
        if (e && id != WEBSOCKET_EVENT_CLOSED)
            ESP_LOGI(TAG, "ws failure type=%u http=%d tls=%d stack=%d verify=%d errno=%d",
                     (unsigned)e->error_handle.error_type, e->error_handle.esp_ws_handshake_status_code,
                     (int)e->error_handle.esp_tls_last_esp_err, e->error_handle.esp_tls_stack_err,
                     e->error_handle.esp_tls_cert_verify_flags, e->error_handle.esp_transport_sock_errno);
        xEventGroupSetBits(s_events, BIT_CLOSED); return;
    }
    if (id != WEBSOCKET_EVENT_DATA || !e || e->data_len < 0 ||
        (e->op_code != 0 && e->op_code != 1 && e->op_code != 2)) return;
    /* Handle both client buffer chunks and RFC6455 continuation frames. */
    if (e->payload_offset == 0 && e->op_code != 0) {
        s_assembly_length = 0U; s_assembly_overflow = false; s_assembly_opcode = e->op_code;
    }
    if (!s_assembly_opcode) { s_control_overflow = true; return; }
    const size_t cap = s_assembly_opcode == 0x1 ? INBOX_BYTES - 1U : DOWN_BYTES;
    if (s_assembly_overflow || s_assembly_length + (size_t)e->data_len > cap) s_assembly_overflow = true;
    else { memcpy(s_assembly + s_assembly_length, e->data_ptr, (size_t)e->data_len); s_assembly_length += (size_t)e->data_len; }
    if (e->payload_offset + e->data_len < e->payload_len || !e->fin) return;
    if (s_assembly_overflow) s_control_overflow = true;
    else if (s_assembly_opcode == 0x1) inbox_push(s_assembly, s_assembly_length);
    else if (!ring_push(&s_down, s_assembly, s_assembly_length)) s_control_overflow = true;
    if (!s_control_overflow && s_assembly_length) s_server_activity = now_us();
    s_assembly_opcode = 0U;
}

static void flush_playback(void)
{
    ++s_flush_generation;
    ring_clear(&s_down);
    s_music_generation = music_audio_flush_voice(s_music_generation);
}

static void codec_task(void *arg)
{
    (void)arg;
    codec_work_t *w = s_codec;
    void *encoder = NULL, *decoder = NULL;
    esp_opus_enc_config_t enc = ESP_OPUS_ENC_CONFIG_DEFAULT();
    enc.sample_rate = (int)XIAOZHI_UPLINK_SAMPLE_RATE; enc.channel = 1; enc.bitrate = ESP_OPUS_BITRATE_AUTO;
    enc.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS; enc.complexity = 3; enc.enable_vbr = true;
    esp_opus_dec_cfg_t dec = ESP_OPUS_DEC_CONFIG_DEFAULT();
    dec.sample_rate = s_downlink_rate; dec.channel = 1;
    const esp_audio_err_t er = esp_opus_enc_open(&enc, sizeof(enc), &encoder);
    const esp_audio_err_t dr = esp_opus_dec_open(&dec, sizeof(dec), &decoder);
    const bool ready = er == ESP_AUDIO_ERR_OK && dr == ESP_AUDIO_ERR_OK &&
        voice_resampler_init(&w->up, VOICE_CAPTURE_SAMPLE_RATE, XIAOZHI_UPLINK_SAMPLE_RATE) &&
        voice_resampler_init(&w->down, s_downlink_rate, VOICE_CAPTURE_SAMPLE_RATE);
    ESP_LOGI(TAG, "codec ready=%u enc=%d dec=%d down_rate=%lu internal=%u", (unsigned)ready, (int)er, (int)dr,
             (unsigned long)s_downlink_rate, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    s_codec_result = ready ? ESP_OK : ESP_FAIL;
    s_codec_ready = ready;
    uint32_t seen_flush = s_flush_generation, capture_generation = 0U, peak = 0U, frames = 0U;
    int64_t level_log = now_us();
    while (ready && !s_codec_exit) {
        bool worked = false;
        if (s_capture_enabled && capture_generation != s_capture_generation) {
            capture_generation = s_capture_generation; w->up_fill = 0U; voice_resampler_reset(&w->up);
        }
        const size_t got = s_capture_enabled ? voice_capture_read(capture_generation, w->capture, VOICE_CAPTURE_CHUNK) : 0U;
        if (got) {
            worked = true;
            uint32_t chunk_peak = 0U;
            for (size_t i = 0U; i < got; ++i) {
                const uint32_t a = (uint32_t)(w->capture[i] < 0 ? -(int32_t)w->capture[i] : w->capture[i]);
                if (a > peak) peak = a;
                if (a > chunk_peak) chunk_peak = a;
            }
            /* Conservative activity marker, not a claim of speech recognition.
             * The utterance and session still have independent hard limits. */
            if (chunk_peak >= 500U) s_voice_activity = now_us();
            w->up_fill += voice_resampler_process(&w->up, w->capture, got, w->up_pcm + w->up_fill,
                                                  sizeof(w->up_pcm) / sizeof(w->up_pcm[0]) - w->up_fill);
            while (w->up_fill >= UPLINK_FRAME_SAMPLES) {
                esp_audio_enc_in_frame_t in = {.buffer = (uint8_t *)w->up_pcm, .len = UPLINK_FRAME_SAMPLES * sizeof(int16_t)};
                esp_audio_enc_out_frame_t out = {.buffer = w->encoded, .len = sizeof(w->encoded)};
                if (esp_opus_enc_process(encoder, &in, &out) != ESP_AUDIO_ERR_OK || !out.encoded_bytes)
                    s_codec_result = ESP_FAIL;
                else if (!ring_push(&s_up, w->encoded, out.encoded_bytes)) s_codec_result = ESP_ERR_INVALID_SIZE;
                else ++frames;
                w->up_fill -= UPLINK_FRAME_SAMPLES;
                memmove(w->up_pcm, w->up_pcm + UPLINK_FRAME_SAMPLES, w->up_fill * sizeof(int16_t));
            }
        }
        if (now_us() - level_log >= 3000000LL) {
            if (s_capture_enabled) ESP_LOGI(TAG, "mic peak=%lu frames=%lu", (unsigned long)peak, (unsigned long)frames);
            peak = 0U; level_log = now_us();
        }
        if (seen_flush != s_flush_generation) { seen_flush = s_flush_generation; (void)esp_opus_dec_reset(decoder); voice_resampler_reset(&w->down); }
        s_decode_busy = true;
        const uint32_t playback_generation = s_music_generation;
        const size_t length = ring_pop(&s_down, w->packet, sizeof(w->packet));
        if (length) {
            worked = true;
            esp_audio_dec_in_raw_t raw = {.buffer = w->packet, .len = (uint32_t)length};
            esp_audio_dec_out_frame_t frame = {.buffer = (uint8_t *)w->decoded, .len = sizeof(w->decoded)};
            esp_audio_dec_info_t info = {0};
            if (esp_opus_dec_decode(decoder, &raw, &frame, &info) == ESP_AUDIO_ERR_OK) {
                const size_t samples = frame.decoded_size / sizeof(int16_t);
                size_t produced = voice_resampler_process(&w->down, w->decoded, samples, w->resampled,
                                                          sizeof(w->resampled) / sizeof(w->resampled[0]));
                size_t offset = 0U;
                /* Backpressure: wait for the owner rather than overwrite. */
                while (offset < produced && !s_codec_exit && seen_flush == s_flush_generation) {
                    const size_t used = music_audio_push(playback_generation, w->resampled + offset, produced - offset);
                    if (used) offset += used; else vTaskDelay(pdMS_TO_TICKS(10));
                }
            } else s_codec_result = ESP_ERR_INVALID_RESPONSE;
        }
        s_decode_busy = false;
        /* At least one tick: with CONFIG_FREERTOS_HZ=100, 5 ms rounds to 0 and the
         * task would only yield, starving IDLE0 and the audio owner. */
        if (!worked) vTaskDelay(1);
    }
    if (encoder) esp_opus_enc_close(encoder);
    if (decoder) (void)esp_opus_dec_close(decoder);
    ESP_LOGI(TAG, "codec exit stack_min=%u", (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    s_codec_done = true;
    vTaskSuspend(NULL);
}

static bool send_text(const char *text)
{
    return esp_websocket_client_is_connected(s_ws) &&
        esp_websocket_client_send_text(s_ws, text, (int)strlen(text), pdMS_TO_TICKS(2000)) >= 0;
}

/* The first listen keeps what the microphone buffered while connecting, so
 * speech that started right after the tap is not lost. */
static esp_err_t begin_listening(const char *session_id, bool keep_buffer)
{
    ring_clear(&s_up);
    if (!xiaozhi_build_listen_start(s_http->out, sizeof(s_http->out), session_id, XIAOZHI_LISTEN_AUTO) ||
        !send_text(s_http->out)) return XIAOZHI_ERR_SERVER_CLOSED;
    if (!keep_buffer) s_capture_generation = voice_capture_begin();
    voice_capture_status_t capture = {0};
    const int64_t deadline = now_us() + 1000000LL;
    do {
        voice_capture_get_status(&capture);
        if (capture.result != ESP_OK) return capture.result == ESP_ERR_INVALID_STATE ? XIAOZHI_ERR_AUDIO_BUSY :
            capture.result == ESP_ERR_NO_MEM ? ESP_ERR_NO_MEM : XIAOZHI_ERR_MICROPHONE;
        if (flag(&s_stop, false) || flag(&s_yield, false)) return ESP_ERR_INVALID_STATE;
        if (capture.active && capture.ready && capture.captured_samples) break;
        vTaskDelay(1);
    } while (now_us() < deadline);
    if (!capture.active || !capture.ready || !capture.captured_samples) return XIAOZHI_ERR_MICROPHONE;
    s_voice_activity = now_us();
    s_capture_enabled = true;
    state(XIAOZHI_STATE_LISTENING);
    return ESP_OK;
}
static void pause_listening(void)
{
    s_capture_enabled = false;
    voice_capture_stop();
    ring_clear(&s_up);
}

static esp_err_t run_session(void)
{
    s_server_activity = now_us();
    s_ws_fault = false;
    const char *url = s_http->ota.websocket ? s_http->ota.ws_url : XIAOZHI_DEFAULT_WS_URL;
    const char *token = s_http->ota.ws_token;
    int n = snprintf(s_http->headers, sizeof(s_http->headers), "%s%s%s%sProtocol-Version: 1\r\nDevice-Id: %s\r\nClient-Id: %s\r\n",
        token[0] ? "Authorization: " : "", token[0] && !strchr(token, ' ') ? "Bearer " : "", token, token[0] ? "\r\n" : "",
        s_http->mac, s_http->uuid);
    if (n <= 0 || (size_t)n >= sizeof(s_http->headers)) return ESP_ERR_INVALID_SIZE;
    xiaozhi_copy_utf8(s_http->url, sizeof(s_http->url), url);
    scrub(s_http->ota.ws_token, sizeof(s_http->ota.ws_token));

    const esp_websocket_client_config_t config = {.uri = s_http->url, .headers = s_http->headers,
        .crt_bundle_attach = esp_crt_bundle_attach, .buffer_size = 1024, .task_stack = WS_STACK_BYTES,
        .task_prio = 4, .task_name = "xiaozhi_ws", .disable_auto_reconnect = true,
        .network_timeout_ms = 10000, .ping_interval_sec = 20, .pingpong_timeout_sec = 60};
    s_ws = esp_websocket_client_init(&config);
    scrub(s_http->headers, sizeof(s_http->headers));
    if (!s_ws) return ESP_ERR_NO_MEM;
    xEventGroupClearBits(s_events, BIT_CONNECTED | BIT_CLOSED);
    esp_err_t r = esp_websocket_register_events(s_ws, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    if (r == ESP_OK) r = esp_websocket_client_start(s_ws);
    EventBits_t bits = 0;
    const int64_t connect_deadline = now_us() + 15000000LL;
    while (r == ESP_OK && now_us() < connect_deadline && !flag(&s_stop, false) && !flag(&s_yield, false)) {
        bits = xEventGroupWaitBits(s_events, BIT_CONNECTED | BIT_CLOSED, pdFALSE, pdFALSE, pdMS_TO_TICKS(100));
        if (bits) break;
    }
    ESP_LOGI(TAG, "ws start=%s connected=%u internal=%u largest=%u", esp_err_to_name(r), (unsigned)((bits & BIT_CONNECTED) != 0),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (r == ESP_OK && !(bits & BIT_CONNECTED))
        r = bits & BIT_CLOSED ? XIAOZHI_ERR_SERVER_CLOSED : XIAOZHI_ERR_CONNECT_TIMEOUT;
    if (r == ESP_OK && (!xiaozhi_build_hello(s_http->out, sizeof(s_http->out)) || !send_text(s_http->out))) r = ESP_FAIL;

    char session_id[XIAOZHI_SESSION_BYTES] = {0};
    char text[INBOX_BYTES];
    xiaozhi_message_t message;
    bool hello = false, tts_stopped = false, responded = false;
    unsigned stt_count = 0U, tts_count = 0U;
    const int64_t started = now_us();
    int64_t last_server = now_us(), last_activity = now_us(), drain_started = 0, quiet_since = 0;
    xiaozhi_state_t current = XIAOZHI_STATE_CONNECTING;
    size_t internal_min = SIZE_MAX;
    while (r == ESP_OK) {
        if (flag(&s_stop, false) || flag(&s_yield, false)) break;
        if (s_control_overflow) { r = XIAOZHI_ERR_CONTROL_OVERFLOW; break; }
        if (s_codec_ready && s_codec_result != ESP_OK) { r = s_codec_result; break; }
        /* A clean server close may finish normally; a transport failure or
         * policy/error close must not masquerade as a successful conversation. */
        if (xEventGroupGetBits(s_events) & BIT_CLOSED) {
            r = hello && !s_ws_fault ? ESP_OK : XIAOZHI_ERR_SERVER_CLOSED;
            break;
        }
        const int64_t now = now_us();
        const int64_t received = s_server_activity;
        if (received > last_server) last_server = received;
        if (now - started > SESSION_LIMIT_US) break;
        if (!hello && now - started > 10000000LL) { r = XIAOZHI_ERR_CONNECT_TIMEOUT; break; }
        if (now - last_server > SERVER_SILENCE_US && current != XIAOZHI_STATE_LISTENING) { r = XIAOZHI_ERR_RESPONSE_TIMEOUT; break; }
        if (current == XIAOZHI_STATE_LISTENING) {
            const int64_t voice = s_voice_activity;
            const int64_t activity = voice > last_activity ? voice : last_activity;
            if (now - activity > (responded ? CONTINUE_IDLE_US : LISTEN_IDLE_US)) { r = XIAOZHI_ERR_IDLE; break; }
            if (now - last_activity > 30000000LL) {
                r = responded ? XIAOZHI_ERR_IDLE : XIAOZHI_ERR_NO_SPEECH;
                break;
            }
            voice_capture_status_t capture = {0}; voice_capture_get_status(&capture);
            if (capture.result != ESP_OK || !capture.active) {
                r = capture.result == ESP_ERR_INVALID_STATE ? XIAOZHI_ERR_AUDIO_BUSY :
                    capture.result == ESP_ERR_NO_MEM ? ESP_ERR_NO_MEM : XIAOZHI_ERR_MICROPHONE;
                break;
            }
        }
        const size_t internal_now = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (internal_now < internal_min) internal_min = internal_now;
        if (internal_now < INTERNAL_EMERGENCY) { ESP_LOGW(TAG, "internal SRAM low=%u, ending session", (unsigned)internal_now); r = ESP_ERR_NO_MEM; break; }

        size_t length;
        while ((length = inbox_pop(text)) > 0U) {
            last_server = now_us();
            if (!xiaozhi_parse_message(&s_http->json, text, length, &message)) continue;
            switch (message.type) {
            case XIAOZHI_MSG_HELLO:
                if (hello) break;
                hello = true;
                memcpy(session_id, message.session_id, sizeof(session_id));
                s_downlink_rate = message.sample_rate;
                ESP_LOGI(TAG, "hello rate=%lu frame=%lu", (unsigned long)message.sample_rate, (unsigned long)message.frame_duration_ms);
                uint32_t audio_generation = 0U;
                if (music_audio_acquire_voice(&audio_generation) != ESP_OK) { r = XIAOZHI_ERR_AUDIO_BUSY; break; }
                s_music_generation = audio_generation;
                s_codec_exit = false; s_codec_done = false; s_codec_ready = false; s_codec_result = ESP_OK;
                if (xTaskCreatePinnedToCoreWithCaps(codec_task, "xiaozhi_codec", CODEC_STACK_BYTES, NULL, 3, &s_codec_task,
                        tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) { s_codec_task = NULL; r = ESP_ERR_NO_MEM; break; }
                const int64_t codec_deadline = now_us() + 3000000LL;
                while (!s_codec_ready && !s_codec_done && now_us() < codec_deadline && !flag(&s_stop, false)) vTaskDelay(1);
                if (!s_codec_ready) { r = s_codec_done ? s_codec_result : ESP_ERR_TIMEOUT; break; }
                s_greeted = true;
                r = begin_listening(session_id, true); current = XIAOZHI_STATE_LISTENING; last_activity = now_us();
                break;
            case XIAOZHI_MSG_STT:
                ++stt_count;
                publish(set_user, message.text); last_activity = now_us();
                if (current == XIAOZHI_STATE_LISTENING) { pause_listening(); current = XIAOZHI_STATE_THINKING; state(current); }
                break;
            case XIAOZHI_MSG_LLM: publish(set_emotion, message.emotion); break;
            case XIAOZHI_MSG_TTS_START:
                ++tts_count;
                pause_listening(); tts_stopped = false; quiet_since = 0;
                current = XIAOZHI_STATE_SPEAKING; state(current);
                break;
            case XIAOZHI_MSG_TTS_SENTENCE: publish(set_assistant, message.text); break;
            case XIAOZHI_MSG_TTS_STOP:
                if (current == XIAOZHI_STATE_SPEAKING) { tts_stopped = true; drain_started = now_us(); }
                break;
            case XIAOZHI_MSG_ALERT: publish(set_assistant, message.text); break;
            case XIAOZHI_MSG_SYSTEM:
                /* Remote reboot/system commands are never honored by the panel. */
                ESP_LOGW(TAG, "system command ignored");
                break;
            default: break;
            }
        }
        if (r != ESP_OK) break;
        if (flag(&s_interrupt, true) && current == XIAOZHI_STATE_SPEAKING) {
            if (xiaozhi_build_abort(s_http->out, sizeof(s_http->out), session_id)) (void)send_text(s_http->out);
            flush_playback(); tts_stopped = true; drain_started = now_us();
        }
        if (current == XIAOZHI_STATE_SPEAKING && tts_stopped) {
            music_audio_status_t music = {0};
            music_audio_get_status(&music);
            if (ring_count(&s_down) == 0U && !s_decode_busy && music.buffered_samples == 0U) {
                if (!quiet_since) quiet_since = now_us();
            } else quiet_since = 0;
            if (now_us() - drain_started > DRAIN_LIMIT_US) { r = XIAOZHI_ERR_PLAYBACK_TIMEOUT; break; }
            if (quiet_since && now_us() - quiet_since >= 100000LL) {
                r = begin_listening(session_id, false); current = XIAOZHI_STATE_LISTENING;
                last_activity = now_us(); tts_stopped = false; responded = true; quiet_since = 0;
            }
        }
        if (hello) {
            music_audio_status_t music = {0}; music_audio_get_status(&music);
            if (!music.active || music.generation != s_music_generation || music.result != ESP_OK) {
                r = music.result == ESP_OK ? XIAOZHI_ERR_AUDIO_BUSY : music.result; break;
            }
        }
        uint8_t packet[UP_BYTES];
        unsigned sent_packets = 0U;
        while (current == XIAOZHI_STATE_LISTENING && sent_packets++ < 8U &&
               !flag(&s_stop, false) && !flag(&s_yield, false) && (length = ring_pop(&s_up, packet, sizeof(packet))) > 0U) {
            if (esp_websocket_client_send_bin(s_ws, (const char *)packet, (int)length, pdMS_TO_TICKS(500)) < 0) {
                r = XIAOZHI_ERR_SERVER_CLOSED; break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "session detail state=%u code=%d elapsed_ms=%lu rx_gap_ms=%lu stt=%u tts=%u stopped=%u",
             (unsigned)current, (int)r, (unsigned long)((now_us() - started) / 1000LL),
             (unsigned long)((now_us() - last_server) / 1000LL), stt_count, tts_count, (unsigned)tts_stopped);
    pause_listening();
    if (hello && esp_websocket_client_is_connected(s_ws) &&
        xiaozhi_build_listen_stop(s_http->out, sizeof(s_http->out), session_id)) (void)send_text(s_http->out);
    ESP_LOGI(TAG, "session end result=%s down_drop=%lu up_drop=%lu internal_min=%u stack_min=%u", esp_err_to_name(r),
             (unsigned long)s_down.dropped, (unsigned long)s_up.dropped, (unsigned)internal_min,
             (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    return r;
}

static bool stop_codec(void)
{
    if (!s_codec_task) return true;
    s_codec_exit = true;
    for (int i = 0; i < 200; ++i) {
        if (s_codec_done && eTaskGetState(s_codec_task) == eSuspended) break;
        vTaskDelay(1);
    }
    if (!s_codec_done || eTaskGetState(s_codec_task) != eSuspended) {
        ESP_LOGE(TAG, "codec quarantined; workspace retained");
        return false;
    }
    vTaskDeleteWithCaps(s_codec_task);
    s_codec_task = NULL;
    return true;
}

static esp_err_t identity(void)
{
    uint8_t mac[6], digest[32];
    esp_err_t r = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (r != ESP_OK) return r;
    static const char salt[] = "np2-xiaozhi-client-id";
    uint8_t input[6 + sizeof(salt)];
    memcpy(input, mac, 6); memcpy(input + 6, salt, sizeof(salt));
    if (mbedtls_sha256(input, sizeof(input), digest, 0) != 0) return ESP_FAIL;
    return xiaozhi_format_mac(mac, s_http->mac) && xiaozhi_client_id(digest, s_http->uuid) ? ESP_OK : ESP_FAIL;
}

static void forget_endpoint(void)
{
    s_cached = false;
    scrub(s_cached_url, sizeof(s_cached_url));
    scrub(s_cached_token, sizeof(s_cached_token));
}

static void close_websocket(void)
{
    if (!s_ws) return;
    (void)esp_websocket_client_stop(s_ws);
    esp_websocket_client_destroy(s_ws);
    s_ws = NULL;
}

/* OTA check (and activation when required), then cache the WebSocket
 * endpoint. The 4 KiB body exists only during this phase. */
static esp_err_t refresh_endpoint(void)
{
    bool pending, challenge;
    portENTER_CRITICAL(&s_lock);
    pending = s_activation_pending; challenge = s_activation_challenge;
    portEXIT_CRITICAL(&s_lock);
    esp_err_t r = ESP_OK;
    bool recheck = true;
    if (pending && challenge) {
        snprintf(s_http->url, sizeof(s_http->url), "%sactivate", XIAOZHI_DEFAULT_OTA_URL);
        r = http_post(s_http->url, "{}");
        ESP_LOGI(TAG, "activate status=%d result=%s", s_http->status, esp_err_to_name(r));
        recheck = r == ESP_OK && s_http->status == 200;
        if (r == ESP_OK && s_http->status != 200 && s_http->status != 202) r = ESP_ERR_INVALID_RESPONSE;
    }
    if (r == ESP_OK && recheck && !flag(&s_stop, false)) r = check_version();
    const bool waiting = pending && (!recheck || r != ESP_OK);
    if (waiting || (r == ESP_OK && s_http->ota.activation)) {
        if (!waiting) publish(set_code, s_http->ota.code);
        portENTER_CRITICAL(&s_lock);
        if (!s_stop) {
            if (!s_activation_pending) s_activation_deadline = now_us() + ACTIVATION_WINDOW_US;
            s_activation_pending = true;
            if (!waiting) s_activation_challenge = s_http->ota.challenge[0] != 0;
            s_activation_next = now_us() + ACTIVATION_POLL_US;
            s_live.state = XIAOZHI_STATE_ACTIVATION; ++s_live.revision;
        }
        portEXIT_CRITICAL(&s_lock);
        r = ESP_OK; /* Pending activation is not a failed network operation. */
    } else if (r == ESP_OK) {
        portENTER_CRITICAL(&s_lock); s_activation_pending = false; portEXIT_CRITICAL(&s_lock);
        publish(set_code, "");
    }
    if (r == ESP_OK && !s_http->ota.activation && !waiting && s_http->ota.websocket) {
        memcpy(s_cached_url, s_http->ota.ws_url, sizeof(s_cached_url));
        memcpy(s_cached_token, s_http->ota.ws_token, sizeof(s_cached_token));
        s_cached = true;
    }
    if (s_http->body) { scrub(s_http->body, BODY_CAP + 1U); heap_caps_free(s_http->body); s_http->body = NULL; }
    return r;
}

esp_err_t xiaozhi_service_process(esp_err_t prerequisite)
{
    esp_err_t r = prerequisite;
    /* A timed-out codec owns its workspace until it actually suspends. Never
     * start another task against those globals or reset its exit flag. */
    if (s_codec_quarantined) {
        if (stop_codec()) {
            ring_free(&s_down); ring_free(&s_up);
            heap_caps_free(s_codec); s_codec = NULL; s_codec_quarantined = false;
        } else r = XIAOZHI_ERR_CODEC_STUCK;
    }
    const size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t needed = sizeof(http_work_t) + BODY_CAP + INBOX_SLOTS * INBOX_BYTES + WS_STACK_BYTES +
                          INTERNAL_FLOOR + TLS_ESTIMATE;
    ESP_LOGI(TAG, "admission free=%u required=%u", (unsigned)internal, (unsigned)needed);
    if (r == ESP_OK && (internal < needed ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < sizeof(http_work_t))) r = ESP_ERR_NO_MEM;
    if (r == ESP_OK) {
        s_http = heap_caps_calloc(1U, sizeof(*s_http), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s_http) r = ESP_ERR_NO_MEM;
    }
    s_inbox_head = s_inbox_count = 0U;
    s_greeted = false;
    if (r == ESP_OK) r = identity();
    const bool cached = r == ESP_OK && s_cached;
    if (cached) {
        s_http->ota = (xiaozhi_ota_result_t){.websocket = true, .ws_version = 1U};
        memcpy(s_http->ota.ws_url, s_cached_url, sizeof(s_cached_url));
        memcpy(s_http->ota.ws_token, s_cached_token, sizeof(s_cached_token));
    } else if (r == ESP_OK) {
        r = refresh_endpoint();
    }
    const bool activation_waiting = flag(&s_activation_pending, false);
    if (r == ESP_OK && !activation_waiting && !flag(&s_stop, false)) {
        s_inbox = heap_caps_calloc(INBOX_SLOTS, INBOX_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        s_assembly = heap_caps_malloc(INBOX_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        s_codec = heap_caps_calloc(1U, sizeof(*s_codec), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_events) s_events = xEventGroupCreate();
        if (!s_inbox || !s_assembly || !s_codec || !s_events ||
            !ring_init(&s_down, DOWN_SLOTS, DOWN_BYTES) || !ring_init(&s_up, UP_SLOTS, UP_BYTES)) r = ESP_ERR_NO_MEM;
        else {
            s_control_overflow = false; s_assembly_opcode = 0U; s_music_generation = 0U;
            s_capture_enabled = false; s_decode_busy = false; s_codec_ready = false;
            s_capture_generation = voice_capture_begin();
            state(XIAOZHI_STATE_CONNECTING);
            r = run_session();
        }
    }
    if ((r == XIAOZHI_ERR_SERVER_CLOSED || r == XIAOZHI_ERR_CONNECT_TIMEOUT || r == ESP_FAIL || r == ESP_ERR_TIMEOUT) &&
        cached && !s_greeted && !s_codec_task && s_music_generation == 0U &&
        !flag(&s_stop, false) && !flag(&s_yield, false)) {
        /* A stale cached token/endpoint: forget it and retry once, fresh. */
        ESP_LOGI(TAG, "cached endpoint failed: %s; refreshing", esp_err_to_name(r));
        forget_endpoint();
        close_websocket();
        s_inbox_head = s_inbox_count = 0U;
        r = refresh_endpoint();
        if (r == ESP_OK && !flag(&s_activation_pending, false)) {
            s_control_overflow = false; s_assembly_opcode = 0U;
            s_capture_generation = voice_capture_begin();
            r = run_session();
        }
    }
    if (r != ESP_OK && !s_greeted) forget_endpoint();
    close_websocket();
    const bool codec_stopped = stop_codec();
    if (!codec_stopped) { s_codec_quarantined = true; r = XIAOZHI_ERR_CODEC_STUCK; }
    music_audio_release_voice(s_music_generation);
    voice_capture_stop();
    if (!s_codec_quarantined && (flag(&s_stop, false) || flag(&s_yield, false))) r = ESP_OK;
    if (r == XIAOZHI_ERR_IDLE) r = ESP_OK;
    if (codec_stopped) {
        ring_free(&s_down); ring_free(&s_up);
        if (s_codec) { heap_caps_free(s_codec); s_codec = NULL; }
    }
    portENTER_CRITICAL(&s_lock);
    char (*inbox)[INBOX_BYTES] = s_inbox; s_inbox = NULL; s_inbox_count = 0U;
    portEXIT_CRITICAL(&s_lock);
    if (inbox) { scrub(inbox, INBOX_SLOTS * INBOX_BYTES); heap_caps_free(inbox); }
    if (s_assembly) { heap_caps_free(s_assembly); s_assembly = NULL; }
    if (s_http) {
        if (s_http->body) heap_caps_free(s_http->body);
        scrub(s_http, sizeof(*s_http)); heap_caps_free(s_http); s_http = NULL;
    }
    ESP_LOGI(TAG, "complete result=%s internal=%u stack_min=%u", esp_err_to_name(r),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    ESP_LOGI(TAG, "completion code=%d greeted=%u codec_stopped=%u", (int)r, (unsigned)s_greeted, (unsigned)codec_stopped);
    portENTER_CRITICAL(&s_lock);
    const bool continuing = r == ESP_OK && s_activation_pending && !s_stop && !s_yield;
    if (!continuing) {
        s_activation_pending = false;
        s_live.state = r == ESP_OK ? XIAOZHI_STATE_IDLE : XIAOZHI_STATE_ERROR;
        s_live.activation_code[0] = '\0';
    }
    s_live.result = r;
    s_busy = false; s_stop = false; s_interrupt = false; s_yield = false; ++s_live.revision;
    portEXIT_CRITICAL(&s_lock);
    return r;
}

/* ---------------- app_loop reducers ---------------- */

void xiaozhi_command(xiaozhi_projection_t *p, const xiaozhi_command_t *cmd)
{
    if (!p || !cmd) return;
    portENTER_CRITICAL(&s_lock);
    if (cmd->action == XIAOZHI_START) {
        if (!s_busy && !s_start_wanted && !s_activation_pending) {
            s_start_wanted = true; s_start_deadline = now_us() + 30000000LL;
            s_live.state = XIAOZHI_STATE_WAITING; s_live.result = ESP_OK;
            s_live.user_text[0] = s_live.assistant_text[0] = s_live.emotion[0] = '\0';
            ++s_live.revision;
        }
    } else if (cmd->action == XIAOZHI_STOP) {
        s_activation_pending = false;
        s_start_wanted = false;
        if (s_busy) { s_stop = true; s_live.state = XIAOZHI_STATE_STOPPING; }
        else { s_live.state = XIAOZHI_STATE_IDLE; s_live.activation_code[0] = '\0'; }
        ++s_live.revision;
    } else if (cmd->action == XIAOZHI_INTERRUPT && s_busy) {
        s_interrupt = true;
    }
    portEXIT_CRITICAL(&s_lock);
}

void xiaozhi_tick(xiaozhi_projection_t *p, bool online, bool trusted)
{
    if (!p) return;
    bool submit = false;
    portENTER_CRITICAL(&s_lock);
    if (s_activation_pending && !s_busy) {
        if (now_us() >= s_activation_deadline) {
            s_activation_pending = false; s_live.state = XIAOZHI_STATE_ERROR;
            s_live.result = XIAOZHI_ERR_ACTIVATION_TIMEOUT; s_live.activation_code[0] = '\0'; ++s_live.revision;
        } else submit = online && trusted && now_us() >= s_activation_next;
    }
    if (s_start_wanted) {
        if (!online || now_us() > s_start_deadline) {
            s_start_wanted = false; s_live.state = XIAOZHI_STATE_ERROR;
            s_live.result = online ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_STATE; ++s_live.revision;
        } else submit = trusted;
    }
    portEXIT_CRITICAL(&s_lock);
    /* Reserve the mailbox before publishing the executor job. A fast failure
     * in the worker must not be overwritten by CONNECTING after completion. */
    if (submit && !flag(&s_busy, false)) {
        portENTER_CRITICAL(&s_lock);
        s_busy = true; s_stop = false; s_interrupt = false; s_yield = false;
        portEXIT_CRITICAL(&s_lock);
        if (network_validation_service_request_xiaozhi() == ESP_OK) {
            portENTER_CRITICAL(&s_lock);
            s_start_wanted = false;
            /* Leave the activation code visible while the short poll runs. */
            if (s_busy && !s_activation_pending) s_live.state = XIAOZHI_STATE_CONNECTING;
            ++s_live.revision;
            portEXIT_CRITICAL(&s_lock);
        } else { portENTER_CRITICAL(&s_lock); s_busy = false; portEXIT_CRITICAL(&s_lock); }
    }
    portENTER_CRITICAL(&s_lock);
    if (p->revision != s_live.revision) { *p = s_live; p->busy = s_busy || s_activation_pending; }
    portEXIT_CRITICAL(&s_lock);
}

void xiaozhi_service_yield_network(void)
{
    portENTER_CRITICAL(&s_lock);
    if (s_busy) s_yield = true;
    portEXIT_CRITICAL(&s_lock);
}
