/* Navigation hub only. Configuration remains in the legacy fallback. */
#pragma once

#include "np_components.h"

#define NP_PREFERENCES_ITEM_COUNT 5U

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *profile_button;
    lv_obj_t *rows[NP_PREFERENCES_ITEM_COUNT];
} np_preferences_view_t;

np_preferences_view_t np_preferences_build(lv_obj_t *parent);
