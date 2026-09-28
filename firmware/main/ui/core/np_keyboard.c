#include "np_keyboard.h"

#include <string.h>

#define NP_KEYBOARD_Y 370
#define NP_KEYBOARD_H 230

static void style_action_keys(lv_obj_t *keyboard)
{
    /* CHECKED in LVGL's stock maps highlights punctuation and navigation too.
     * Keep the maps/widths/callback intact; reserve that accent for OK. */
    lv_buttonmatrix_clear_button_ctrl_all(keyboard, LV_BUTTONMATRIX_CTRL_CHECKED);
    const char *const *map = lv_buttonmatrix_get_map(keyboard);
    uint32_t button = 0;
    for (uint32_t entry = 0; map[entry][0] != '\0'; ++entry) {
        if (strcmp(map[entry], "\n") == 0) continue;
        if (strcmp(map[entry], LV_SYMBOL_OK) == 0) {
            lv_buttonmatrix_set_button_ctrl(keyboard, button, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
        ++button;
    }
}

static void set_mode(np_keyboard_t *keyboard, lv_keyboard_mode_t mode)
{
    if (keyboard != NULL && keyboard->keyboard != NULL) {
        lv_keyboard_set_mode(keyboard->keyboard, mode);
        style_action_keys(keyboard->keyboard);
    }
}

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
    np_keyboard_binding_t *const binding = lv_event_get_user_data(event);
    np_keyboard_t *const keyboard = binding == NULL ? NULL : binding->owner;
    lv_obj_t *const target = lv_event_get_target(event);
    if (keyboard == NULL || target == NULL) return;

    switch (lv_event_get_code(event)) {
    case LV_EVENT_FOCUSED:
        np_keyboard_focus(keyboard, target, binding->mode);
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
        /* Lazy modals destroy their inputs. Return the binding slot to the
         * bounded pool so repeated navigation cannot exhaust it or leave a
         * stale textarea pointer attached to a newly allocated widget. */
        *binding = (np_keyboard_binding_t){0};
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
    if (code == LV_EVENT_VALUE_CHANGED && keyboard->pressed_key != NULL &&
        (strcmp(keyboard->pressed_key, "abc") == 0 ||
         strcmp(keyboard->pressed_key, "ABC") == 0 ||
         strcmp(keyboard->pressed_key, "1#") == 0)) {
        /* The standard callback has just installed the next mode's map. */
        style_action_keys(keyboard->keyboard);
    }
    if (code == LV_EVENT_PRESSED) {
        keyboard->interaction_inside_keyboard = true;
        const uint32_t index = lv_keyboard_get_selected_button(keyboard->keyboard);
        keyboard->pressed_key = index == LV_BUTTONMATRIX_BUTTON_NONE ? NULL
                              : lv_keyboard_get_button_text(keyboard->keyboard, index);
        return;
    }
    if (code == LV_EVENT_CANCEL) {
        np_keyboard_hide(keyboard);
        return;
    }
    if (code == LV_EVENT_READY) {
        if (keyboard->private_input_active && keyboard->private_input != NULL) {
            keyboard->private_input(keyboard->private_input_user_data, LV_SYMBOL_OK);
        }
        return;
    }
    if (code != LV_EVENT_VALUE_CHANGED || !keyboard->private_input_active ||
        keyboard->private_input == NULL) return;

    const uint32_t index = lv_keyboard_get_selected_button(keyboard->keyboard);
    if (index == LV_BUTTONMATRIX_BUTTON_NONE) return;
    const char *const key = keyboard->pressed_key != NULL ? keyboard->pressed_key
                        : lv_keyboard_get_button_text(keyboard->keyboard, index);
    if (key == NULL) return;

    if (strcmp(key, LV_SYMBOL_BACKSPACE) == 0) {
        keyboard->private_input(keyboard->private_input_user_data, LV_SYMBOL_BACKSPACE);
    } else if (strcmp(key, LV_SYMBOL_NEW_LINE) == 0) {
        keyboard->private_input(keyboard->private_input_user_data, LV_SYMBOL_OK);
    } else if (strcmp(key, LV_SYMBOL_LEFT) != 0 && strcmp(key, LV_SYMBOL_RIGHT) != 0 &&
               strcmp(key, LV_SYMBOL_CLOSE) != 0 && strcmp(key, LV_SYMBOL_KEYBOARD) != 0 &&
               strcmp(key, "1#") != 0 && strcmp(key, "abc") != 0 &&
               strcmp(key, "ABC") != 0) {
        keyboard->private_input(keyboard->private_input_user_data, key);
    }
    keyboard->pressed_key = NULL;
}

static void stop_private_input(np_keyboard_t *keyboard)
{
    if (keyboard == NULL || !keyboard->private_input_active) return;
    keyboard->private_input_active = false;
    keyboard->private_input = NULL;
    keyboard->private_input_user_data = NULL;
}

static void ensure_keyboard_events(np_keyboard_t *keyboard)
{
    if (keyboard == NULL || keyboard->keyboard == NULL ||
        keyboard->keyboard_events_registered) return;
    lv_obj_add_event_cb(keyboard->keyboard, keyboard_event_cb, LV_EVENT_ALL,
                        keyboard);
    keyboard->keyboard_events_registered = true;
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
    /* Keep LVGL's original maps and its event callback. FontAwesome control
     * symbols are part of NP_FONT_ICON, so the map renders intact. */
    lv_keyboard_set_mode(result.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    style_action_keys(result.keyboard);
    lv_obj_set_size(result.keyboard, NP_SCREEN_W, NP_KEYBOARD_H);
    lv_obj_set_style_bg_color(result.keyboard, np_c_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(result.keyboard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(result.keyboard, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(result.keyboard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.keyboard, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(result.keyboard, NP_SP_8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.keyboard, NP_SP_8, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.keyboard, NP_SP_8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(result.keyboard, np_c_surface_raised(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(result.keyboard, np_c_text(), LV_PART_ITEMS);
    /* A single source font gives Latin keys and LVGL's control symbols the
     * same line height and baseline, keeping every glyph centered. */
    lv_obj_set_style_text_font(result.keyboard, &ui_font_np_keyboard_20,
                               LV_PART_ITEMS);
    lv_obj_set_style_radius(result.keyboard, NP_RADIUS_CONTROL, LV_PART_ITEMS);
    lv_obj_set_style_border_width(result.keyboard, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(result.keyboard, np_c_hairline(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(result.keyboard, np_c_accent_bg(),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(result.keyboard, np_c_accent(),
                                LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(result.keyboard, np_c_accent(),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(result.keyboard, np_c_accent_active(),
                              LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(result.keyboard, np_c_text_on_accent(),
                                LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(result.keyboard, np_c_accent(),
                                  LV_PART_ITEMS | LV_STATE_PRESSED);
    np_set_visible(result.root, false);
    return result;
}

void np_keyboard_bind(np_keyboard_t *keyboard, lv_obj_t *textarea,
                      np_keyboard_mode_t mode)
{
    if (keyboard == NULL || keyboard->keyboard == NULL || textarea == NULL) return;
    ensure_keyboard_events(keyboard);
    if (mode == NP_KEYBOARD_MODE_PASSWORD) {
        lv_textarea_set_password_mode(textarea, true);
    }
    for (uint8_t i = 0; i < NP_KEYBOARD_MAX_BINDINGS; ++i) {
        np_keyboard_binding_t *const binding = &keyboard->bindings[i];
        if (binding->textarea == textarea) {
            binding->mode = mode;
            return;
        }
        if (binding->textarea != NULL) continue;
        *binding = (np_keyboard_binding_t){
            .owner = keyboard,
            .textarea = textarea,
            .mode = mode,
        };
        lv_obj_add_event_cb(textarea, target_event_cb, LV_EVENT_ALL, binding);
        return;
    }
}

void np_keyboard_focus(np_keyboard_t *keyboard, lv_obj_t *textarea,
                       np_keyboard_mode_t mode)
{
    if (keyboard == NULL || keyboard->root == NULL ||
        keyboard->keyboard == NULL || textarea == NULL) return;
    stop_private_input(keyboard);
    keyboard->target = textarea;
    keyboard->interaction_inside_keyboard = false;
    set_mode(keyboard, lv_mode(mode));
    lv_keyboard_set_textarea(keyboard->keyboard, textarea);
    lv_obj_move_foreground(keyboard->root);
    np_set_visible(keyboard->root, true);
}

void np_keyboard_open(np_keyboard_t *keyboard, np_keyboard_mode_t mode,
                      np_keyboard_input_cb_t input, void *user_data)
{
    if (keyboard == NULL || keyboard->root == NULL || keyboard->keyboard == NULL ||
        input == NULL) return;
    ensure_keyboard_events(keyboard);
    np_keyboard_clear_target(keyboard);
    stop_private_input(keyboard);
    keyboard->private_input_active = true;
    keyboard->private_input = input;
    keyboard->private_input_user_data = user_data;
    set_mode(keyboard, lv_mode(mode));
    lv_keyboard_set_textarea(keyboard->keyboard, NULL);
    lv_obj_move_foreground(keyboard->root);
    np_set_visible(keyboard->root, true);
}

void np_keyboard_clear_target(np_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    ++keyboard->lifecycle_generation;
    keyboard->target = NULL;
    keyboard->interaction_inside_keyboard = false;
    keyboard->pressed_key = NULL;
    if (keyboard->keyboard != NULL) lv_keyboard_set_textarea(keyboard->keyboard, NULL);
}

void np_keyboard_hide(np_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    np_keyboard_clear_target(keyboard);
    stop_private_input(keyboard);
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
