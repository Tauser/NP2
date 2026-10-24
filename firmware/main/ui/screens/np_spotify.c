#include "np_spotify.h"
#include "app_event_bus.h"
#include "app_state.h"
#include "provisioning_service.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include "src/misc/cache/instance/lv_image_cache.h"

static void enabled(lv_obj_t *o, bool yes)
{ if (!o) return; if (yes) lv_obj_remove_state(o, LV_STATE_DISABLED); else lv_obj_add_state(o, LV_STATE_DISABLED); }
static void post(np_spotify_view_t *v, spotify_remote_action_t action, uint32_t value)
{
    spotify_remote_command_t c = {.action = action, .value = value, .revision = v->modal_revision};
    if (app_event_bus_post_spotify(&c) != ESP_OK) np_set_text(v->status, "Painel ocupado. Tente novamente.");
}
static void row_event(lv_event_t *e)
{
    np_spotify_view_t *v = lv_event_get_user_data(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    for (unsigned i = 0; i < SPOTIFY_REMOTE_DEVICES; ++i)
        if (lv_event_get_target(e) == v->device_rows[i]) {
            post(v, SPOTIFY_REMOTE_TRANSFER, i); np_modal_hide(&v->device_modal); break;
        }
}
static lv_obj_t *slider(lv_obj_t *parent, int x, int y, int width, int max);
static void action_event(lv_event_t *e)
{
    np_spotify_view_t *v = lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);
    if (v->syncing) return;
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        if (target == v->volume) post(v, SPOTIFY_REMOTE_VOLUME, (uint32_t)lv_slider_get_value(target));
        else if (target == v->position) post(v, SPOTIFY_REMOTE_SEEK,
            (uint32_t)((uint64_t)lv_slider_get_value(target) * v->state.duration_ms / 1000U));
        return;
    }
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (target == v->link) {
        bool ok = provisioning_service_arm_open_network() == ESP_OK;
        if (ok) v->usb_until = lv_tick_get() + 60000U;
        np_set_text(v->status, ok ? "USB liberada por 60 s. Conclua no computador." : "USB indisponível");
    } else if (target == v->play) post(v, v->state.playing ? SPOTIFY_REMOTE_PAUSE : SPOTIFY_REMOTE_PLAY, 0);
    else if (target == v->previous) post(v, SPOTIFY_REMOTE_PREVIOUS, 0);
    else if (target == v->next) post(v, SPOTIFY_REMOTE_NEXT, 0);
    else if (target == v->refresh) post(v, SPOTIFY_REMOTE_REFRESH, 0);
    else if (target == v->devices) {
        if (v->device_modal.scrim == NULL) {
            np_modal_create(&v->device_modal, lv_obj_get_parent(v->root), 222, 76, 580, 496,
                NP_ICON_VOLUME_UP, np_c_positive(), "Spotify", "Dispositivo de saída e volume");
            v->refresh = np_icon_button(v->device_modal.content, 500, 116, 48, NP_ICON_REFRESH);
            lv_obj_add_event_cb(v->refresh, action_event, LV_EVENT_CLICKED, v);
            v->volume = slider(v->device_modal.content, 32, 28, 360, 100);
            v->volume_text = np_label(v->device_modal.content, "Volume", NP_FONT_SM, np_c_text_2(), 412, 16, 136, LV_TEXT_ALIGN_RIGHT);
            lv_obj_add_event_cb(v->volume, action_event, LV_EVENT_RELEASED, v);
            v->link = np_button(v->device_modal.content, 24, 64, 532, 48, "Vincular conta Spotify", false);
            lv_obj_add_event_cb(v->link, action_event, LV_EVENT_CLICKED, v);
            v->device_hint = np_label(v->device_modal.content, "Buscando dispositivos...", NP_FONT_SM,
                np_c_text_2(), 24, 124, 464, LV_TEXT_ALIGN_LEFT);
            lv_obj_set_height(v->device_hint, 40);
            lv_label_set_long_mode(v->device_hint, LV_LABEL_LONG_WRAP);
            for (unsigned i = 0; i < SPOTIFY_REMOTE_DEVICES; ++i) {
                v->device_rows[i] = np_button(v->device_modal.content, 24, 170 + (int)i * 50, 532, 48, "", false);
                lv_obj_add_event_cb(v->device_rows[i], row_event, LV_EVENT_CLICKED, v);
                np_set_visible(v->device_rows[i], false);
            }
        }
        np_modal_show(&v->device_modal);
        np_set_text(v->device_hint, "Atualizando dispositivos...");
        if (v->state.linked && !v->state.auth_required && !v->state.busy) post(v, SPOTIFY_REMOTE_DEVICES_REQUEST, 0);
    }
}
static void dispose_event(lv_event_t *e)
{
    np_spotify_view_t *v = lv_event_get_user_data(e);
    if (v->device_modal.scrim) { lv_obj_delete(v->device_modal.scrim); v->device_modal = (np_modal_t){0}; }
    lv_image_cache_drop(&v->cover_image);
    spotify_cover_release(v->cover_lease); v->cover_lease = -1;
}
static lv_obj_t *slider(lv_obj_t *parent, int x, int y, int width, int max)
{
    lv_obj_t *s = lv_slider_create(parent);
    lv_obj_set_pos(s, x, y); lv_obj_set_size(s, width, 8);
    lv_slider_set_range(s, 0, max);
    lv_obj_set_style_bg_color(s, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s, np_c_positive(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, np_c_text(), LV_PART_KNOB);
    lv_obj_set_ext_click_area(s, 20);
    return s;
}
np_spotify_view_t np_spotify_build(lv_obj_t *parent)
{
    np_spotify_view_t v = {0};
    v.cover_lease = -1;
    v.root = np_panel(parent, 24, 76, 560, 376);
    np_set_clip_corner(v.root, true);
    v.cover = np_image(v.root, 0, 0, 560, 376);
    lv_image_set_inner_align(v.cover, LV_IMAGE_ALIGN_COVER);
    lv_image_set_antialias(v.cover, true);
    np_set_visible(v.cover, false);
    v.cover_shade = np_fill(v.root, 0, 0, 560, 376, lv_color_black(), 190, 0);
    lv_obj_remove_flag(v.cover_shade, LV_OBJ_FLAG_CLICKABLE);
    np_set_visible(v.cover_shade, false);
    np_label(v.root, "Spotify", NP_FONT_MD, np_c_text(), 24, 20, 180, LV_TEXT_ALIGN_LEFT);
    v.title = np_label(v.root, "Conecte seu Spotify", NP_FONT_TITLE, np_c_text(), 24, 68, 512, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(v.title, 84); lv_label_set_long_mode(v.title, LV_LABEL_LONG_DOT);
    v.artist = np_label(v.root, "Sua música, no seu dispositivo", NP_FONT_MD, np_c_text_2(), 24, 160, 512, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(v.artist, 28); lv_label_set_long_mode(v.artist, LV_LABEL_LONG_DOT);
    np_label(v.root, NP_ICON_DISPLAY, NP_FONT_ICON, np_c_positive(), 24, 198, 24, LV_TEXT_ALIGN_LEFT);
    v.device = np_label(v.root, "Nenhum dispositivo ativo", NP_FONT_SM, np_c_positive(), 56, 200, 480, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(v.device, 24); lv_label_set_long_mode(v.device, LV_LABEL_LONG_DOT);
    v.position = slider(v.root, 32, 246, 496, 1000);
    lv_obj_set_style_height(v.position, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(v.position, 3, LV_PART_KNOB);
    v.time = np_label(v.root, "0:00", NP_FONT_SM, np_c_text_2(), 24, 260, 100, LV_TEXT_ALIGN_LEFT);
    v.duration = np_label(v.root, "0:00", NP_FONT_SM, np_c_text_2(), 436, 260, 100, LV_TEXT_ALIGN_RIGHT);
    v.previous = np_icon_button(v.root, 168, 294, 48, NP_ICON_SKIP_PREVIOUS);
    v.play = np_icon_button(v.root, 248, 286, 64, NP_ICON_PLAY_ARROW);
    lv_obj_t *play_label = lv_obj_get_child(v.play, 0);
    lv_obj_set_style_text_font(play_label, NP_FONT_ICON_BADGE, 0);
    lv_obj_set_height(play_label, NP_FONT_ICON_BADGE->line_height); lv_obj_center(play_label);
    lv_obj_set_style_bg_color(v.play, np_c_positive(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(v.play, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(v.play, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    np_set_text_color(play_label, np_c_surface());
    v.next = np_icon_button(v.root, 344, 294, 48, NP_ICON_SKIP_NEXT);
    v.devices = np_icon_button(v.root, 488, 294, 48, NP_ICON_SETTINGS);
    v.status = np_label(v.root, "", NP_FONT_SM, np_c_text_2(), 24, 224, 512, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_bg_color(v.status, np_c_surface(), 0);
    lv_obj_set_style_bg_opa(v.status, LV_OPA_COVER, 0);
    lv_obj_set_height(v.status, 20); lv_label_set_long_mode(v.status, LV_LABEL_LONG_DOT);
    return v;
}
void np_spotify_bind(np_spotify_view_t *v)
{
    lv_obj_add_event_cb(v->root, dispose_event, LV_EVENT_DELETE, v);
    lv_obj_t *buttons[] = {v->play, v->previous, v->next, v->devices};
    for (unsigned i = 0; i < sizeof(buttons)/sizeof(buttons[0]); ++i)
        lv_obj_add_event_cb(buttons[i], action_event, LV_EVENT_CLICKED, v);
    lv_obj_add_event_cb(v->position, action_event, LV_EVENT_RELEASED, v);
}
void np_spotify_sync(np_spotify_view_t *v, bool online)
{
    app_state_get_spotify_projection(&v->state);
    spotify_remote_projection_t *p = &v->state;
    if (p->cover_generation != v->cover_generation) {
        int8_t lease = -1;
        const spotify_cover_t *art = spotify_cover_acquire(p->cover_generation, &lease);
        np_set_visible(v->cover, false);
        lv_image_set_src(v->cover, NULL);
        lv_image_cache_drop(&v->cover_image);
        spotify_cover_release(v->cover_lease);
        v->cover_lease = lease;
        v->cover_generation = p->cover_generation;
        if (art) {
            v->cover_image = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                .w = art->width, .h = art->height, .stride = art->stride},
                .data_size = art->stride * art->height, .data = art->pixels};
            lv_image_set_src(v->cover, &v->cover_image);
            np_set_visible(v->cover, true);
        }
        np_set_visible(v->cover_shade, art != NULL);
    }
    v->syncing = true;
    np_set_visible(v->link, !p->linked || p->auth_required);
    bool control = online && p->linked && !p->auth_required && !p->busy && p->available && !p->restricted;
    enabled(v->play, control); enabled(v->next, control); enabled(v->previous, control);
    enabled(v->position, control && p->duration_ms > 0); enabled(v->volume, control && p->supports_volume);
    enabled(v->devices, true);
    enabled(v->refresh, online && p->linked && !p->busy);
    np_set_text(v->title, p->available ? p->title : p->linked ? "Escolha uma música no Spotify" : "Conecte seu Spotify");
    np_set_text(v->artist, p->available ? p->artist : "Abra o Spotify no celular ou computador");
    np_set_text(v->device, p->device_name[0] ? p->device_name : "Nenhum dispositivo ativo");
    np_set_text(lv_obj_get_child(v->play, 0), p->playing ? NP_ICON_PAUSE : NP_ICON_PLAY_ARROW);
    uint64_t ms = p->progress_ms;
    uint64_t now = (uint64_t)esp_timer_get_time()/1000U;
    if (p->playing && online && now >= p->sampled_ms && now - p->sampled_ms < 15000U) ms += now - p->sampled_ms;
    if (ms > p->duration_ms) ms = p->duration_ms;
    char text[64];
    snprintf(text, sizeof(text), "%u:%02u", (unsigned)(ms/60000U), (unsigned)(ms/1000U%60U));
    np_set_text(v->time, text);
    snprintf(text, sizeof(text), "%u:%02u", (unsigned)(p->duration_ms/60000U), (unsigned)(p->duration_ms/1000U%60U));
    np_set_text(v->duration, text);
    if (!lv_obj_has_state(v->position, LV_STATE_PRESSED)) lv_slider_set_value(v->position,
        p->duration_ms ? (int32_t)(ms*1000U/p->duration_ms) : 0, LV_ANIM_OFF);
    if (v->volume && !lv_obj_has_state(v->volume, LV_STATE_PRESSED)) lv_slider_set_value(v->volume, p->volume, LV_ANIM_OFF);
    snprintf(text, sizeof(text), "Volume %u%%", p->volume); np_set_text(v->volume_text, text);
    const char *status = v->usb_until && (int32_t)(v->usb_until - lv_tick_get()) > 0 && (!p->linked || p->auth_required) ?
        "USB liberada por 60 s. Conclua no computador." : !online ? "Sem conexão. Aguardando a rede retornar." :
        !p->linked ? "Vincule sua conta para começar." : p->auth_required ? "Autorize novamente sua conta Spotify." :
         p->result == SPOTIFY_REMOTE_FORBIDDEN ? "Acesso recusado. Confira Premium e usuários autorizados no app Spotify." :
        p->result == SPOTIFY_REMOTE_RATE_LIMIT ? "Spotify pediu uma pausa. Aguarde a próxima atualização." :
        p->result != ESP_OK ? "Não foi possível concluir. Atualize ou abra o Spotify no outro dispositivo." :
        p->restricted ? "Este dispositivo não permite controle remoto pela API." : "";
    np_set_text(v->status, status);
    np_set_visible(v->status, status[0] != 0);
    if (np_modal_is_visible(&v->device_modal) && !p->busy) {
        v->modal_revision = p->revision;
        np_set_text(v->device_hint, v->usb_until && (int32_t)(v->usb_until - lv_tick_get()) > 0 ?
            "USB liberada por 60 s. Conclua no computador." : p->device_count ? "Toque para mudar a saída; depois toque em reproduzir." : "Abra o Spotify em outro dispositivo e atualize a lista.");
        for (unsigned i = 0; i < SPOTIFY_REMOTE_DEVICES; ++i) {
            np_set_visible(v->device_rows[i], i < p->device_count);
            if (i < p->device_count) {
                np_set_text(lv_obj_get_child(v->device_rows[i], 0), p->devices[i].name);
                enabled(v->device_rows[i], !p->devices[i].restricted);
            }
        }
    }
    v->syncing = false;
}
