#pragma once

#include "np_keyboard.h"
#include "np_modal.h"

typedef enum {
    NP_WIFI_PASSWORD_APPEND,
    NP_WIFI_PASSWORD_BACKSPACE,
    NP_WIFI_PASSWORD_REVEAL,
    NP_WIFI_PASSWORD_SUBMIT,
    NP_WIFI_PASSWORD_CANCEL,
} np_wifi_password_action_t;

/* Character is a key press, never a password buffer. Product UI owns the bridge. */
typedef bool (*np_wifi_password_action_cb_t)(np_wifi_password_action_t action,
                                            char character);

typedef struct {
    np_modal_t modal;
    lv_obj_t *field;
    lv_obj_t *hint;
    lv_obj_t *reveal;
    lv_obj_t *connect;
    np_keyboard_t *keyboard;
    bool secure;
    bool syncing_mask;
    bool visible;
    np_wifi_password_action_cb_t action;
} np_wifi_password_t;

void np_wifi_password_create(np_wifi_password_t *view, lv_obj_t *parent,
                              np_keyboard_t *keyboard,
                              const char *ssid, bool secure,
                              np_wifi_password_action_cb_t action,
                              lv_event_cb_t draw);
void np_wifi_password_sync(np_wifi_password_t *view, uint8_t length, bool visible);
