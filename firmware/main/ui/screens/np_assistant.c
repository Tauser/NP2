#include "np_assistant.h"
#include <stdio.h>
#include <string.h>

#define ORB 68
#define MARGIN 16
#define GAP 12
#define CAPSULE_MAX_W 720
#define CAPSULE_H 108

static int32_t clamp(int32_t n, int32_t lo, int32_t hi)
{ return n < lo ? lo : n > hi ? hi : n; }

static void place(np_assistant_view_t *v)
{
    lv_obj_set_pos(v->orb, v->x, v->y);
    int32_t left = v->x - MARGIN - GAP;
    int32_t right = NP_SCREEN_W - MARGIN - v->x - ORB - GAP;
    int32_t w, x, y;
    if (left >= 340 || right >= 340) {
        bool use_left = left >= right;
        w = clamp(use_left ? left : right, 340, CAPSULE_MAX_W);
        x = use_left ? v->x - GAP - w : v->x + ORB + GAP;
        y = clamp(v->y + (ORB - CAPSULE_H) / 2, MARGIN, NP_SCREEN_H - MARGIN - CAPSULE_H);
    } else {
        w = CAPSULE_MAX_W;
        x = clamp(v->x + ORB / 2 - w / 2, MARGIN, NP_SCREEN_W - MARGIN - w);
        y = v->y >= NP_SCREEN_H / 2 ? v->y - GAP - CAPSULE_H : v->y + ORB + GAP;
        y = clamp(y, MARGIN, NP_SCREEN_H - MARGIN - CAPSULE_H);
    }
    lv_obj_set_pos(v->root, x, y);
    lv_obj_set_width(v->root, w);
    lv_obj_set_width(v->status, w - 92);
    lv_obj_set_width(v->text, w - 92);
    lv_obj_set_x(v->close, w - 64);
    lv_obj_set_x(v->code, w - 218);
    if (v->state.state == XIAOZHI_STATE_ACTIVATION)
        lv_obj_set_width(v->status, w - 232);
}

static void eyes(lv_timer_t *timer)
{
    np_assistant_view_t *v = lv_timer_get_user_data(timer);
    uint32_t t = lv_tick_elaps(v->eye_epoch);
    int32_t h = 18, dx = 0, dy = 0;
    switch (v->state.state) {
    case XIAOZHI_STATE_LISTENING: h = 22; break;
    case XIAOZHI_STATE_SPEAKING: h = 15 + (int32_t)((t / 160U) % 3U) * 3; break;
    case XIAOZHI_STATE_CONNECTING:
    case XIAOZHI_STATE_WAITING:
    case XIAOZHI_STATE_THINKING: dx = ((t / 400U) % 2U) ? 3 : -3; dy = -2; break;
    case XIAOZHI_STATE_ERROR:
    case XIAOZHI_STATE_STOPPING: h = 10; break;
    default: dx = (int32_t)((t / 1800U) % 3U) * 2 - 2; break;
    }
    uint32_t blink = t % 4800U;
    if (blink >= 4480U && blink < 4640U) h = 3;
    lv_obj_t *eye[2] = {v->left_eye, v->right_eye};
    for (unsigned i = 0; i < 2; ++i) {
        int32_t x = 22 + (int32_t)i * 17 + dx, y = (ORB - h) / 2 + dy;
        if (lv_obj_get_height(eye[i]) != h) lv_obj_set_height(eye[i], h);
        if (lv_obj_get_x(eye[i]) != x || lv_obj_get_y(eye[i]) != y) lv_obj_set_pos(eye[i], x, y);
    }
}

static void orb_event(lv_event_t *event)
{
    np_assistant_view_t *v = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_DELETE) {
        if (v->eye_timer) { lv_timer_delete(v->eye_timer); v->eye_timer = NULL; }
        v->orb = NULL;
        return;
    }
    if (!v->available) return;
    lv_indev_t *indev = lv_indev_active();
    if (code == LV_EVENT_PRESSED && indev) {
        v->pressed = true; v->dragged = false;
        lv_indev_get_point(indev, &v->press_point);
        v->press_origin = (lv_point_t){v->x, v->y};
    } else if (code == LV_EVENT_PRESSING && v->pressed && indev) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        int32_t dx = point.x - v->press_point.x, dy = point.y - v->press_point.y;
        if (dx > 8 || dx < -8 || dy > 8 || dy < -8) v->dragged = true;
        if (v->dragged) {
            v->x = clamp(v->press_origin.x + dx, MARGIN, NP_SCREEN_W - MARGIN - ORB);
            v->y = clamp(v->press_origin.y + dy, MARGIN, NP_SCREEN_H - MARGIN - ORB);
            place(v);
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        v->pressed = false;
        if (code == LV_EVENT_PRESS_LOST) v->dragged = true;
    } else if (code == LV_EVENT_CLICKED && !v->dragged && v->action) {
        v->action(NP_ASSISTANT_TAP_ORB, v->user);
    }
}

