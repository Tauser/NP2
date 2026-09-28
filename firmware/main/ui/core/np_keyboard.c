#include "np_keyboard.h"

#define NP_KEYBOARD_Y 370
#define NP_KEYBOARD_H 230

static void keyboard_reconcile_async(void *user_data)
{
    np_keyboard_t *const keyboard = user_data;
    if (keyboard == NULL) return;
    keyboard->reconcile_pending = false;
    if (!np_keyboard_is_visible(keyboard)) return;

    /* A press on a key may create a transient defocus on the textarea. The
     * first reconciliation following that interaction keeps the session. */
    if (keyboard->interaction_inside_keyboard) {
        keyboard->interaction_inside_keyboard = false;
        return;
    }
    if (keyboard->target != NULL &&
        lv_obj_has_state(keyboard->target, LV_STATE_FOCUSED)) {
        return;
    }
    np_keyboard_hide(keyboard);
}

static void schedule_reconcile(np_keyboard_t *keyboard)
{
    if (keyboard == NULL || keyboard->reconcile_pending) return;
    keyboard->reconcile_pending = true;
    if (lv_async_call(keyboard_reconcile_async, keyboard) != LV_RESULT_OK) {
        keyboard->reconcile_pending = false;
    }
}

static void target_event_cb(lv_event_t *event)
{
    np_keyboard_t *const keyboard = lv_event_get_user_data(event);
    lv_obj_t *const target = lv_event_get_target(event);
    if (keyboard == NULL || target == NULL) return;

    switch (lv_event_get_code(event)) {
    case LV_EVENT_FOCUSED:
        np_keyboard_focus(keyboard, target, NP_KEYBOARD_MODE_TEXT);
        break;
    case LV_EVENT_DEFOCUSED:
        schedule_reconcile(keyboard);
        break;
    case LV_EVENT_READY:
    case LV_EVENT_CANCEL:
        np_keyboard_hide(keyboard);
        break;
    case LV_EVENT_DELETE:
        if (keyboard->target == target) np_keyboard_hide(keyboard);
        break;
    default:
        break;
    }
}

static void keyboard_event_cb(lv_event_t *event)
{
    np_keyboard_t *const keyboard = lv_event_get_user_data(event);
    if (keyboard == NULL) return;
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        keyboard->interaction_inside_keyboard = true;
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        np_keyboard_hide(keyboard);
    }
}

static lv_keyboard_mode_t lv_mode(np_keyboard_mode_t mode)
{
    switch (mode) {
    case NP_KEYBOARD_MODE_NUMERIC:
        return LV_KEYBOARD_MODE_NUMBER;
    case NP_KEYBOARD_MODE_PASSWORD:
    case NP_KEYBOARD_MODE_TEXT:
    default:
        return LV_KEYBOARD_MODE_TEXT_LOWER;
    }
}

np_keyboard_t np_keyboard_create(lv_obj_t *parent)
{
    np_keyboard_t result = {0};
    if (parent == NULL) return result;

    result.root = np_group(parent, 0, NP_KEYBOARD_Y, NP_SCREEN_W, NP_KEYBOARD_H);
    result.keyboard = lv_keyboard_create(result.root);
    if (result.keyboard == NULL) {
        lv_obj_delete(result.root);
        return (np_keyboard_t){0};
    }
    lv_obj_set_size(result.keyboard, NP_SCREEN_W, NP_KEYBOARD_H);
    lv_obj_set_style_bg_color(result.keyboard, np_c_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(result.keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(result.keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(result.keyboard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.keyboard, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(result.keyboard, np_c_surface_raised(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(result.keyboard, np_c_text(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(result.keyboard, NP_FONT_SM, LV_PART_ITEMS);
    lv_obj_set_style_radius(result.keyboard, NP_RADIUS_CONTROL, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(result.keyboard, np_c_accent_bg(),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    np_set_visible(result.root, false);
    return result;
}

void np_keyboard_bind(np_keyboard_t *keyboard, lv_obj_t *textarea,
                      np_keyboard_mode_t mode)
{
    if (keyboard == NULL || keyboard->keyboard == NULL || textarea == NULL) return;
    if (mode == NP_KEYBOARD_MODE_PASSWORD) {
        lv_textarea_set_password_mode(textarea, true);
    }
    lv_obj_add_event_cb(textarea, target_event_cb, LV_EVENT_ALL, keyboard);
    lv_obj_add_event_cb(keyboard->keyboard, keyboard_event_cb, LV_EVENT_ALL, keyboard);
}

void np_keyboard_focus(np_keyboard_t *keyboard, lv_obj_t *textarea,
                       np_keyboard_mode_t mode)
{
    if (keyboard == NULL || keyboard->root == NULL ||
        keyboard->keyboard == NULL || textarea == NULL) return;
    keyboard->target = textarea;
    keyboard->interaction_inside_keyboard = false;
    lv_keyboard_set_mode(keyboard->keyboard, lv_mode(mode));
    lv_keyboard_set_textarea(keyboard->keyboard, textarea);
    lv_obj_move_foreground(keyboard->root);
    np_set_visible(keyboard->root, true);
}

void np_keyboard_clear_target(np_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    ++keyboard->lifecycle_generation;
    keyboard->target = NULL;
    keyboard->interaction_inside_keyboard = false;
    if (keyboard->keyboard != NULL) lv_keyboard_set_textarea(keyboard->keyboard, NULL);
}

void np_keyboard_hide(np_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    np_keyboard_clear_target(keyboard);
    if (keyboard->root != NULL) np_set_visible(keyboard->root, false);
}

bool np_keyboard_is_visible(const np_keyboard_t *keyboard)
{
    return keyboard != NULL && keyboard->root != NULL &&
           !lv_obj_has_flag(keyboard->root, LV_OBJ_FLAG_HIDDEN);
}

void np_keyboard_destroy(np_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    np_keyboard_hide(keyboard);
    if (keyboard->root != NULL) lv_obj_delete(keyboard->root);
    *keyboard = (np_keyboard_t){0};
}
