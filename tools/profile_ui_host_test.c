#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/screens/np_profile.h"

static uint8_t pixels[1024 * 600 * 2];
static np_keyboard_t keyboard;
static np_profile_view_t view;

static unsigned objects(lv_obj_t *root)
{
    unsigned count = 1U;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        count += objects(lv_obj_get_child(root, i));
    return count;
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *buffer)
{
    (void)area; (void)buffer; lv_display_flush_ready(display);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(1024, 600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    keyboard = np_keyboard_create(lv_screen_active());
    view = np_profile_build(lv_screen_active());
    np_set_visible(view.root, true);
    user_profile_t saved = {.name = "Tauser Gonçalves", .avatar_color = 2U};
    np_profile_sync(&view, true, &saved, false, ESP_OK);
    assert(!strcmp(lv_label_get_text(view.greeting), "Olá, Tauser!"));
    assert(!strcmp(lv_label_get_text(view.avatar_initials), "TG"));
    const unsigned initial = objects(view.root);
    np_profile_open_editor(&view, &keyboard, true);
    assert(np_modal_is_visible(&view.editor));
    assert(np_keyboard_is_visible(&keyboard) && keyboard.target == view.name_input);
    assert(!strcmp(lv_textarea_get_text(view.name_input), saved.name));
    assert(view.color_buttons[0] != NULL);
    lv_obj_send_event(view.color_buttons[1], LV_EVENT_CLICKED, NULL);
    assert(view.draft_color == 1U);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(view.color_buttons[1], 0)),
                   NP_ICON_CHECK));
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(view.color_buttons[2], 0)), ""));
    const unsigned edited = objects(view.root);
    assert(edited > initial);
    for (unsigned i = 0; i < 50; ++i) {
        lv_textarea_set_text(view.name_input, "Ana Silva");
        user_profile_t draft = {0};
        assert(np_profile_editor_value(&view, &draft));
        assert(!strcmp(draft.name, "Ana Silva"));
        np_profile_close_editor(&view);
        assert(!np_keyboard_is_visible(&keyboard) && keyboard.target == NULL);
        np_profile_open_editor(&view, &keyboard, false);
        assert(objects(view.root) == edited);
    }
    np_profile_close_editor(&view);
    lv_obj_delete(view.root);
    lv_tick_inc(10);
    lv_timer_handler();
    assert(keyboard.target == NULL);
    np_keyboard_destroy(&keyboard);
    puts("Profile UI lifecycle: PASS");
    return 0;
}
