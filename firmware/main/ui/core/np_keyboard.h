/* Persistent virtual keyboard shared by Product UI scenes. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "np_components.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NP_KEYBOARD_MODE_TEXT = 0,
    NP_KEYBOARD_MODE_PASSWORD,
    NP_KEYBOARD_MODE_NUMERIC,
} np_keyboard_mode_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *keyboard;
    lv_obj_t *target;
    bool interaction_inside_keyboard;
    bool reconcile_pending;
    uint32_t lifecycle_generation;
} np_keyboard_t;

np_keyboard_t np_keyboard_create(lv_obj_t *parent);
void np_keyboard_bind(np_keyboard_t *keyboard, lv_obj_t *textarea,
                      np_keyboard_mode_t mode);
void np_keyboard_focus(np_keyboard_t *keyboard, lv_obj_t *textarea,
                       np_keyboard_mode_t mode);
void np_keyboard_hide(np_keyboard_t *keyboard);
void np_keyboard_clear_target(np_keyboard_t *keyboard);
bool np_keyboard_is_visible(const np_keyboard_t *keyboard);
void np_keyboard_destroy(np_keyboard_t *keyboard);

#ifdef __cplusplus
}
#endif
