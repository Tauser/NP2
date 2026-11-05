#include "music_audio_service.h"
#include "platform.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
static bool fail_allocate;
static unsigned allocations;
static atomic_uint wakes;
static atomic_bool pushing;
static uint32_t thread_generation;
void *heap_caps_malloc(size_t n,unsigned caps)
{
    assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));assert(n==262144);
    if(fail_allocate)return NULL;
    void *p=malloc(n);assert(p);++allocations;return p;
}
void heap_caps_free(void *p) { assert(p && allocations==1);--allocations;free(p); }
static void wake(void) { ++wakes; }
static void *produce(void *arg)
{
    (void)arg;int16_t pcm[256]={0};
    for(unsigned i=0;i<10000;++i){music_audio_push(thread_generation,pcm,256);pushing=true;}
    return NULL;
}
int main(void)
{
    music_audio_status_t status;music_audio_bind_wake(wake);music_audio_owner_maintain();
    music_audio_get_status(&status);assert(!status.active && !status.ready && !allocations);
    uint32_t first=music_audio_begin();music_audio_get_status(&status);assert(status.active && !status.ready && !allocations);
    fail_allocate=true;music_audio_owner_maintain();music_audio_get_status(&status);
    assert(!status.active && status.result==ESP_ERR_NO_MEM && !allocations);
    fail_allocate=false;uint32_t generation=music_audio_begin();assert(generation!=first);
    music_audio_owner_maintain();music_audio_get_status(&status);assert(status.ready && allocations==1);
    int16_t in[256],out[256];for(unsigned i=0;i<256;++i)in[i]=1000;
    assert(!music_audio_push(first,in,256));assert(music_audio_push(generation,in,256)==256);
    music_audio_pause(true);assert(!music_audio_owner_read(out,256));
    music_audio_pause(false);assert(music_audio_set_gain(25)==ESP_OK);
    assert(music_audio_owner_read(out,256)==256 && out[0]==250 && out[255]==250);
    assert(music_audio_set_gain(101)==ESP_ERR_INVALID_ARG);
    music_audio_stop();assert(allocations==1);music_audio_owner_maintain();assert(!allocations);
    thread_generation=music_audio_begin();music_audio_owner_maintain();
    pthread_t producer;assert(!pthread_create(&producer,NULL,produce,NULL));
    while(!pushing){}
    music_audio_stop();music_audio_owner_maintain();pthread_join(producer,NULL);assert(!allocations);
    generation=music_audio_begin();music_audio_owner_maintain();
    music_audio_stop();uint32_t newer=music_audio_begin();music_audio_owner_maintain();
    assert(allocations==1 && !music_audio_push(generation,in,256) && newer!=generation);
    music_audio_owner_fail(ESP_FAIL);music_audio_get_status(&status);
    assert(!status.active && status.result==ESP_FAIL);
    music_audio_owner_maintain();assert(!allocations && wakes>5);
    uint32_t voice=0U;
    generation=music_audio_begin();
    assert(music_audio_acquire_voice(&voice)==ESP_ERR_INVALID_STATE && !voice);
    music_audio_stop();assert(music_audio_acquire_voice(&voice)==ESP_OK && voice);
    music_audio_owner_maintain();assert(music_audio_begin()==0U);
    assert(music_audio_push(voice,in,256)==256);
    uint32_t next=music_audio_flush_voice(voice);assert(next && next!=voice);
    assert(!music_audio_push(voice,in,256));music_audio_release_voice(voice);
    music_audio_get_status(&status);assert(status.active && status.generation==next);
    music_audio_stop();generation=music_audio_begin();music_audio_release_voice(next);
    music_audio_get_status(&status);assert(status.active && status.generation==generation);
    music_audio_stop();music_audio_owner_maintain();assert(!allocations);
    puts("Music service: lazy PSRAM-only allocation, no fallback, failure/retry, owner free, concurrent stop/push, generations/pause/gain/wake PASS");
}