static void close_event(lv_event_t *event)
{
    np_assistant_view_t *v = lv_event_get_user_data(event);
    if (v->available && v->action) v->action(NP_ASSISTANT_TAP_CLOSE, v->user);
}

static void capsule_deleted(lv_event_t *event)
{
    np_assistant_view_t *v = lv_event_get_user_data(event);
    v->root = NULL;
}

void np_assistant_destroy(np_assistant_view_t *v)
{
    if (!v) return;
    if (v->eye_timer) { lv_timer_delete(v->eye_timer); v->eye_timer = NULL; }
    if (v->orb) lv_obj_delete(v->orb);
    if (v->root) lv_obj_delete(v->root);
    memset(v, 0, sizeof(*v));
}

bool np_assistant_build(np_assistant_view_t *v, lv_obj_t *parent, np_assistant_action_cb_t cb, void *user)
{
    if (!v || !parent || v->root || v->orb) return false;
    v->rendered_revision = UINT32_MAX; v->action = cb; v->user = user;
    v->x = NP_SCREEN_W - MARGIN - ORB; v->y = NP_SCREEN_H - MARGIN - ORB;
    v->root = np_raised(parent, 0, 0, CAPSULE_MAX_W, CAPSULE_H);
    v->orb = np_fill(parent, v->x, v->y, ORB, ORB, lv_color_hex(0x83c8f1), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    if (!v->root || !v->orb) goto fail;
    np_set_radius(v->root, 28);
    lv_obj_set_style_pad_all(v->root, 0, 0);
    lv_obj_set_style_pad_all(v->orb, 0, 0);
    lv_obj_add_flag(v->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(v->orb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_remove_flag(v->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(v->orb, LV_OBJ_FLAG_SCROLLABLE);
    v->left_eye = np_fill(v->orb, 22, 25, 7, 18, lv_color_hex(0x11202a), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    v->right_eye = np_fill(v->orb, 39, 25, 7, 18, lv_color_hex(0x11202a), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    v->status = np_label(v->root, "", NP_FONT_MD, np_c_text(), 24, 16, 600, LV_TEXT_ALIGN_LEFT);
    v->code = np_label(v->root, "", NP_FONT_LG, np_c_warning(), 0, 12, 140, LV_TEXT_ALIGN_RIGHT);
    v->text = np_label(v->root, "", NP_FONT_SM, np_c_text_2(), 24, 46, 600, LV_TEXT_ALIGN_LEFT);
    v->close = np_icon_button(v->root, 0, 30, NP_TOUCH_TARGET, NP_ICON_CLOSE);
    if (!v->left_eye || !v->right_eye || !v->status || !v->code || !v->text || !v->close) goto fail;
    lv_obj_remove_flag(v->left_eye, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(v->right_eye, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(v->status, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_long_mode(v->text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_height(v->text, 48);
    lv_obj_add_event_cb(v->orb, orb_event, LV_EVENT_ALL, v);
    lv_obj_add_event_cb(v->root, capsule_deleted, LV_EVENT_DELETE, v);
    lv_obj_add_event_cb(v->close, close_event, LV_EVENT_CLICKED, v);
    v->eye_epoch = lv_tick_get();
    v->eye_timer = lv_timer_create(eyes, 80U, v);
    if (!v->eye_timer) goto fail;
    lv_timer_pause(v->eye_timer);
    place(v);
    np_set_visible(v->root, false); np_set_visible(v->orb, false);
    return true;
fail:
    np_assistant_destroy(v);
    return false;
}

void np_assistant_set_available(np_assistant_view_t *v, bool available)
{
    if (!v || !v->orb || v->available == available) return;
    v->available = available;
    np_set_visible(v->orb, available);
    np_set_visible(v->root, available && v->open);
    if (available) lv_timer_resume(v->eye_timer);
    else { lv_timer_pause(v->eye_timer); v->pressed = false; v->dragged = true; }
}

void np_assistant_set_open(np_assistant_view_t *v, bool open)
{
    if (!v || !v->root) return;
    v->open = open;
    np_set_visible(v->root, open && v->available);
    place(v);
    if (open) { lv_obj_move_foreground(v->root); lv_obj_move_foreground(v->orb); }
}

bool np_assistant_owns_touch(const np_assistant_view_t *v, const lv_obj_t *obj)
{
    for (; obj; obj = lv_obj_get_parent(obj))
        if (obj == v->orb || obj == v->root) return true;
    return false;
}

static const char *error_text(esp_err_t result, bool online)
{
    if (!online || result == ESP_ERR_INVALID_STATE) return "Sem conexão com a internet";
    if (result == XIAOZHI_ERR_ACTIVATION_TIMEOUT) return "Código expirou - toque para tentar de novo";
    if (result == XIAOZHI_ERR_SERVER_CLOSED) return "A conexão foi encerrada - tente novamente";
    if (result == ESP_ERR_NO_MEM) return "Memória insuficiente no momento";
    if (result == ESP_ERR_TIMEOUT) return "O servidor não respondeu a tempo";
    if (result == XIAOZHI_ERR_AUDIO_BUSY) return "Áudio em uso - tente ao terminar a reprodução";
    if (result == XIAOZHI_ERR_CODEC_STUCK) return "Assistente indisponível - áudio em recuperação";
    if (result == XIAOZHI_ERR_MICROPHONE) return "Falha no microfone - toque no círculo para tentar de novo";
    if (result == XIAOZHI_ERR_NO_SPEECH) return "Não reconheci a fala - toque para tentar novamente";
    if (result == XIAOZHI_ERR_CONNECT_TIMEOUT) return "Demorou para conectar - toque para tentar novamente";
    if (result == XIAOZHI_ERR_RESPONSE_TIMEOUT) return "O servidor parou de responder - tente novamente";
    if (result == XIAOZHI_ERR_PLAYBACK_TIMEOUT) return "Não foi possível concluir o áudio - tente novamente";
    return "Não foi possível falar com o assistente";
}

void np_assistant_sync(np_assistant_view_t *v, const xiaozhi_projection_t *p, bool online)
{
    if (!v || !v->root || !p || p->revision == v->rendered_revision) return;
    v->rendered_revision = p->revision; v->state = *p;
    const bool activation = p->state == XIAOZHI_STATE_ACTIVATION;
    const char *status = "Toque no círculo para falar";
    switch (p->state) {
    case XIAOZHI_STATE_WAITING: status = "Aguardando a rede ficar livre..."; break;
    case XIAOZHI_STATE_CONNECTING: status = "Preparando o assistente..."; break;
    case XIAOZHI_STATE_STOPPING: status = "Encerrando..."; break;
    case XIAOZHI_STATE_ACTIVATION: status = "Ative em xiaozhi.me"; break;
    case XIAOZHI_STATE_LISTENING: status = "Ouvindo... pode falar"; break;
    case XIAOZHI_STATE_THINKING: status = "Pensando..."; break;
    case XIAOZHI_STATE_SPEAKING: status = "Respondendo"; break;
    case XIAOZHI_STATE_ERROR: status = error_text(p->result, online); break;
    case XIAOZHI_STATE_IDLE: default: break;
    }
    np_set_text(v->status, status);
    np_set_visible(v->code, activation);
    place(v);
    if (activation) {
        np_set_text(v->code, p->activation_code[0] ? p->activation_code : "------");
        np_set_text(v->text, "Console > Adicionar dispositivo > informe o código");
    } else if (p->assistant_text[0]) np_set_text(v->text, p->assistant_text);
    else if (p->user_text[0]) {
        char line[XIAOZHI_TEXT_BYTES + 8];
        snprintf(line, sizeof(line), "“%s”", p->user_text);
        np_set_text(v->text, line);
    } else np_set_text(v->text, p->state == XIAOZHI_STATE_SPEAKING ? "Toque no círculo para interromper" : "");
}
