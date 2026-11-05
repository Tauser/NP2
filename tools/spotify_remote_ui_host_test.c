#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/screens/np_spotify.h"
#include "src/draw/lv_image_decoder_private.h"
#include "app_state.h"
#include "ui/screens/np_home_widget_gesture.h"
#include "ui/screens/np_screens.h"
static _Alignas(64) uint8_t pixels[1024*600*2];
static spotify_remote_projection_t state;
static spotify_remote_command_t last_command;
esp_err_t app_event_bus_post_spotify(const spotify_remote_command_t *c) { last_command=*c; return ESP_OK; }
void app_state_get_spotify_projection(spotify_remote_projection_t *out) { *out=state; }
esp_err_t provisioning_service_arm_open_network(void) { return ESP_OK; }
int64_t esp_timer_get_time(void) { return (int64_t)lv_tick_get()*1000; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) { (void)a; (void)p; lv_display_flush_ready(d); }
static void advance(void) { for(int i=0;i<20;++i) {lv_tick_inc(10); lv_timer_handler();} }
static void screenshot(const char *path)
{ advance(); FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels)); fclose(f); }
int main(int argc,char **argv)
{
    assert(argc==4); lv_init();
    uint8_t jpeg[8192]; FILE *fixture=fopen(argv[3],"rb"); assert(fixture);
    size_t jpeg_size=fread(jpeg,1,sizeof(jpeg),fixture); assert(feof(fixture)); fclose(fixture);
    lv_display_t *d=lv_display_create(1024,600);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    /* Desktop offscreen fixture only, hardware remains TRIPLE_PARTIAL. */
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d,flush);
    assert(np_home_widget_destination(-72,0)==1);
    assert(np_home_widget_destination(72,0)==0);
    assert(np_home_widget_destination(-100,-60)==-1);
    assert(np_home_widget_destination(-71,0)==-1);
    assert(np_home_widget_step(0,-72,0)==1 && np_home_widget_step(1,-72,0)==2 && np_home_widget_step(2,-72,0)==2);
    assert(np_home_widget_step(2,72,0)==1 && np_home_widget_step(0,72,0)==0 && np_home_widget_step(1,-71,0)==-1);
    assert(np_home_widget_destination(-100,60)==-1);
    lv_font_glyph_dsc_t glyph;
    const unsigned icons[] = {0xe034,0xe037};
    for(unsigned i=0;i<2;++i) {
        assert(lv_font_get_glyph_dsc(NP_FONT_ICON_BADGE,&glyph,icons[i],0) && !glyph.is_placeholder);
    }
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON,&glyph,0xe050,0) && !glyph.is_placeholder);
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON,&glyph,0xe044,0) && !glyph.is_placeholder);
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON,&glyph,0xe045,0) && !glyph.is_placeholder);
    lv_obj_t *content=np_scene(lv_screen_active());
    np_header_t header=np_header(lv_screen_active());
    np_home_view_t home=np_home_build_with_header(content,&header);
    np_set_visible(home.weather_card,false);
    for(unsigned cycle=0;cycle<100;++cycle) {
        static np_spotify_view_t v;
        v=np_spotify_build(home.root); np_spotify_bind(&v);
        assert(np_home_widget_owner(v.position,home.weather_card,&v)==-1);
        assert(np_home_widget_owner(lv_obj_get_child(v.play,0),home.weather_card,&v)==-1);
        assert(np_home_widget_owner(v.title,home.weather_card,&v)==1);
        assert(np_home_widget_owner(home.weather_place,home.weather_card,&v)==1);
        assert(np_home_widget_owner(home.root,home.weather_card,&v)==0);
        memset(&state,0,sizeof(state)); np_spotify_sync(&v,true);
        assert(lv_obj_has_state(v.play,LV_STATE_DISABLED));
        lv_obj_send_event(v.devices,LV_EVENT_CLICKED,NULL); np_spotify_sync(&v,true);
        lv_obj_send_event(v.link,LV_EVENT_CLICKED,NULL); np_spotify_sync(&v,true);
        assert(strstr(lv_label_get_text(v.status),"USB liberada"));
        assert(strstr(lv_label_get_text(v.device_hint),"USB liberada"));
        np_modal_hide(&v.device_modal);
        if(cycle==0) screenshot(argv[1]);
        state.linked=state.available=state.playing=state.supports_volume=true;
        state.volume=45; state.duration_ms=200000; state.progress_ms=63000; state.sampled_ms=lv_tick_get();
        strcpy(state.title,"Uma música para o seu dia"); strcpy(state.artist,"Seu artista favorito");
        strcpy(state.device_name,"Computador"); strcpy(state.device_id,"fixture");
        assert(spotify_cover_decode(jpeg,jpeg_size,&state.cover_generation));
        np_spotify_sync(&v,true); assert(!lv_obj_has_state(v.play,LV_STATE_DISABLED));
        lv_image_decoder_dsc_t decoded={0};
        assert(lv_image_decoder_open(&decoded,&v.cover_image,NULL)==LV_RESULT_OK);
        assert(decoded.decoded->data==v.cover_image.data);
        assert((uintptr_t)v.cover_image.data%64==0);
        assert(v.cover_image.header.stride==lv_draw_buf_width_to_stride(v.cover_image.header.w,LV_COLOR_FORMAT_RGB565));
        lv_image_decoder_close(&decoded);
        lv_obj_send_event(v.play,LV_EVENT_CLICKED,NULL); assert(last_command.action==SPOTIFY_REMOTE_PAUSE);
        lv_obj_send_event(v.devices,LV_EVENT_CLICKED,NULL); assert(np_modal_is_visible(&v.device_modal));
        assert(np_home_widget_owner(v.volume,home.weather_card,&v)==-1);
        state.device_count=1; state.revision=7; strcpy(state.devices[0].name,"Sala");
        strcpy(state.devices[0].id,"fixture-speaker"); np_spotify_sync(&v,true);
        lv_obj_send_event(v.device_rows[0],LV_EVENT_CLICKED,NULL);
        assert(last_command.action==SPOTIFY_REMOTE_TRANSFER && last_command.revision==7);
        assert(lv_obj_has_flag(v.status,LV_OBJ_FLAG_HIDDEN));
        if(cycle==0) {
            screenshot(argv[2]);
            /* Synthetic red album must fill only the card, be darkened,
             * preserve rounded corners and keep foreground text above it. */
            unsigned bg_r=pixels[(120*1024+550)*2+1]>>3;
            assert(bg_r>=6 && bg_r<=8);
            assert(lv_obj_get_width(v.title)==512);
            assert(lv_image_get_antialias(v.cover));
            assert(lv_image_get_inner_align(v.cover)==LV_IMAGE_ALIGN_COVER);
            lv_point_t pivot; lv_image_get_pivot(v.cover,&pivot);
            assert(pivot.x==0 && pivot.y==0);
            unsigned outside=pixels[(120*1024+590)*2+1]>>3;
            assert(outside<bg_r);
            unsigned corner=pixels[(76*1024+24)*2+1]>>3;
            assert(corner<bg_r);
            printf("Loaded art: full-card cover, uniform shade, clipping and antialias PASS\n");
        }
        memset(state.title,'W',sizeof(state.title)-1); state.title[sizeof(state.title)-1]=0;
        memset(state.artist,'W',sizeof(state.artist)-1); state.artist[sizeof(state.artist)-1]=0;
        np_spotify_sync(&v,true); lv_obj_update_layout(v.root);
        assert(lv_obj_get_height(v.title)==84 && lv_obj_get_height(v.artist)==28);
        np_spotify_sync(&v,false); assert(lv_obj_has_state(v.play,LV_STATE_DISABLED));
        state.cover_generation=spotify_cover_clear(); np_spotify_sync(&v,true);
        assert(lv_obj_has_flag(v.cover,LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_has_flag(v.cover_shade,LV_OBJ_FLAG_HIDDEN));
        lv_obj_delete(v.root); advance();
    }
    puts("Spotify UI: 100 build/dispose cycles, unlinked/offline states, physical USB feedback, player intents, modal device selection PASS");
}
