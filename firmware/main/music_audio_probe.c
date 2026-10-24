/* Laboratory-only PCM path exercise. No Spotify/session/credential involved. */
#include "music_audio_service.h"
#include "device_control_service.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    music_pcm_converter_t converter;
    uint8_t stereo[1024];
    int16_t mono[256];
} probe_workspace_t;
static void put(uint8_t *p,int16_t value)
{ p[0]=(uint8_t)value;p[1]=(uint8_t)((uint16_t)value>>8U); }
void music_audio_run_probe(void)
{
    const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    device_control_status_t controls={0};device_control_get_status(&controls);
    music_audio_status_t status={0};music_audio_get_status(&status);
    if(!controls.ready || !controls.audio_ready || status.active)return;
    const uint8_t old_gain=status.gain;
    const size_t psram_before=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    size_t psram_allocated=0,observed=heap_caps_get_free_size(caps);
    unsigned peak=0,total=0;
    esp_err_t result=ESP_OK;
    if(observed<80U*1024U+sizeof(probe_workspace_t))return;
    probe_workspace_t *work=heap_caps_calloc(1,sizeof(*work),caps);
    if(!work)return;
    const uint32_t generation=music_audio_begin();
    (void)music_audio_set_gain(100);
    const int64_t deadline=esp_timer_get_time()+10LL*1000000;
    for(;;){
        music_audio_get_status(&status);
        if(!status.active || esp_timer_get_time()>=deadline){result=ESP_ERR_TIMEOUT;goto done;}
        if(status.ready)break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    psram_allocated=psram_before-heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    unsigned position=0;
    while(position<3U*44100U){
        unsigned frames=3U*44100U-position;if(frames>256)frames=256;
        for(unsigned i=0;i<frames;++i){
            /* 1 kHz, deliberately quiet triangle. Converter sees real stereo
             * PCM bytes, not samples written directly to the codec. */
            const unsigned phase=((position+i)*1000U)%44100U;
            const int value=phase<22050U ? -1024+(int)(phase*2048U/22050U)
                : 1024-(int)((phase-22050U)*2048U/22050U);
            put(work->stereo+4*i,(int16_t)value);put(work->stereo+4*i+2,(int16_t)value);
        }
        size_t produced=0;
        if(music_pcm_convert(&work->converter,work->stereo,frames*4,work->mono,256,&produced)!=frames*4){result=ESP_FAIL;goto done;}
        size_t sent=0;
        while(sent<produced){
            music_audio_get_status(&status);
            if(!status.active || status.generation!=generation || esp_timer_get_time()>=deadline){result=ESP_ERR_TIMEOUT;goto done;}
            sent+=music_audio_push(generation,work->mono+sent,produced-sent);
            if(sent<produced)vTaskDelay(pdMS_TO_TICKS(1));
        }
        total+=(unsigned)produced;position+=frames;
        music_audio_get_status(&status);if(status.buffered_samples>peak)peak=status.buffered_samples;
        size_t internal=heap_caps_get_free_size(caps);if(internal<observed)observed=internal;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    do{
        music_audio_get_status(&status);
        if(!status.active || esp_timer_get_time()>=deadline){result=ESP_ERR_TIMEOUT;break;}
        if(status.buffered_samples)vTaskDelay(pdMS_TO_TICKS(10));
    }while(status.buffered_samples);
done:
    music_audio_stop();
    (void)music_audio_set_gain(old_gain);
    heap_caps_free(work);
    const int64_t cleanup_deadline=esp_timer_get_time()+1000000;
    do{
        music_audio_get_status(&status);
        if(status.ready)vTaskDelay(pdMS_TO_TICKS(10));
    }while(status.ready && esp_timer_get_time()<cleanup_deadline);
    if(status.ready || observed<80U*1024U || total!=66150U)result=ESP_FAIL;
    ESP_LOGI("np2_music","probe result=%ld pcm=%u peak_buffer=%u queue_bytes=%u internal_observed=%u psram_before=%u psram_allocated=%u psram_after=%u stack_min_bytes=%u integrity=%u",
        (long)result,total,peak,262144U,(unsigned)observed,(unsigned)psram_before,(unsigned)psram_allocated,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),
        (unsigned)(uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t)),
        heap_caps_check_integrity_all(false) ? 1U:0U);
}
