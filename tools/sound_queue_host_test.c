#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "sound_melody.h"
#include "esp_err.h"
#include "music_audio_service.h"
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_CODEC_DEV_OK 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define DEVICE_CONTROL_SOUND_CHUNK_SAMPLES 256U
#define DEVICE_CONTROL_NOTIFICATION_TONE_MIN_INTERVAL_MS 350U
static unsigned writes;static bool fail_write;
static TickType_t tick=10000U;
static TickType_t xTaskGetTickCount(void) { return tick; }
static void xTaskNotifyGive(TaskHandle_t task) { assert(task); }
static int esp_codec_dev_write(void *speaker,void *samples,int bytes)
{ assert(speaker && samples && bytes>0 && bytes<=512);++writes;return fail_write ? ESP_FAIL : 0; }
static int s_lock;
static TaskHandle_t s_task=(void *)1;
static void *s_speaker=(void *)2;
static bool s_started,s_volume_pending;
static uint8_t s_requested_volume;
static uint8_t s_sound_pending,s_sound_cancel,s_sound_variants[SOUND_CATEGORY_COUNT];
static TickType_t s_sound_ticks[SOUND_CATEGORY_COUNT];
static bool s_sound_queued[SOUND_CATEGORY_COUNT];
static sound_player_t s_player;
static int s_active_sound=-1;
static struct { bool ready,audio_ready;uint8_t volume_percent; } s_status;
static music_audio_status_t music;
static unsigned music_reads;
static size_t music_count=256;
void music_audio_get_status(music_audio_status_t *out) { *out=music; }
size_t music_audio_owner_read(int16_t *out,size_t n)
{ ++music_reads;assert(n==256);for(size_t i=0;i<music_count;++i)out[i]=1234;return music_count; }
void music_audio_owner_fail(esp_err_t result) { music.result=result;music.active=false; }
#include "sound_queue_under_test.inc"
int main(void)
{
    assert(device_control_play_melody(SOUND_STARTUP,1U) == ESP_ERR_INVALID_STATE);
    s_started=true;s_status.ready=true;s_status.audio_ready=true;s_status.volume_percent=65U;
    assert(device_control_play_melody(SOUND_STARTUP,1U) == ESP_OK);
    assert(play_pending_sound_chunk() && s_active_sound == SOUND_STARTUP && writes == 1U);
    assert(device_control_play_pomodoro_alarm() == ESP_OK);
    assert(play_pending_sound_chunk() && s_active_sound == SOUND_ALARM);
    assert(device_control_play_pomodoro_alarm() == ESP_ERR_INVALID_STATE);
    assert(device_control_play_melody(SOUND_ALARM,0U) == ESP_OK);
    assert(!play_pending_sound_chunk() && s_active_sound == -1);
    assert(device_control_play_notification_tone() == ESP_OK);
    s_status.volume_percent=0U;unsigned before=writes;
    assert(!play_pending_sound_chunk() && before == writes && s_sound_pending == 0U);
    assert(device_control_play_notification_tone() == ESP_ERR_INVALID_STATE);
    s_status.volume_percent=65U;tick+=400U;
    assert(device_control_play_notification_tone() == ESP_OK);
    assert(play_pending_sound_chunk());device_control_stop_sounds();before=writes;
    assert(!play_pending_sound_chunk() && before == writes && s_sound_pending == 0U);
    tick+=400U;fail_write=true;
    assert(device_control_play_notification_tone() == ESP_OK && !play_pending_sound_chunk() && s_active_sound == -1);
    assert(device_control_play_melody(SOUND_CATEGORY_COUNT,1U) == ESP_ERR_INVALID_ARG);
    assert(device_control_play_melody(SOUND_STARTUP,4U) == ESP_ERR_INVALID_ARG);
    fail_write=false;music=(music_audio_status_t){.active=true,.ready=true};
    tick+=400;s_status.audio_ready=true;
    assert(device_control_play_pomodoro_alarm()==ESP_OK);
    assert(play_audio_chunk() && s_active_sound==SOUND_ALARM && !music_reads);
    device_control_stop_sounds();assert(play_audio_chunk() && music_reads==1);
    music.paused=true;before=writes;assert(!play_audio_chunk() && writes==before);
    music.paused=false;music_count=0;assert(play_audio_chunk());
    fail_write=true;assert(!play_audio_chunk() && !music.active && music.result==ESP_FAIL);
    wake_audio_owner();
    puts("Sound owner: chunk bound, alarm priority, rate limit, category/master/volume mute, codec failure PASS");
}
