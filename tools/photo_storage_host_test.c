#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "esp_err.h"
#include "device_control_profile.h"
#include "cache_record.h"
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define NVS_PARTITION "fixture"
#define DEVICE_CONTROL_NAMESPACE "notifications"
#define DEVICE_CONTROL_SLOT_ZERO_KEY "ctl0"
#define DEVICE_CONTROL_SLOT_ONE_KEY "ctl1"
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define ESP_RETURN_ON_ERROR(expr,...) do { esp_err_t r=(expr);if(r != ESP_OK) return r; } while(0)
typedef int nvs_handle_t;
static int s_status_lock;
static struct { bool device_control_profile_valid;uint32_t device_control_profile_generation;device_control_profile_t device_control_profile;esp_err_t device_control_profile_result; } s_status;
static uint8_t blobs[2][48],staged[48];static size_t sizes[2],staged_size;static unsigned target,writes;static bool fail_commit;
static esp_err_t nvs_open_from_partition(const char *partition,const char *ns,int mode,nvs_handle_t *handle)
{ (void)partition;(void)ns;(void)mode;*handle=1;return ESP_OK; }
static void nvs_close(nvs_handle_t handle) { (void)handle; }
static esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size)
{
    (void)handle;unsigned slot=!strcmp(key,"ctl1");if (!sizes[slot]) return ESP_ERR_NVS_NOT_FOUND;
    if (*size<sizes[slot]) return ESP_FAIL;
    memcpy(out,blobs[slot],sizes[slot]);*size=sizes[slot];return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size)
{ (void)handle;assert(size<=48U);target=!strcmp(key,"ctl1");memcpy(staged,data,size);staged_size=size;return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t handle)
{ (void)handle;if (fail_commit) return ESP_FAIL;memcpy(blobs[target],staged,staged_size);sizes[target]=staged_size;++writes;return ESP_OK; }
#include "photo_storage_under_test.inc"
int main(void)
{
    device_control_profile_record_t selected;
    assert(select_latest_device_control_profile(&selected)==ESP_ERR_NOT_FOUND);
    for(unsigned size=2;size<=6;++size){
        if(size==5) continue;
        memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));
        cache_record_header_t h={.magic=NP2_CACHE_RECORD_MAGIC,.schema_version=1,.header_size=sizeof(h),.generation=7,.payload_size=size};
        const uint8_t old[]={26,100,1,PHOTO_CONFIG_DEFAULT,28,30};
        h.payload_crc32=cache_record_crc32(old,size);
        h.header_crc32=cache_record_crc32((const uint8_t *)&h,offsetof(cache_record_header_t,header_crc32));
        memcpy(blobs[1],&h,sizeof(h));memcpy(blobs[1]+sizeof(h),old,size);sizes[1]=size<=4 ? 28 : 32;
        refresh_device_control_profile_status();
        assert(writes==0 && s_status.device_control_profile_valid);
        assert(s_status.device_control_profile.brightness_percent==26 && s_status.device_control_profile.volume_percent==100);
        assert(s_status.device_control_profile.photo_config==PHOTO_CONFIG_DEFAULT);
        assert(s_status.device_control_profile.photo_clock_widget==PHOTO_CLOCK_WIDGET_DEFAULT);
    }
    /* The deployed 16-byte gallery profile keeps its timings and selection;
     * reading it must neither write NVS nor activate the new exit timer. */
    cache_record_header_t gallery_header={.magic=NP2_CACHE_RECORD_MAGIC,.schema_version=1,
        .header_size=sizeof(gallery_header),.generation=7,.payload_size=16};
    const uint8_t gallery[]={26,100,1,1,28,30,6,3,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    gallery_header.payload_crc32=cache_record_crc32(gallery,sizeof(gallery));
    gallery_header.header_crc32=cache_record_crc32((const uint8_t *)&gallery_header,
        offsetof(cache_record_header_t,header_crc32));
    memcpy(blobs[1],&gallery_header,sizeof(gallery_header));
    memcpy(blobs[1]+sizeof(gallery_header),gallery,sizeof(gallery));sizes[1]=40;
    refresh_device_control_profile_status();
    assert(!writes && s_status.device_control_profile_valid);
    assert(s_status.device_control_profile.photo_idle_choice==6 && s_status.device_control_profile.photo_interval_choice==3);
    assert(s_status.device_control_profile.photo_enabled_mask==UINT64_MAX-1 && !s_status.device_control_profile.photo_exit_choice);
    device_control_profile_t profile=s_status.device_control_profile;
    profile.photo_config=PHOTO_FRAME_ENABLED;
    profile.photo_idle_choice=7;profile.photo_interval_choice=9;
    profile.photo_enabled_mask=(UINT64_C(1)<<63)|4U;
    profile.photo_clock_widget|=PHOTO_WIDGET_TOP;
    profile.photo_bitcoin_widget=0;profile.photo_exit_choice=8;
    fail_commit=true;assert(write_device_control_profile(&profile)==ESP_FAIL);fail_commit=false;
    assert(select_latest_device_control_profile(&selected)==ESP_OK && selected.header.generation==7);
    assert(write_device_control_profile(&profile)==ESP_OK && writes==1);
    assert(select_latest_device_control_profile(&selected)==ESP_OK && selected.header.generation==8);
    assert(selected.header.payload_size==24 && sizes[0]==48 && !memcmp(&selected.profile,&profile,sizeof(profile)));
    /* Valid CRC alone is insufficient for malformed timer/reserved bytes. */
    uint8_t bad[24];memcpy(bad,&profile,sizeof(bad));
    device_control_profile_t decoded;
    bad[16]=PHOTO_EXIT_CHOICE_COUNT;
    assert(!device_control_profile_decode(bad,sizeof(bad),&decoded));
    bad[16]=8;bad[23]=1;
    assert(!device_control_profile_decode(bad,sizeof(bad),&decoded));
    profile.photo_exit_choice=PHOTO_EXIT_CHOICE_COUNT;
    assert(write_device_control_profile(&profile)==ESP_ERR_INVALID_ARG && writes==1);
    profile.photo_exit_choice=8;profile.reserved[6]=1;
    assert(write_device_control_profile(&profile)==ESP_ERR_INVALID_ARG && writes==1);
    profile.reserved[6]=0;
    profile.photo_config=255;assert(write_device_control_profile(&profile)==ESP_ERR_INVALID_ARG && writes==1);
    blobs[0][25]^=1;
    assert(select_latest_device_control_profile(&selected)==ESP_OK && selected.header.generation==7);
    blobs[1][0]^=1;assert(select_latest_device_control_profile(&selected)==ESP_ERR_INVALID_CRC);
    puts("Photo controls NVS: 2/3/4/6/16-byte legacy without writes, 24-byte timer roundtrip, failed commit, invalid prefs and CRC fallback PASS");
}
