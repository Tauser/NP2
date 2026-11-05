#include "music_audio_service.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static music_pcm_queue_t s_queue = {.gain=100};
static esp_err_t s_result = ESP_OK;
static bool s_voice_reserved;
static void (*s_wake)(void);
void music_audio_bind_wake(void (*wake)(void)) { s_wake=wake; }
static void notify(void) { if(s_wake)s_wake(); }

uint32_t music_audio_begin(void)
{
    portENTER_CRITICAL(&s_lock);
    const uint32_t generation=s_voice_reserved ? 0U : music_pcm_begin(&s_queue);
    if(generation)s_result=ESP_OK;
    portEXIT_CRITICAL(&s_lock);notify();return generation;
}
void music_audio_stop(void)
{ portENTER_CRITICAL(&s_lock);s_voice_reserved=false;music_pcm_stop(&s_queue);portEXIT_CRITICAL(&s_lock);notify(); }
esp_err_t music_audio_acquire_voice(uint32_t *generation)
{
    if(!generation)return ESP_ERR_INVALID_ARG;
    *generation=0U;
    portENTER_CRITICAL(&s_lock);
    const bool available=!s_queue.active && !s_voice_reserved;
    if(available){s_voice_reserved=true;*generation=music_pcm_begin(&s_queue);s_result=ESP_OK;}
    portEXIT_CRITICAL(&s_lock);
    if(available)notify();
    return available ? ESP_OK : ESP_ERR_INVALID_STATE;
}
uint32_t music_audio_flush_voice(uint32_t generation)
{
    portENTER_CRITICAL(&s_lock);
    uint32_t next=0U;
    if(s_voice_reserved && s_queue.active && generation==s_queue.generation)
        next=music_pcm_begin(&s_queue);
    portEXIT_CRITICAL(&s_lock);notify();return next;
}
void music_audio_release_voice(uint32_t generation)
{
    portENTER_CRITICAL(&s_lock);
    if(s_voice_reserved && generation==s_queue.generation){s_voice_reserved=false;music_pcm_stop(&s_queue);}
    portEXIT_CRITICAL(&s_lock);notify();
}
void music_audio_pause(bool paused)
{ portENTER_CRITICAL(&s_lock);if(!s_voice_reserved)s_queue.paused=paused;portEXIT_CRITICAL(&s_lock);notify(); }
esp_err_t music_audio_set_gain(uint8_t percent)
{
    if(percent>100)return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);s_queue.gain=percent;portEXIT_CRITICAL(&s_lock);return ESP_OK;
}
size_t music_audio_push(uint32_t generation,const int16_t *pcm,size_t count)
{
    portENTER_CRITICAL(&s_lock);
    const size_t used=music_pcm_push(&s_queue,generation,pcm,count);
    portEXIT_CRITICAL(&s_lock);if(used)notify();return used;
}
void music_audio_get_status(music_audio_status_t *out)
{
    if(!out)return;
    portENTER_CRITICAL(&s_lock);
    *out=(music_audio_status_t){.active=s_queue.active,.paused=s_queue.paused,
        .ready=s_queue.samples!=NULL,.generation=s_queue.generation,
        .buffered_samples=(uint32_t)s_queue.count,.gain=s_queue.gain,.result=s_result};
    portEXIT_CRITICAL(&s_lock);
}
void music_audio_owner_maintain(void)
{
    int16_t *old=NULL;
    portENTER_CRITICAL(&s_lock);
    if(!s_queue.active && s_queue.samples) {
        old=s_queue.samples;s_queue.samples=NULL;s_queue.capacity=0;s_queue.count=0;s_queue.head=0;
    }
    const bool allocate=s_queue.active && !s_queue.samples && s_result==ESP_OK;
    portEXIT_CRITICAL(&s_lock);
    if(old)heap_caps_free(old);
    if(!allocate)return;
    int16_t *memory=heap_caps_malloc(MUSIC_PCM_QUEUE_SAMPLES*sizeof(int16_t),
        MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    portENTER_CRITICAL(&s_lock);
    if(s_queue.active && memory) {
        s_queue.samples=memory;s_queue.capacity=MUSIC_PCM_QUEUE_SAMPLES;memory=NULL;
    } else if(s_queue.active) {
        s_result=ESP_ERR_NO_MEM;s_voice_reserved=false;music_pcm_stop(&s_queue);
    }
    portEXIT_CRITICAL(&s_lock);
    if(memory)heap_caps_free(memory);
}
size_t music_audio_owner_read(int16_t *pcm,size_t capacity)
{
    portENTER_CRITICAL(&s_lock);
    const size_t count=music_pcm_read(&s_queue,pcm,capacity);
    portEXIT_CRITICAL(&s_lock);return count;
}
void music_audio_owner_fail(esp_err_t result)
{
    portENTER_CRITICAL(&s_lock);
    s_result=result;s_voice_reserved=false;music_pcm_stop(&s_queue);
    portEXIT_CRITICAL(&s_lock);
}
