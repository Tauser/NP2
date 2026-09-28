#include "np_wifi_password.h"

#include <stdio.h>
#include <string.h>

static void complete_key_map(np_wifi_password_t *view)
{
    const char *const *const map = lv_keyboard_get_map_array(view->keyboard);
    const bool special = lv_keyboard_get_mode(view->keyboard) == LV_KEYBOARD_MODE_SPECIAL;
    for (size_t i = 0U; i < sizeof(view->key_map) / sizeof(view->key_map[0]); ++i) {
        const char *key = map[i];
        /* Entry is append/backspace only, so cursor arrows are unused. Use
         * their existing cells for the four ASCII symbols absent in LVGL. */
        if (strcmp(key, LV_SYMBOL_LEFT) == 0) key = special ? "~" : "|";
        else if (strcmp(key, LV_SYMBOL_RIGHT) == 0) key = special ? "^" : "`";
        view->key_map[i] = key;
        if (key[0] == '\0') {
            lv_buttonmatrix_set_map(view->keyboard, view->key_map);
            return;
        }
    }
}

static void closing(void *user_data)
{
    np_wifi_password_t *const view = user_data;
    view->visible = false;
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

static void key_event(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    const uint32_t index = lv_keyboard_get_selected_button(view->keyboard);
    if (index == LV_BUTTONMATRIX_BUTTON_NONE || view->action == NULL) return;
    const char *const key = lv_keyboard_get_button_text(view->keyboard, index);
    if (key == NULL) return;
    /* No textarea is ever associated. Mode keys remain local presentation. */
    if (strcmp(key, "abc") == 0 || strcmp(key, "ABC") == 0 || strcmp(key, "1#") == 0) {
        lv_keyboard_set_mode(view->keyboard, strcmp(key, "abc") == 0 ? LV_KEYBOARD_MODE_TEXT_LOWER :
                             strcmp(key, "ABC") == 0 ? LV_KEYBOARD_MODE_TEXT_UPPER : LV_KEYBOARD_MODE_SPECIAL);
        complete_key_map(view);
    }
    else if (strcmp(key, LV_SYMBOL_BACKSPACE) == 0) (void)view->action(NP_WIFI_PASSWORD_BACKSPACE, 0);
    else if (strcmp(key, LV_SYMBOL_OK) == 0 || strcmp(key, LV_SYMBOL_NEW_LINE) == 0) {
        if (view->action(NP_WIFI_PASSWORD_SUBMIT, 0)) np_modal_hide(&view->modal);
    } else if (strcmp(key, LV_SYMBOL_KEYBOARD) == 0 || strcmp(key, LV_SYMBOL_CLOSE) == 0) {
        np_modal_hide(&view->modal);
    } else if (key[0] != '\0' && key[1] == '\0') {
        (void)view->action(NP_WIFI_PASSWORD_APPEND, key[0]);
    }
}

void np_wifi_password_create(np_wifi_password_t *view, lv_obj_t *parent,
                              const char *ssid, bool secure,
                              np_wifi_password_action_cb_t action,
                              lv_event_cb_t draw)
{
    if (view == NULL || parent == NULL) return;
    *view = (np_wifi_password_t){.secure = secure, .action = action};
    /* One small child dialog over Wi-Fi, including one matrix keyboard. */
    np_modal_create(&view->modal, parent, 192, 50, 640, 500,
                     NP_ICON_WIFI, np_c_positive(), "Conectar rede", ssid);
    np_modal_set_close_callback(&view->modal, closing, view);
    lv_obj_t *const content = view->modal.content;
    view->field = np_fill(content, 24, 12, 592, 64, np_c_surface_raised(),
                           LV_OPA_COVER, NP_RADIUS_CONTROL);
    if (draw != NULL) lv_obj_add_event_cb(view->field, draw, LV_EVENT_DRAW_MAIN, NULL);
    view->hint = np_label(content, "", NP_FONT_SM, np_c_text_2(), 24, 82, 330, LV_TEXT_ALIGN_LEFT);
    view->reveal = np_button(content, 400, 82, 216, 48, "Mostrar senha", false);
    lv_obj_add_event_cb(view->reveal, reveal_event, LV_EVENT_CLICKED, view);
    np_set_visible(view->reveal, secure);
    view->keyboard = lv_keyboard_create(content);
    lv_obj_set_pos(view->keyboard, 16, 144);
    lv_obj_set_size(view->keyboard, 608, 200);
    lv_obj_set_style_text_font(view->keyboard, NP_FONT_SM, LV_PART_ITEMS);
    lv_obj_set_style_text_color(view->keyboard, np_c_text(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(view->keyboard, np_c_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view->keyboard, np_c_surface_raised(), LV_PART_ITEMS);
    lv_obj_remove_event_cb(view->keyboard, lv_keyboard_def_event_cb);
    complete_key_map(view);
    lv_obj_add_event_cb(view->keyboard, key_event, LV_EVENT_VALUE_CHANGED, view);
    np_set_visible(view->keyboard, secure);
    lv_obj_t *const cancel = np_button(content, 24, 366, 180, 48, "Cancelar", false);
    view->connect = np_button(content, 436, 366, 180, 48, "Conectar", true);
    lv_obj_set_style_opa(view->connect, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_add_event_cb(cancel, cancel_event, LV_EVENT_CLICKED, view);
    lv_obj_add_event_cb(view->connect, connect_event, LV_EVENT_CLICKED, view);
    np_wifi_password_sync(view, 0U, false);
    np_modal_show(&view->modal);
}

void np_wifi_password_sync(np_wifi_password_t *view, uint8_t length, bool visible)
{
    if (view == NULL || view->field == NULL) return;
    view->visible = visible;
    char hint[64];
    if (view->secure) (void)snprintf(hint, sizeof(hint), "Senha: %u/63 caracteres (minimo 8)", length);
    else (void)snprintf(hint, sizeof(hint), "Rede aberta: nenhuma senha necessaria");
    np_set_text(view->hint, hint);
    np_set_text(lv_obj_get_child(view->reveal, 0), visible ? "Ocultar senha" : "Mostrar senha");
    if (!view->secure || length >= 8U) lv_obj_remove_state(view->connect, LV_STATE_DISABLED);
    else lv_obj_add_state(view->connect, LV_STATE_DISABLED);
    lv_obj_invalidate(view->field);
}
