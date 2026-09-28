#include "np_wifi_password.h"

#include <string.h>

static void closing(void *user_data)
{
    np_wifi_password_t *const view = user_data;
    view->visible = false;
    np_keyboard_hide(view->keyboard);
    if (view->action != NULL) (void)view->action(NP_WIFI_PASSWORD_CANCEL, 0);
    lv_obj_invalidate(view->field);
}

static void cancel_event(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    np_modal_hide(&view->modal);
}

static void connect_event(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    if (view->action != NULL && view->action(NP_WIFI_PASSWORD_SUBMIT, 0)) {
        np_modal_hide(&view->modal);
    }
}

static void reveal_event(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    if (view->action != NULL) (void)view->action(NP_WIFI_PASSWORD_REVEAL, !view->visible);
}

static void keyboard_input(void *user_data, const char *key)
{
    np_wifi_password_t *const view = user_data;
    if (view == NULL || key == NULL || view->action == NULL) return;
    if (strcmp(key, LV_SYMBOL_BACKSPACE) == 0) {
        (void)view->action(NP_WIFI_PASSWORD_BACKSPACE, 0);
    }
    else if (strcmp(key, LV_SYMBOL_OK) == 0 || strcmp(key, LV_SYMBOL_NEW_LINE) == 0) {
        if (view->action(NP_WIFI_PASSWORD_SUBMIT, 0)) np_modal_hide(&view->modal);
    } else if (key[0] != '\0' && key[1] == '\0') {
        (void)view->action(NP_WIFI_PASSWORD_APPEND, key[0]);
    }
}

static void field_event(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    if (view == NULL || !view->secure) return;
    np_keyboard_open(view->keyboard, NP_KEYBOARD_MODE_PASSWORD, keyboard_input, view);
}

void np_wifi_password_create(np_wifi_password_t *view, lv_obj_t *parent,
                              np_keyboard_t *keyboard,
                              const char *ssid, bool secure,
                              np_wifi_password_action_cb_t action,
                              lv_event_cb_t draw)
{
    if (view == NULL || parent == NULL) return;
    *view = (np_wifi_password_t){.keyboard = keyboard, .secure = secure, .action = action};
    /* It stays above the child dialog, so the shared keyboard remains usable. */
    np_modal_create(&view->modal, parent, 320, 104, 384, 258,
                     NP_ICON_WIFI, np_c_positive(), "Senha da rede", ssid);
    np_modal_set_close_callback(&view->modal, closing, view);
    lv_obj_t *const content = view->modal.content;
    view->hint = np_label(content, secure ? "Digite a senha para conectar" :
                          "Rede aberta: nenhuma senha necessaria",
                          NP_FONT_SM, np_c_text_2(), 20, 10, 344, LV_TEXT_ALIGN_LEFT);
    view->field = np_fill(content, 20, 38, 344, 48, np_c_surface_raised(),
                           LV_OPA_COVER, NP_RADIUS_CONTROL);
    lv_obj_add_flag(view->field, LV_OBJ_FLAG_CLICKABLE);
    np_apply_input_style(view->field);
    lv_obj_set_style_border_width(view->field, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(view->field, np_c_accent(), LV_PART_MAIN);
    if (draw != NULL) lv_obj_add_event_cb(view->field, draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(view->field, field_event, LV_EVENT_CLICKED, view);
    view->reveal = np_button(content, 20, 94, 170, 32, "Mostrar senha", false);
    lv_obj_add_event_cb(view->reveal, reveal_event, LV_EVENT_CLICKED, view);
    np_set_visible(view->reveal, secure);
    lv_obj_t *const cancel = np_button(content, 20, 132, 164, 44, "Cancelar", false);
    view->connect = np_button(content, 200, 132, 164, 44, "Conectar", true);
    lv_obj_set_style_opa(view->connect, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_add_event_cb(cancel, cancel_event, LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->connect, connect_event, LV_EVENT_CLICKED, view);
    np_wifi_password_sync(view, 0U, false);
    np_modal_show(&view->modal);
    if (secure) np_keyboard_open(view->keyboard, NP_KEYBOARD_MODE_PASSWORD,
                                 keyboard_input, view);
}

void np_wifi_password_sync(np_wifi_password_t *view, uint8_t length, bool visible)
{
    if (view == NULL || view->field == NULL) return;
    view->visible = visible;
    np_set_text(lv_obj_get_child(view->reveal, 0), visible ? "Ocultar senha" : "Mostrar senha");
    if (!view->secure || length >= 8U) lv_obj_remove_state(view->connect, LV_STATE_DISABLED);
    else lv_obj_add_state(view->connect, LV_STATE_DISABLED);
    lv_obj_invalidate(view->field);
}
