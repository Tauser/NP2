#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "esp_err.h"
#include "notification_profile.h"
#include "cache_record.h"
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define NVS_PARTITION "fixture"
#define NOTIFICATION_NAMESPACE "notifications"
#define NOTIFICATION_SLOT_ZERO_KEY "n0"
#define NOTIFICATION_SLOT_ONE_KEY "n1"
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define ESP_RETURN_ON_ERROR(expr,...) do { esp_err_t r=(expr);if(r != ESP_OK) return r; } while(0)
typedef int nvs_handle_t;
static int s_status_lock;
static struct { bool notification_profile_valid;uint32_t notification_profile_generation;notification_profile_t notification_profile;esp_err_t notification_profile_result; } s_status;
static uint8_t blobs[2][32],staged[32];static size_t sizes[2],staged_size;static unsigned target,writes;static bool fail_commit;
static esp_err_t nvs_open_from_partition(const char *partition,const char *ns,int mode,nvs_handle_t *handle)
{ (void)partition;(void)ns;(void)mode;*handle=1;return ESP_OK; }
static void nvs_close(nvs_handle_t handle) { (void)handle; }
static esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size)
{
    (void)handle;unsigned slot=!strcmp(key,"n1");if (!sizes[slot]) return ESP_ERR_NVS_NOT_FOUND;
    if (*size<sizes[slot]) return ESP_FAIL;
    memcpy(out,blobs[slot],sizes[slot]);*size=sizes[slot];return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size)
{ (void)handle;assert(size<=32U);target=!strcmp(key,"n1");memcpy(staged,data,size);staged_size=size;return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t handle)
{ (void)handle;if (fail_commit) return ESP_FAIL;memcpy(blobs[target],staged,staged_size);sizes[target]=staged_size;++writes;return ESP_OK; }
#include "sound_storage_under_test.inc"
int main(void)
{
    notification_profile_record_t selected;
    assert(select_latest_notification_profile(&selected) == ESP_ERR_NOT_FOUND);
    cache_record_header_t h={.magic=NP2_CACHE_RECORD_MAGIC,.schema_version=1U,.header_size=sizeof(h),.generation=7U,.payload_size=3U};
    const uint8_t old[]={1U,0U,1U};h.payload_crc32=cache_record_crc32(old,sizeof(old));
    h.header_crc32=cache_record_crc32((const uint8_t *)&h,offsetof(cache_record_header_t,header_crc32));
    memcpy(blobs[1],&h,sizeof(h));memcpy(blobs[1]+sizeof(h),old,sizeof(old));sizes[1]=28U;
    refresh_notification_profile_status();assert(writes == 0U && s_status.notification_profile_valid);
    assert(!s_status.notification_profile.sound_enabled && s_status.notification_profile.melodies[SOUND_STARTUP] == 1U);
    notification_profile_t profile=s_status.notification_profile;profile.sound_enabled=true;profile.melodies[0]=0U;profile.melodies[1]=2U;profile.melodies[2]=3U;
    fail_commit=true;assert(write_notification_profile(&profile) == ESP_FAIL);fail_commit=false;
    assert(select_latest_notification_profile(&selected) == ESP_OK && selected.header.generation == 7U);
    assert(write_notification_profile(&profile) == ESP_OK && writes == 1U);
    assert(select_latest_notification_profile(&selected) == ESP_OK && selected.header.generation == 8U && selected.header.schema_version == 2U);
    assert(!memcmp(&selected.profile,&profile,sizeof(profile)));
    blobs[0][25]^=1U;
    assert(select_latest_notification_profile(&selected) == ESP_OK && selected.header.generation == 7U);
    blobs[1][0]^=1U;assert(select_latest_notification_profile(&selected) == ESP_ERR_INVALID_CRC);
    puts("Sound NVS: installed v1 switches/defaults, v2 roundtrip, failed commit, CRC generation fallback PASS");
}
