#include <assert.h>
#include <stdio.h>
#include "ui/screens/settings/np_settings_notifications.h"
static uint8_t pixels[1024*600*2];
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area;(void)data;lv_display_flush_ready(display); }
static void advance(void) { for (unsigned i=0U;i<20U;++i) { lv_tick_inc(10U);lv_timer_handler(); } }
int main(int argc,char **argv)
{
    assert(argc == 2);lv_init();lv_display_t *display=lv_display_create(1024,600);
    lv_font_glyph_dsc_t glyph={0};assert(lv_font_get_glyph_dsc(NP_FONT_ICON,&glyph,0xe8b5,0) && !glyph.is_placeholder);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display,flush);
    np_settings_notifications_view_t v=np_settings_notifications_scene_build(lv_screen_active());
    np_set_visible(v.root,true);uint8_t choices[]={1U,2U,3U};
    np_set_text(v.persistence_status,"Preferências salvas automaticamente");
    np_settings_notifications_sync(&v.notifications,true,true,true,choices);advance();
    for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) {
        assert(lv_dropdown_get_selected(v.notifications.melodies[i]) == choices[i]);
        lv_area_t a,b;lv_obj_get_coords(v.notifications.melodies[i],&a);lv_obj_get_coords(v.notifications.previews[i],&b);
        assert(a.y1>=80 && a.y2<540 && a.x2<b.x1 && b.x2<1000 && b.y2-b.y1>=47);
        lv_dropdown_open(v.notifications.melodies[i]);advance();
        assert(lv_dropdown_is_open(v.notifications.melodies[i]));lv_dropdown_close(v.notifications.melodies[i]);
    }
    advance();FILE *file=fopen(argv[1],"wb");assert(file);assert(fwrite(pixels,1U,sizeof(pixels),file) == sizeof(pixels));fclose(file);
    lv_obj_delete(v.root);advance();lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    for (unsigned i=0U;i<100U;++i) { v=np_settings_notifications_scene_build(lv_screen_active());lv_obj_delete(v.root);advance(); }
    lv_mem_monitor(&after);assert(after.free_size>=before.free_size);
    puts("Sound settings UI: three selectors/previews, dropdown bounds, 100 lifecycles PASS");
}
