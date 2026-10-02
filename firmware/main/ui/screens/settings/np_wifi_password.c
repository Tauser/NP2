#include "np_wifi_password.h"

#include <string.h>

static void closing(void *user_data)
{
    np_wifi_password_t *const view = user_data;
    view->visible = false;
    np_keyboard_hide(view->keyboard);
    lv_textarea_set_text(view->field, "");
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

static void reject_widget_input(lv_event_t *event)
{
    np_wifi_password_t *const view = lv_event_get_user_data(event);
    if (!view->syncing_mask) lv_textarea_set_insert_replace(view->field, "");
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
    np_modal_create(&view->modal, parent, 320, 104, 384, 230,
                     NP_ICON_WIFI, np_c_positive(), "Senha da rede", ssid);
    np_modal_set_close_callback(&view->modal, closing, view);
    lv_obj_t *const content = view->modal.content;
    view->hint = np_label(content, secure ? "Digite a senha para conectar" :
                          "Rede aberta: nenhuma senha necessaria",
                          NP_FONT_SM, np_c_text_2(), 20, 10, 344, LV_TEXT_ALIGN_LEFT);
    /* The textarea is presentation-only: it retains mask characters, never
     * the secret. Global keyboard keys go straight to provisioning through
     * the existing private-input contract. Reveal uses immediate glyphs. */
    view->field = np_form_text_input(content, 20, 38, 344, 48, "", NULL);
    lv_textarea_set_password_mode(view->field, true);
    lv_textarea_set_password_show_time(view->field, 0);
    lv_textarea_set_max_length(view->field, 63);
    lv_obj_add_event_cb(view->field, reject_widget_input, LV_EVENT_INSERT, view);
    np_set_visible(lv_textarea_get_label(view->field), false);
    lv_obj_set_style_bg_opa(view->field, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_add_flag(view->field, LV_OBJ_FLAG_CLICKABLE);
    np_form_apply_field_style(view->field);
    lv_obj_set_style_border_width(view->field, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(view->field, np_c_accent(), LV_PART_MAIN);
    if (draw != NULL) lv_obj_add_event_cb(view->field, draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(view->field, field_event, LV_EVENT_CLICKED, view);
    view->reveal = np_form_icon_button(content, 316, 42, 40, NP_ICON_VISIBILITY);
    lv_obj_add_event_cb(view->reveal, reveal_event, LV_EVENT_CLICKED, view);
    np_set_visible(view->reveal, secure);
    lv_obj_t *const cancel = np_button(content, 20, 104, 164, 44, "Cancelar", false);
    view->connect = np_button(content, 200, 104, 164, 44, "Conectar", true);
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
    (void)length;
    view->syncing_mask = true;
    /* The field is only a drawing surface. The service renders the masked or
     * revealed characters in DRAW_MAIN; storing literal asterisks here made
     * revealed characters overlap them and look different from the input. */
    lv_textarea_set_text(view->field, "");
    lv_textarea_set_password_mode(view->field, true);
    view->syncing_mask = false;
    np_set_text(lv_obj_get_child(view->reveal, 0),
                visible ? NP_ICON_VISIBILITY_OFF : NP_ICON_VISIBILITY);
    if (!view->secure || length >= 8U) lv_obj_remove_state(view->connect, LV_STATE_DISABLED);
    else lv_obj_add_state(view->connect, LV_STATE_DISABLED);
    lv_obj_invalidate(view->field);
}
