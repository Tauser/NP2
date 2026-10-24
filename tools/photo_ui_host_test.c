#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ui/screens/np_photo_frame.h"
#include "ui/screens/np_market.h"
#include "ui/screens/settings/np_settings_display_sound.h"
#include "ui/screens/settings/np_settings_photo_frame.h"
#include "ui/screens/np_preferences.h"
#include "src/misc/cache/instance/lv_image_cache.h"
static uint8_t pixels[1024*600*2];
static uint8_t photo_pixels[PHOTO_BYTES];
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area;(void)data;lv_display_flush_ready(display); }
static void advance(void) { for(unsigned i=0;i<20;++i){lv_tick_inc(10);lv_timer_handler();} }
static void shot(const char *folder,const char *name)
{
    char path[512];snprintf(path,sizeof(path),"%s/%s.rgb565",folder,name);
    advance();FILE *f=fopen(path,"wb");assert(f);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);
}
int main(int argc,char **argv)
{
    assert(argc==3);FILE *f=fopen(argv[2],"rb");assert(f);uint8_t header[24];
    assert(fread(header,1,24,f)==24 && photo_header_valid(header,NULL));
    assert(fread(photo_pixels,1,PHOTO_BYTES,f)==PHOTO_BYTES);fclose(f);
    const lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=1024,.h=600,.stride=2048},.data_size=PHOTO_BYTES,.data=photo_pixels};
    lv_init();lv_display_t *display=lv_display_create(1024,600);
    lv_font_glyph_dsc_t arrow={0};
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON,&arrow,0xe313,0) && !arrow.is_placeholder);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    np_photo_frame_view_t v=np_photo_frame_build(lv_screen_active());
    np_photo_frame_set_source(&v,&image);np_set_visible(v.root,true);
    offline_data_snapshot_t data={0};data.market.available=true;data.market.bitcoin_usd_cents=6784000;
    np_photo_frame_sync(&v,1792694520U,true,&data.market,PHOTO_BITCOIN_WIDGET_DEFAULT,PHOTO_CLOCK_WIDGET_DEFAULT);
    shot(argv[1],"frame");assert(!strcmp(lv_label_get_text(v.bitcoin),"67.840"));
    assert(lv_obj_get_style_text_font(v.bitcoin,0)==lv_obj_get_style_text_font(v.clock,0));
    data.market.bitcoin_usd_cents=99999900;
    np_photo_frame_sync(&v,1792694520U,true,&data.market,28,30);advance();
    assert(lv_obj_get_height(v.bitcoin)<=80); /* six digits remain on one line */
    assert(lv_obj_get_x(v.bitcoin)+lv_obj_get_width(v.bitcoin)<=lv_obj_get_width(v.widgets[0]));
    data.market.bitcoin_usd_cents=6784000;
    assert(!memcmp(pixels,photo_pixels,2));assert(!memcmp(pixels+PHOTO_BYTES-2,photo_pixels+PHOTO_BYTES-2,2));
    assert(!lv_obj_has_flag(v.widgets[0],LV_OBJ_FLAG_CLICKABLE));
    for(unsigned i=0;i<64;++i){
        np_photo_frame_sync(&v,1792694520U,true,&data.market,i,63U-i);
        assert(lv_obj_get_style_y(v.widgets[0],0)==((i&PHOTO_WIDGET_TOP)?24:456));
        assert(lv_obj_has_flag(v.widgets[0],LV_OBJ_FLAG_HIDDEN)==!(i&PHOTO_WIDGET_VISIBLE));
        assert(lv_obj_get_style_text_font(v.bitcoin,0)==lv_obj_get_style_text_font(v.clock,0));
    }
    data.market.available=false;np_photo_frame_sync(&v,0,false,&data.market,28,30);
    assert(!strcmp(lv_label_get_text(v.bitcoin),"--") && !strcmp(lv_label_get_text(v.clock),"--:--"));
    lv_obj_delete(v.root);advance();
    np_market_view_t market=np_market_build_with_header(lv_screen_active(),NULL);
    data.market.available=true;np_market_sync(&market,&data);
    assert(lv_obj_get_style_bg_opa(lv_obj_get_parent(market.btc_price),0)==LV_OPA_COVER);
    for(unsigned i=0;i<lv_obj_get_child_count(market.root);++i)
        assert(!lv_obj_check_type(lv_obj_get_child(market.root,i),&lv_image_class));
    shot(argv[1],"market");
    lv_obj_delete(market.root);advance();
    lv_mem_monitor_t gallery_before,gallery_after,modal_after;lv_mem_monitor(&gallery_before);
    np_settings_photo_frame_view_t settings=np_settings_photo_frame_build(lv_screen_active());
    np_set_visible(settings.root,true);
    photo_frame_status_t gallery={.ready=true,.mounted=true,.count=5,.catalog=15U | (UINT64_C(1)<<63)};
    np_settings_photo_frame_sync(&settings,&gallery,UINT64_MAX,true,3,4,0,false);
    assert(settings.ids[0]==1 && settings.ids[3]==4);
    assert(lv_dropdown_get_option_count(settings.photo_idle)==PHOTO_IDLE_CHOICE_COUNT);
    assert(lv_dropdown_get_option_count(settings.photo_interval)==PHOTO_INTERVAL_CHOICE_COUNT);
    assert(lv_dropdown_get_option_count(settings.photo_exit)==PHOTO_EXIT_CHOICE_COUNT);
    static uint8_t thumb_pixels[PHOTO_THUMB_BYTES];
    for(unsigned y=0;y<PHOTO_THUMB_HEIGHT;++y)for(unsigned x=0;x<PHOTO_THUMB_WIDTH;++x)
        memcpy(thumb_pixels+(y*PHOTO_THUMB_WIDTH+x)*2,photo_pixels+((y*600/PHOTO_THUMB_HEIGHT)*1024+x*1024/PHOTO_THUMB_WIDTH)*2,2);
    const lv_image_dsc_t thumbnail={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=PHOTO_THUMB_WIDTH,.h=PHOTO_THUMB_HEIGHT,.stride=PHOTO_THUMB_WIDTH*2},.data_size=PHOTO_THUMB_BYTES,.data=thumb_pixels};
    photo_thumbnail_page_t page={.catalog=gallery.catalog,.page=0,.ids={1,2,3,4},.images={&thumbnail,&thumbnail,&thumbnail,&thumbnail}};
    np_settings_photo_frame_set_thumbnails(&settings,&page);
    shot(argv[1],"settings");lv_mem_monitor(&gallery_after);
    np_settings_photo_frame_sync(&settings,&gallery,0,true,7,9,8,true);
    assert(lv_dropdown_get_selected(settings.photo_exit)==8);
    assert(lv_obj_has_state(settings.photo_preview,LV_STATE_DISABLED));
    assert(lv_obj_has_flag(settings.checks[0],LV_OBJ_FLAG_HIDDEN));
    settings.page=1;np_settings_photo_frame_sync(&settings,&gallery,UINT64_MAX,true,3,4,0,false);
    assert(settings.ids[0]==64 && !settings.ids[1]);
    assert(lv_obj_has_state(settings.next,LV_STATE_DISABLED));
    np_settings_photo_frame_set_thumbnails(&settings,&page);
    assert(lv_obj_has_flag(settings.images[0],LV_OBJ_FLAG_HIDDEN));
    np_photo_widget_editor_open(&settings,28,30);
    lv_mem_monitor(&modal_after);
    printf("Photo gallery allocation: scene=%u modal_total=%u bytes (host pointers)\n",
        (unsigned)(gallery_before.free_size-gallery_after.free_size),(unsigned)(gallery_before.free_size-modal_after.free_size));
    assert(np_photo_widget_editor_value(&settings.widget_editor,0)==30);
    assert(np_photo_widget_editor_value(&settings.widget_editor,1)==30);
    lv_dropdown_set_selected(settings.widget_editor.position[0],1);
    lv_dropdown_set_selected(settings.widget_editor.contrast[0],3);
    assert(np_photo_widget_editor_value(&settings.widget_editor,0)==47);
    lv_dropdown_set_selected(settings.widget_editor.size[1],0);
    assert(!(np_photo_widget_editor_value(&settings.widget_editor,0)&PHOTO_WIDGET_LARGE));
    assert(!(np_photo_widget_editor_value(&settings.widget_editor,1)&PHOTO_WIDGET_LARGE));
    lv_dropdown_set_selected(settings.widget_editor.size[1],1);
    shot(argv[1],"widgets");np_modal_hide(&settings.widget_editor.modal);
    lv_obj_delete(settings.root);advance();lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    for(unsigned i=0;i<100;++i){
        v=np_photo_frame_build(lv_screen_active());np_photo_frame_set_source(&v,&image);
        lv_obj_delete(v.root);
        settings=np_settings_photo_frame_build(lv_screen_active());
        np_photo_widget_editor_open(&settings,28,30);lv_obj_delete(settings.root);advance();
    }
    np_preferences_view_t prefs=np_preferences_build(lv_screen_active());
    assert(NP_PREFERENCES_ITEM_COUNT==6);
    np_set_visible(prefs.root,true);shot(argv[1],"preferences");lv_obj_delete(prefs.root);advance();
    lv_image_cache_drop(&thumbnail);lv_image_cache_drop(&image);lv_mem_monitor(&after);assert(after.free_size>=before.free_size);
    puts("Photo LVGL: full photograph, 64 widget settings, missing data/time, market without wallpaper, modal controls, 100 lifecycles PASS");
}
