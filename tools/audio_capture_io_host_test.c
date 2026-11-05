#define NP2_AUDIO_CAPTURE_HOST_TEST 1
#include <assert.h>
#include <stdio.h>
#include "../firmware/main/audio_capture_io.c"
static TaskHandle_t current=(void*)1;
static uint32_t seen_timeout;
static esp_err_t read_result;
static size_t available=512;
TaskHandle_t xTaskGetCurrentTaskHandle(void){return current;}
esp_err_t __real_i2s_channel_read(i2s_chan_handle_t h,void *p,size_t n,size_t *received,uint32_t timeout){
    (void)h;(void)p;*received=available<n ? available : n;seen_timeout=timeout;return read_result;
}
int esp_codec_dev_read(esp_codec_dev_handle_t h,void *p,int n){
    size_t got;
    current=(void*)2;
    assert(__wrap_i2s_channel_read(h,p,n,&got,1000)==read_result && seen_timeout==1000);
    current=(void*)1;
    int r=__wrap_i2s_channel_read(h,p,n,&got,1000);assert(seen_timeout==20);return r==ESP_OK ? ESP_OK : ESP_FAIL;
}
int main(void){char pcm[512];size_t got;
    assert(audio_capture_io_read((void*)1,pcm,sizeof(pcm),&got)==ESP_OK && got==512 && !atomic_load(&s_reader));
    assert(__wrap_i2s_channel_read((void*)1,pcm,sizeof(pcm),&got,1000)==ESP_OK && seen_timeout==1000);
    read_result=ESP_ERR_TIMEOUT;
    available=128;
    assert(audio_capture_io_read((void*)1,pcm,sizeof(pcm),&got)==ESP_OK && got==128 && !atomic_load(&s_reader));
    available=0;
    assert(audio_capture_io_read((void*)1,pcm,sizeof(pcm),&got)==ESP_ERR_TIMEOUT && got==0 && !atomic_load(&s_reader));
    available=3;
    assert(audio_capture_io_read((void*)1,pcm,sizeof(pcm),&got)==ESP_ERR_INVALID_SIZE && !atomic_load(&s_reader));
    assert(__wrap_i2s_channel_read((void*)1,pcm,sizeof(pcm),&got,5)==ESP_ERR_TIMEOUT && seen_timeout==5);
    puts("PASS bounded RX, codec ESP_FAIL flattening, partial PCM16, empty timeout, odd size rejection, task scope and cleanup");}
