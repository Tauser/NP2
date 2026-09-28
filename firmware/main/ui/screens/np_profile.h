/* Single primary profile. Presentation only; no identity storage or I/O. */
#pragma once

#include "np_components.h"

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *home_button;
    lv_obj_t *edit_button;
    lv_obj_t *name_row;
    lv_obj_t *avatar_row;
    lv_obj_t *initial_screen_row;
    lv_obj_t *preferences_row;
} np_profile_view_t;

np_profile_view_t np_profile_build(lv_obj_t *parent);
