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

#define NP_KEYBOARD_MAX_BINDINGS 8U

typedef struct np_keyboard np_keyboard_t;
/* Keys are public labels from the keyboard matrix, never entered text. */
typedef void (*np_keyboard_input_cb_t)(void *user_data, const char *key);

typedef struct {
    np_keyboard_t *owner;
    lv_obj_t *textarea;
    np_keyboard_mode_t mode;
} np_keyboard_binding_t;

struct np_keyboard {
    lv_obj_t *root;
    lv_obj_t *keyboard;
    lv_obj_t *target;
    np_keyboard_binding_t bindings[NP_KEYBOARD_MAX_BINDINGS];
    bool interaction_inside_keyboard;
    bool reconcile_pending;
    bool keyboard_events_registered;
    bool private_input_active;
    np_keyboard_input_cb_t private_input;
    void *private_input_user_data;
    uint32_t lifecycle_generation;
};

np_keyboard_t np_keyboard_create(lv_obj_t *parent);
void np_keyboard_bind(np_keyboard_t *keyboard, lv_obj_t *textarea,
                      np_keyboard_mode_t mode);
void np_keyboard_focus(np_keyboard_t *keyboard, lv_obj_t *textarea,
                       np_keyboard_mode_t mode);
void np_keyboard_open(np_keyboard_t *keyboard, np_keyboard_mode_t mode,
                      np_keyboard_input_cb_t input, void *user_data);
void np_keyboard_hide(np_keyboard_t *keyboard);
void np_keyboard_clear_target(np_keyboard_t *keyboard);
bool np_keyboard_is_visible(const np_keyboard_t *keyboard);
void np_keyboard_destroy(np_keyboard_t *keyboard);

#ifdef __cplusplus
}
#endif
