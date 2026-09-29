/* Real LVGL widgets and scene code; these tests do not simulate a radio. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/screens/settings/np_settings_wifi.h"
#include "ui/screens/settings/np_wifi_password.h"

static unsigned submits, cancels;
static bool action(np_wifi_password_action_t kind, char character)
{
    (void)character;
    if (kind == NP_WIFI_PASSWORD_SUBMIT) ++submits;
    if (kind == NP_WIFI_PASSWORD_CANCEL) ++cancels;
    return true;
}
static uint32_t objects(lv_obj_t *root)
{
    uint32_t count = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) count += objects(lv_obj_get_child(root, i));
    return count;
}
static void click(lv_obj_t *obj) { lv_obj_send_event(obj, LV_EVENT_CLICKED, NULL); }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)area; (void)pixels;
    lv_display_flush_ready(display);
}
static np_settings_wifi_view_t view;
static np_wifi_password_t password;
static np_keyboard_t keyboard;
static np_settings_wifi_add_t add;
static uint8_t pixels[1024 * 600 * 2];
int main(int argc, char **argv)
{
    lv_init();
    const uint32_t wifi_glyphs[] = {0xe63e, 0xe4ca, 0xe4d9, 0xe648};
    lv_font_glyph_dsc_t glyph = {0};
    for (unsigned i = 0; i < sizeof(wifi_glyphs) / sizeof(wifi_glyphs[0]); ++i)
        assert(lv_font_get_glyph_dsc(NP_FONT_ICON_BADGE, &glyph, wifi_glyphs[i], 0));
    const uint32_t action_glyphs[] = {0xe157, 0xe328, 0xe202, 0xe872};
    for (unsigned i = 0; i < sizeof(action_glyphs) / sizeof(action_glyphs[0]); ++i)
        assert(lv_font_get_glyph_dsc(NP_FONT_ICON, &glyph, action_glyphs[i], 0));
    lv_display_t *display = lv_display_create(1024, 600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    keyboard = np_keyboard_create(lv_screen_active());
    connectivity_scan_result_t results[40] = {0};
    for (unsigned i = 0; i < 40; ++i) {
        snprintf(results[i].ssid, sizeof(results[i].ssid), "Rede de teste %02u", i);
        results[i].rssi = -40 - i;
        results[i].secure = i != 1;
    }
    const uint32_t baseline = objects(lv_screen_active());
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        view = np_settings_wifi_scene_build(lv_screen_active());
        np_settings_wifi_scene_bind(&view);
        np_settings_wifi_scene_bind(&view); /* Idempotent registration. */
        const uint32_t built = objects(view.root);
        np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_ONLINE, results[0].ssid,
            40, results, "192.0.2.2", "192.0.2.1", false);
        click(view.wifi.network_rows[0]);
        assert(strcmp(view.wifi.selected_ssid, results[0].ssid) == 0);
        np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_ONLINE, "Fora da busca",
            0, NULL, "192.0.2.3", "192.0.2.1", false);
        click(view.current_row);
        assert(strcmp(lv_label_get_text(view.ip), "192.0.2.3") == 0);
        assert(!lv_obj_has_state(view.wifi.forget_button, LV_STATE_DISABLED));
        assert(lv_obj_has_state(view.wifi.connect_button, LV_STATE_DISABLED));
        np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_ONLINE, results[0].ssid,
            40, results, "192.0.2.2", "192.0.2.1", false);
        click(view.wifi.network_rows[0]);
        assert(strcmp(lv_label_get_text(view.ip), "192.0.2.2") == 0);
        assert(!lv_obj_has_state(view.wifi.forget_button, LV_STATE_DISABLED));
        assert(lv_obj_has_state(view.wifi.connect_button, LV_STATE_DISABLED));
        click(view.wifi.network_rows[1]);
        assert(!view.wifi.network_secure[1]);
        assert(!lv_obj_has_state(view.wifi.connect_button, LV_STATE_DISABLED));
        assert(lv_obj_has_state(view.wifi.forget_button, LV_STATE_DISABLED));
        assert(strcmp(lv_label_get_text(view.ip), "Não disponível") == 0);
        for (unsigned page = 1; page < 10; ++page) {
            click(view.next_page);
            assert(view.page == page);
            assert(strcmp(lv_label_get_text(view.wifi.network_names[0]), results[page * 4].ssid) == 0);
            assert(objects(view.root) == built);
        }
        click(view.next_page);
        assert(view.page == 9);
        click(view.current_row);
        assert(view.page == 0);
        assert(strcmp(view.wifi.selected_ssid, results[0].ssid) == 0);
        np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_OFFLINE, "", 1, results, "", "", false);
        assert(view.page == 0);
        assert(strcmp(lv_label_get_text(view.ip), "Não disponível") == 0);
        np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_FAILED, "", 0, NULL, "", "", false);
        assert(lv_obj_has_state(view.wifi.connect_button, LV_STATE_DISABLED));
        assert(objects(view.root) == built);
        if (cycle == 0) {
            printf("Wi-Fi scene: %u LVGL objects including shared header\n", (unsigned)built);
            np_settings_wifi_scene_sync(&view, NP_WIFI_STATUS_ONLINE, results[0].ssid,
                40, results, "192.0.2.2", "192.0.2.1", false);
            click(view.current_row);
            lv_refr_now(display);
            if (argc > 1) {
                FILE *file = fopen(argv[1], "wb"); assert(file);
                assert(fwrite(pixels, 1, sizeof(pixels), file) == sizeof(pixels)); fclose(file);
            }
        }
        np_wifi_password_create(&password, view.root, &keyboard, results[0].ssid, true, action, NULL);
        assert(lv_textarea_get_one_line(password.field));
        assert(lv_textarea_get_password_mode(password.field));
        assert(np_keyboard_is_visible(&keyboard));
        assert(keyboard.target == NULL && keyboard.private_input_active);
        np_wifi_password_sync(&password, 63, false);
        assert(strlen(lv_textarea_get_text(password.field)) == 63);
        for (const char *p = lv_textarea_get_text(password.field); *p; ++p) assert(*p == '*');
        lv_textarea_set_cursor_pos(password.field, 0);
        lv_textarea_add_text(password.field, "unexpected");
        assert(strlen(lv_textarea_get_text(password.field)) == 63);
        np_wifi_password_sync(&password, 8, true);
        assert(!lv_textarea_get_password_mode(password.field));
        assert(strcmp(lv_textarea_get_text(password.field), "********") == 0);
        click(password.connect);
        assert(!np_keyboard_is_visible(&keyboard));
        assert(keyboard.target == NULL && !keyboard.private_input_active);
        assert(lv_textarea_get_text(password.field)[0] == '\0');
        lv_obj_delete(password.modal.scrim);
        password = (np_wifi_password_t){0};
        /* Same cleanup ordering used on navigation in product_ui. */
        np_wifi_password_create(&password, view.root, &keyboard, results[1].ssid, true, action, NULL);
        np_keyboard_hide(&keyboard);
        np_settings_wifi_add_create(&add, view.root, &keyboard);
        lv_textarea_set_text(add.ssid, "Rede manual");
        assert(keyboard.target == add.ssid && !keyboard.private_input_active);
        assert(lv_obj_has_state(add.secure_switch, LV_STATE_CHECKED));
        np_modal_hide(&add.modal);
        assert(keyboard.target == NULL && !np_keyboard_is_visible(&keyboard));
        assert(lv_textarea_get_text(add.ssid)[0] == '\0');
        lv_obj_delete(add.modal.scrim);
        for (unsigned i = 0; i < NP_KEYBOARD_MAX_BINDINGS; ++i)
            assert(keyboard.bindings[i].textarea == NULL);
        add = (np_settings_wifi_add_t){0};
        lv_obj_delete(view.root);
        password = (np_wifi_password_t){0}; view = (np_settings_wifi_view_t){0};
        assert(keyboard.target == NULL && !keyboard.private_input_active);
        assert(objects(lv_screen_active()) == baseline);
    }
    assert(submits == 100 && cancels == 100);
    np_keyboard_destroy(&keyboard);
    puts("Wi-Fi UI: pagination, selection, data gating, masking and 100 lifecycle cycles passed");
    return 0;
}
