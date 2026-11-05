#include "voice_capture.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int16_t *s_ring;
static size_t s_head, s_count;
static uint32_t s_generation, s_dropped, s_captured;
static bool s_active;
static esp_err_t s_result = ESP_OK;
static void (*s_wake)(void);

void voice_capture_bind_wake(void (*wake)(void)) { s_wake = wake; }
static void notify(void) { if (s_wake) s_wake(); }

uint32_t voice_capture_begin(void)
{
    portENTER_CRITICAL(&s_lock);
    s_active = true; s_head = 0U; s_count = 0U; s_dropped = 0U; s_captured = 0U; s_result = ESP_OK;
    if (++s_generation == 0U) s_generation = 1U;
    const uint32_t generation = s_generation;
    portEXIT_CRITICAL(&s_lock);
    notify();
    return generation;
}

void voice_capture_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    s_active = false; s_count = 0U; s_head = 0U;
    if (++s_generation == 0U) s_generation = 1U;
    portEXIT_CRITICAL(&s_lock);
    notify();
}

size_t voice_capture_read(uint32_t generation, int16_t *out, size_t capacity)
{
    if (!out) return 0U;
    portENTER_CRITICAL(&s_lock);
    size_t n = 0U;
    if (s_active && s_ring && generation == s_generation) {
        n = capacity < s_count ? capacity : s_count;
        if (n > VOICE_CAPTURE_CHUNK) n = VOICE_CAPTURE_CHUNK;
        for (size_t i = 0U; i < n; ++i) out[i] = s_ring[(s_head + i) % VOICE_CAPTURE_RING_SAMPLES];
        s_head = (s_head + n) % VOICE_CAPTURE_RING_SAMPLES; s_count -= n;
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

void voice_capture_get_status(voice_capture_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_lock);
    *out = (voice_capture_status_t){.active = s_active, .ready = s_ring != NULL, .generation = s_generation,
        .buffered_samples = (uint32_t)s_count, .dropped_samples = s_dropped,
        .captured_samples = s_captured, .result = s_result};
    portEXIT_CRITICAL(&s_lock);
}

void voice_capture_owner_maintain(void)
{
    int16_t *old = NULL;
    portENTER_CRITICAL(&s_lock);
    if (!s_active && s_ring) { old = s_ring; s_ring = NULL; }
    const bool allocate = s_active && !s_ring && s_result == ESP_OK;
    portEXIT_CRITICAL(&s_lock);
    if (old) heap_caps_free(old);
    if (!allocate) return;
    /* Decoded audio only; JSON/TLS stay internal (ADR-105 exception class). */
    int16_t *memory = heap_caps_malloc(VOICE_CAPTURE_RING_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    portENTER_CRITICAL(&s_lock);
    if (s_active && memory) { s_ring = memory; memory = NULL; s_head = 0U; s_count = 0U; }
    else if (s_active) { s_result = ESP_ERR_NO_MEM; s_active = false; }
    portEXIT_CRITICAL(&s_lock);
    if (memory) heap_caps_free(memory);
}

bool voice_capture_owner_wanted(void)
{
    portENTER_CRITICAL(&s_lock);
    const bool wanted = s_active && s_ring != NULL;
    portEXIT_CRITICAL(&s_lock);
    return wanted;
}

void voice_capture_owner_push(uint32_t generation, const int16_t *pcm, size_t count)
{
    if (!pcm) return;
    portENTER_CRITICAL(&s_lock);
    if (s_active && s_ring && generation == s_generation) {
        if (count > VOICE_CAPTURE_CHUNK) count = VOICE_CAPTURE_CHUNK;
        s_captured += count;
        for (size_t i = 0U; i < count; ++i) {
            if (s_count == VOICE_CAPTURE_RING_SAMPLES) {
                s_head = (s_head + 1U) % VOICE_CAPTURE_RING_SAMPLES; --s_count; ++s_dropped;
            }
            s_ring[(s_head + s_count) % VOICE_CAPTURE_RING_SAMPLES] = pcm[i];
            ++s_count;
        }
    }
    portEXIT_CRITICAL(&s_lock);
}

void voice_capture_owner_fail(esp_err_t result)
{
    portENTER_CRITICAL(&s_lock);
    s_result = result; s_active = false; s_count = 0U;
    portEXIT_CRITICAL(&s_lock);
}
