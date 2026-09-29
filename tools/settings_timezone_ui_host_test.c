/* Real LVGL and embedded catalog. No simulated persistence is called durable. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ui/screens/settings/np_settings_timezone.h"

static np_keyboard_t keyboard;
static np_settings_timezone_view_t view;
static uint8_t pixels[1024 * 600 * 2];
static unsigned applies;
static uint16_t submitted;
static esp_err_t apply_result;
static esp_err_t apply(void *data, uint16_t index)
{ (void)data; ++applies; submitted = index; return apply_result; }
static unsigned objects(lv_obj_t *root)
{
    unsigned count = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) count += objects(lv_obj_get_child(root, i));
    return count;
}
static void flush(lv_display_t *d, const lv_area_t *area, uint8_t *buf)
{ (void)area; (void)buf; lv_display_flush_ready(d); }
static void tick(void) { lv_tick_inc(10); lv_timer_handler(); }
static void click(lv_obj_t *obj) { lv_obj_send_event(obj, LV_EVENT_CLICKED, NULL); }
static void check_catalog(const char *file)
{
    FILE *csv = fopen(file, "rb"); assert(csv);
    char line[256], iana[64], posix[72], actual[72];
    uint16_t index = 5;
    while (fgets(line, sizeof(line), csv)) {
        assert(sscanf(line, "\"%63[^\"]\",\"%71[^\"]\"", iana, posix) == 2);
        if (!strcmp(iana, "America/Sao_Paulo") || !strcmp(iana, "America/New_York") ||
            !strcmp(iana, "Europe/London") || !strcmp(iana, "America/Argentina/Buenos_Aires")) continue;
        assert(timezone_catalog_copy_iana(index, actual, sizeof(actual)) && !strcmp(actual, iana));
        assert(timezone_catalog_copy_posix(index, actual, sizeof(actual)) && !strcmp(actual, posix));
        ++index;
    }
    fclose(csv); assert(index == 462);
    const char *legacy[] = {"America/Sao_Paulo", "America/Sao_Paulo",
        "America/Argentina/Buenos_Aires", "America/New_York", "Europe/London"};
    for (uint16_t i = 0; i < 5; ++i)
        assert(timezone_catalog_copy_iana(i, actual, sizeof(actual)) && !strcmp(actual, legacy[i]));
    int16_t offset;
    assert(timezone_catalog_standard_offset(0, &offset) && offset == -180);
    assert(timezone_catalog_standard_offset(3, &offset) && offset == -300);
    assert(timezone_catalog_standard_offset(4, &offset) && offset == 0);
    assert(!strcmp(timezone_catalog_country(0), "Brasil"));
    assert(!timezone_catalog_get(462, &(timezone_catalog_entry_t){0}));
}
int main(int argc, char **argv)
{
    assert(argc == 3); check_catalog(argv[2]);
    lv_init();
    lv_font_glyph_dsc_t glyph;
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON_BADGE, &glyph, 0xe8b5, 0));
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON_BADGE, &glyph, 0xe80b, 0));
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON, &glyph, 0xe80b, 0));
    assert(lv_font_get_glyph_dsc(NP_FONT_ICON, &glyph, 0xe86c, 0));
    lv_display_t *display = lv_display_create(1024, 600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    keyboard = np_keyboard_create(lv_screen_active());
    np_settings_timezone_scene_create(&view, lv_screen_active(), &keyboard, apply, NULL);
    np_set_visible(view.root, true);
    const unsigned count = objects(view.root), callbacks = lv_obj_get_event_count(view.apply_button);
    assert(view.timezone.filtered_count == 462);
    assert(lv_obj_get_child_count(view.timezone.list) == 8); /* spacer + seven */
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        np_settings_timezone_scene_create(&view, lv_screen_active(), &keyboard, apply, NULL);
        assert(lv_obj_get_event_count(view.apply_button) == callbacks);
        np_settings_timezone_scene_enter(&view, 0);
        np_settings_timezone_scene_sync(&view, 0, false, ESP_OK, true);
        click(view.apply_button);
        assert(applies == 0); /* Current preference does not enqueue another write. */
        for (unsigned first = 0; first < 450; first += 17) {
            lv_obj_scroll_to_y(view.timezone.list, first * 54, LV_ANIM_OFF);
            lv_obj_update_layout(view.timezone.list);
            lv_obj_send_event(view.timezone.list, LV_EVENT_SCROLL, NULL);
            assert(objects(view.root) == count);
            assert(view.timezone.rows[0].catalog_index == view.timezone.filtered[view.timezone.viewport_start]);
            assert(lv_label_get_text(view.timezone.rows[0].title) == view.row_titles[0]);
        }
        lv_obj_scroll_to_y(view.timezone.list, 100000, LV_ANIM_OFF);
        lv_obj_update_layout(view.timezone.list);
        lv_obj_send_event(view.timezone.list, LV_EVENT_SCROLL, NULL);
        assert(view.timezone.rows[5].catalog_index == 461);
        lv_textarea_set_text(view.timezone.search, "São Paulo");
        assert(view.timezone.filtered_count == 2);
        lv_textarea_set_text(view.timezone.search, "Brasil");
        assert(view.timezone.filtered_count > 10);
        lv_textarea_set_text(view.timezone.search, "GMT+5:45");
        assert(view.timezone.filtered_count > 0);
        int16_t offset;
        assert(timezone_catalog_standard_offset(view.timezone.filtered[0], &offset) && offset == 345);
        lv_textarea_set_text(view.timezone.search, "not_a_real_city");
        assert(view.timezone.filtered_count == 0 && !lv_obj_has_flag(view.empty_label, LV_OBJ_FLAG_HIDDEN));
        lv_textarea_set_text(view.timezone.search, "London");
        assert(view.timezone.filtered_count >= 1);
        click(view.timezone.rows[0].root);
        assert(view.timezone.selected_index == 4 && !applies);
        click(view.apply_button);
        assert(applies == 1 && submitted == 4 && view.awaiting_projection);
        /* A new local draft while the previous request is being projected
         * must neither be overwritten nor hold the keyboard/apply forever. */
        lv_textarea_set_text(view.timezone.search, "São Paulo");
        click(view.timezone.rows[0].root);
        assert(view.timezone.selected_index == 0);
        np_settings_timezone_scene_sync(&view, 0, false, ESP_OK, true);
        assert(view.awaiting_projection); /* stale projection cannot undo submit */
        np_settings_timezone_scene_sync(&view, 4, true, ESP_OK, true);
        assert(view.pending && !view.awaiting_projection);
        assert(view.timezone.selected_index == 0);
        lv_textarea_set_text(view.timezone.search, "London");
        click(view.timezone.rows[0].root);
        np_settings_timezone_scene_sync(&view, 4, false, ESP_FAIL, true);
        assert(!lv_obj_has_state(view.apply_button, LV_STATE_DISABLED));
        click(view.apply_button);
        np_settings_timezone_scene_sync(&view, 4, false, ESP_OK, true);
        assert(!view.pending && !strcmp(lv_label_get_text(view.persistence_status), "Preferência salva"));
        click(view.timezone.search);
        assert(np_keyboard_is_visible(&keyboard) && keyboard.target == view.timezone.search);
        lv_obj_send_event(view.timezone.search, LV_EVENT_DEFOCUSED, NULL);
        np_keyboard_hide(&keyboard);
        np_set_visible(view.root, false);
        tick();
        assert(!np_keyboard_is_visible(&keyboard) && keyboard.target == NULL);
        np_set_visible(view.root, true);
        assert(objects(view.root) == count);
        applies = 0;
    }
    const clock_t started = clock();
    for (unsigned i = 0; i < 100; ++i) lv_textarea_set_text(view.timezone.search, i % 2 ? "GMT-3" : "Brasil");
    printf("Timezone scene: %u objects; filter mean %.3f ms on host\n", count,
        1000.0 * (clock() - started) / CLOCKS_PER_SEC / 100);
    np_settings_timezone_scene_enter(&view, 0);
    np_settings_timezone_scene_sync(&view, 0, false, ESP_OK, true);
    tick();
    lv_refr_now(display);
    FILE *image = fopen(argv[1], "wb"); assert(image);
    assert(fwrite(pixels, 1, sizeof(pixels), image) == sizeof(pixels)); fclose(image);
    /* A deleted target with pending async is cancelled via binding DELETE. */
    click(view.timezone.search);
    lv_obj_send_event(view.timezone.search, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_delete(view.root);
    tick(); assert(!np_keyboard_is_visible(&keyboard) && keyboard.target == NULL);
    np_keyboard_destroy(&keyboard);
    lv_display_delete(display); lv_deinit();
    puts("Timezone UI: 462 records, fixed pool, search, apply, async and 100 cached cycles PASS");
    return 0;
}
