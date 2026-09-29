/* Single primary profile; visual editor only. app_loop owns saved identity. */
#pragma once

#include "np_components.h"
#include "np_modal.h"
#include "np_keyboard.h"
#include "user_profile.h"
#include "esp_err.h"

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *home_button;
    lv_obj_t *edit_button;
    lv_obj_t *name_row;
    lv_obj_t *avatar_row;
    lv_obj_t *initial_screen_row;
    lv_obj_t *preferences_row;
    lv_obj_t *avatar;
    lv_obj_t *avatar_initials;
    lv_obj_t *greeting;
    lv_obj_t *identity_hint;
    lv_obj_t *name_value;
    lv_obj_t *avatar_value;
    np_modal_t editor;
    lv_obj_t *name_input;
    lv_obj_t *color_buttons[USER_PROFILE_COLOR_COUNT];
    lv_obj_t *editor_avatar;
    lv_obj_t *editor_initials;
    lv_obj_t *save_button;
    lv_obj_t *cancel_button;
    np_keyboard_t *keyboard;
    uint8_t draft_color;
    uint8_t current_color;
} np_profile_view_t;

np_profile_view_t np_profile_build(lv_obj_t *parent);
void np_profile_sync(np_profile_view_t *view, bool configured,
                     const user_profile_t *profile, bool pending, esp_err_t result);
void np_profile_open_editor(np_profile_view_t *view, np_keyboard_t *keyboard,
                            bool focus_name);
void np_profile_close_editor(np_profile_view_t *view);
bool np_profile_editor_value(const np_profile_view_t *view, user_profile_t *out_profile);
