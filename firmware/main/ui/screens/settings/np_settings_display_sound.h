/* Shared construction; product_ui owns callbacks and services. */
#pragma once
#include "np_components.h"
typedef struct {
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_value;
    lv_obj_t *brightness_bubble;
    lv_obj_t *brightness_bubble_value;
    lv_obj_t *volume_slider;
    lv_obj_t *volume_value;
    lv_obj_t *volume_bubble;
    lv_obj_t *volume_bubble_value;
    lv_obj_t *night_switch;
    lv_obj_t *night_detail;
} np_display_sound_controls_t;
typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    np_display_sound_controls_t controls;
} np_settings_display_sound_view_t;
void np_settings_display_sound_slider(lv_obj_t *parent, int32_t y,
    const char *icon, const char *label, uint8_t percent,
    lv_obj_t **out_slider, lv_obj_t **out_value,
    lv_obj_t **out_bubble, lv_obj_t **out_bubble_value);
void np_settings_display_sound_night(lv_obj_t *parent, int32_t text_x,
    int32_t y, int32_t switch_x, np_display_sound_controls_t *controls);
np_settings_display_sound_view_t np_settings_display_sound_build(lv_obj_t *parent);
