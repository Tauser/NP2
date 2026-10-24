#include <assert.h>
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "photo_frame_model.h"
#include "photo_thumbnail_service.h"
#include "device_control_profile.h"
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
static TickType_t tick;
static unsigned allocations;
static bool fail_alloc;
TickType_t xTaskGetTickCount(void) { return tick; }
void *heap_caps_aligned_alloc(size_t align, size_t size, unsigned caps)
{
    (void)align; assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    assert(size == PHOTO_BYTES || size == PHOTO_PAGE_SIZE * PHOTO_THUMB_BYTES); if (fail_alloc) return NULL;
    ++allocations; return malloc(size);
}
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 999999U; }
#include "../firmware/main/photo_thumbnail_service.c"
#include "../firmware/main/photo_frame_service.c"

static void reset(void)
{
    if (s_file) fclose(s_file);
    close_thumbnail();
    for (unsigned i=0;i<2;++i) free(s_thumbnails[i].pixels);
    memset(s_thumbnails,0,sizeof(s_thumbnails));s_requested=false;s_requested_page=0;
    s_thumb_published=s_thumb_loading=-1;s_thumb_item=s_thumb_y=s_thumb_generation=0;
    s_thumb_retry=0;s_thumb_failed=false;
    for (unsigned i=0;i<2;++i) free(s_slots[i].pixels);
    memset(s_slots,0,sizeof(s_slots)); memset(&s_status,0,sizeof(s_status));
    s_config=PHOTO_CONFIG_DEFAULT;s_interval_choice=PHOTO_INTERVAL_CHOICE_DEFAULT;s_enabled_mask=UINT64_MAX; s_visible=false; s_published=-1; s_catalog=0;
    s_file=NULL;s_loading=-1;s_index=0;s_offset=0;s_scan_tick=s_swap_tick=s_retry_tick=0;
    s_scanned=s_attempted=false;tick=0;allocations=0;fail_alloc=false;
    for(unsigned i=1;i<=64;++i){char path[64];snprintf(path,sizeof(path),"np2/photos/photo%03u.npf",i);remove(path);}
}
static void le16(uint8_t *p,unsigned v) { p[0]=v;p[1]=v>>8; }
static void le32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;++i)p[i]=v>>(i*8); }
static void fixture(unsigned id,uint8_t value,bool bad_crc,size_t payload_size,bool trailing)
{
    char path[64];snprintf(path,sizeof(path),"np2/photos/photo%03u.npf",id);
    FILE *f=fopen(path,"wb");assert(f);
    uint8_t *pixels=malloc(PHOTO_BYTES);assert(pixels);memset(pixels,value,PHOTO_BYTES);
    uint8_t h[24]={0};memcpy(h,"NPFP",4);le16(h+4,1);le16(h+6,PHOTO_WIDTH);
    le16(h+8,PHOTO_HEIGHT);le16(h+10,1);le32(h+16,PHOTO_BYTES);
    le32(h+20,~photo_crc_update(UINT32_MAX,pixels,PHOTO_BYTES)^(bad_crc?1U:0U));
    assert(fwrite(h,1,24,f)==24);assert(fwrite(pixels,1,payload_size,f)==payload_size);
    if(trailing) fputc(1,f);
    fclose(f);
    free(pixels);
}
static void steps(unsigned count) { for(unsigned i=0;i<count;++i){photo_frame_service_step(true);tick+=100;} }
static void legacy(void)
{
    device_control_profile_t p;const uint8_t old[]={26,100,1,PHOTO_CONFIG_DEFAULT};
    for(unsigned size=2;size<=4;++size){assert(device_control_profile_decode(old,size,&p));
        assert(p.photo_exit_choice==PHOTO_EXIT_CHOICE_DEFAULT);
        assert(p.brightness_percent==26 && p.volume_percent==100);
        assert(p.photo_bitcoin_widget==PHOTO_BITCOIN_WIDGET_DEFAULT && p.photo_clock_widget==PHOTO_CLOCK_WIDGET_DEFAULT);}
    assert(!device_control_profile_decode(old,5,&p));
    assert(photo_idle_ms(PHOTO_CONFIG_DEFAULT)==300000 && photo_interval_ms(PHOTO_CONFIG_DEFAULT)==120000);
    assert(photo_widget_opacity(PHOTO_BITCOIN_WIDGET_DEFAULT)==91);
    const uint8_t *s=(const uint8_t *)"123456789";
    assert(~photo_crc_update(UINT32_MAX,s,9)==0xcbf43926U);
}
static void exit_timer(void)
{
    const uint32_t durations[]={0,30000,60000,120000,300000,600000,900000,1800000,3600000};
    for(unsigned i=0;i<PHOTO_EXIT_CHOICE_COUNT;++i) {
        assert(photo_exit_choice_ms(i)==durations[i]);
        assert(!photo_exit_due(i,1000,1000));
        if(!durations[i]) { assert(!photo_exit_due(i,0,UINT32_MAX));continue; }
        assert(!photo_exit_due(i,1000,1000+durations[i]-1));
        assert(photo_exit_due(i,1000,1000+durations[i]));
        assert(photo_exit_due(i,1000,1000+durations[i]+100));
        uint32_t start=UINT32_MAX-100;
        assert(!photo_exit_due(i,start,start+durations[i]-1));
        assert(photo_exit_due(i,start,start+durations[i]));
    }
    assert(!photo_exit_due(255,0,UINT32_MAX));
    /* Starting a new session resets elapsed time, including after tick wrap. */
    assert(photo_exit_due(1,0,30000));
    assert(!photo_exit_due(1,30000,30000));
}
int main(void)
{
    exit_timer();
    _mkdir("np2");_mkdir("np2/photos");legacy();reset();
    photo_frame_service_step(false);assert(s_status.ready && !s_status.available);
    steps(1);assert(!s_status.count && !allocations);
    reset();fixture(1,0x11,false,PHOTO_BYTES,false);
    photo_frame_service_configure(2U); /* legacy wallpaper flag alone is ignored */
    steps(40);assert(!s_status.available && !allocations);
    photo_frame_service_set_visible(true);steps(40);
    assert(s_status.available && allocations==1); /* manual preview still works */
    reset();fixture(1,0x11,false,PHOTO_BYTES,false);steps(40);
    assert(s_status.available && s_status.generation==1 && allocations==1);
    uint32_t gen;int8_t lease;const lv_image_dsc_t *one=photo_frame_service_acquire(&gen,&lease);
    assert(one && gen==1 && one->data[0]==0x11);assert(!photo_frame_service_acquire(&gen,&lease));
    photo_frame_service_set_visible(true);tick+=300000;steps(50);
    assert(s_status.generation==1 && allocations==1);photo_frame_service_release(lease);
    reset();fixture(1,0x11,false,PHOTO_BYTES,false);fixture(2,0x22,false,PHOTO_BYTES,false);steps(40);
    int8_t old_lease,new_lease;one=photo_frame_service_acquire(&gen,&old_lease);assert(one);
    photo_frame_service_set_visible(true);tick+=121000;steps(40);
    const lv_image_dsc_t *two=photo_frame_service_acquire(&gen,&new_lease);
    assert(two && gen==2 && two->data[0]==0x22 && one->data[0]==0x11);
    tick+=121000;steps(40);assert(s_status.generation==2 && one->data[0]==0x11);
    photo_frame_service_release(old_lease);steps(40);assert(s_status.generation==3 && two->data[0]==0x22);
    photo_frame_service_release(new_lease);
    reset();fixture(1,1,false,PHOTO_BYTES,false);fixture(2,2,true,PHOTO_BYTES,false);steps(40);
    one=photo_frame_service_acquire(&gen,&old_lease);photo_frame_service_set_visible(true);tick+=121000;steps(40);
    assert(s_status.available && s_status.generation==1 && s_status.result==ESP_ERR_INVALID_CRC && one->data[0]==1);
    photo_frame_service_release(old_lease);
    reset();fixture(1,1,false,17,false);steps(3);assert(!s_status.available && s_status.result==ESP_FAIL);
    reset();fixture(1,1,false,PHOTO_BYTES,true);steps(40);assert(!s_status.available && s_status.result==ESP_ERR_INVALID_CRC);
    reset();fixture(1,1,false,PHOTO_BYTES,false);fail_alloc=true;steps(1);
    assert(!s_status.available && s_status.result==ESP_ERR_NO_MEM && allocations==0);
    reset();fixture(1,1,false,PHOTO_BYTES,false);steps(1);assert(s_loading>=0);
    photo_frame_service_step(false);assert(s_file==NULL && s_loading<0 && !s_status.pending);
    reset();puts("Photos: legacy prefs, CRC, absence, single photo, two leases/backpressure, corrupt/truncated/trailing SD, memory failure, removal PASS");
}
