/* Real LVGL rendering, bounded history and animation lifecycle checks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/screens/settings/np_settings_system.h"

static uint8_t pixels[1024 * 600 * 2];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    (void)area; (void)data;
    lv_display_flush_ready(display);
}
static void advance(unsigned ms)
{
    for (unsigned i = 0; i < ms; i += 10U) { lv_tick_inc(10); lv_timer_handler(); }
}
static void check_model(void)
{
    uint8_t percent = 0;
    assert(system_used_percent(100, 100) == 0);
    assert(system_used_percent(100, 0) == 100);
    assert(system_used_percent(UINT64_C(32) << 30U, UINT64_C(24) << 30U) == 25);
    assert(system_used_percent(0, 0) == 0);
    assert(!system_cpu_window(100, 0, 50, 0, &percent));
    assert(system_cpu_window(2000000, 0, 1600000, 0, &percent) && percent == 20);
    /* Both clock and idle counter cross UINT32_MAX. */
    assert(system_cpu_window(1499999, UINT32_MAX - 500000U,
                             1199999, UINT32_MAX - 400000U, &percent) && percent == 20);
    assert(!system_cpu_window(40000000, 0, 20000000, 0, &percent));
    assert(system_cpu_window(2000000, 0, 2000001, 0, &percent) && percent == 0);
}
static void screenshot(const char *path)
{
    advance(200);
    FILE *file = fopen(path, "wb");
    assert(file != NULL && fwrite(pixels, 1, sizeof(pixels), file) == sizeof(pixels));
    fclose(file);
}
int main(int argc, char **argv)
{
    check_model();
    lv_init();
    lv_font_glyph_dsc_t glyph = {0};
    const uint32_t icons[] = {0xe322, 0xf7a3, 0xe1db, 0xe623, 0xe5d5, 0xf8c7};
    for (unsigned i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i) {
        assert(lv_font_get_glyph_dsc(NP_FONT_ICON, &glyph, icons[i], 0));
        assert(!glyph.is_placeholder);
        assert(glyph.resolved_font == (icons[i] == 0xe5d5 ? NP_FONT_ICON : &ui_font_np_material_system_24));
        assert(glyph.box_w > 0 && glyph.box_h > 0 && glyph.box_h <= 24);
    }
    lv_display_t *display = lv_display_create(1024, 600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    /* FULL only in this desktop off-screen test; hardware remains partial. */
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    np_settings_system_view_t view = {0};
    np_settings_system_scene_create(&view, lv_screen_active());
    lv_obj_t *root = view.root;
    np_settings_system_scene_create(&view, lv_screen_active());
    assert(root == view.root);
    np_set_visible(view.root, true);
    lv_obj_update_layout(view.root);
    np_settings_system_sync(&view.system, "system-resource-test", "46 °C", false);
    system_resource_snapshot_t r = {
        .available = true, .cpu_available = true, .cpu_percent = 18,
        .cpu_core_percent = {12, 24}, .sequence = 1, .uptime_s = 194400,
        .internal_total_bytes = 400 * 1024, .internal_free_bytes = 148 * 1024,
        .internal_largest_bytes = 96 * 1024,
        .psram_total_bytes = 32 * 1024 * 1024, .psram_free_bytes = 19818086,
        .storage_available = true, .storage_total_bytes = 9 * 1024 * 1024,
        .storage_used_bytes = 1887437,
        .sd_mounted = true, .sd_available = true,
        .sd_total_bytes = UINT64_C(31890132172), .sd_free_bytes = UINT64_C(28454158336),
    };
    np_settings_system_sync_resources(&view.system, &r);
    assert(lv_arc_get_value(view.system.cpu_arc) == 18);
    assert(lv_obj_has_state(view.update_button, LV_STATE_DISABLED));
    assert(lv_obj_get_height(view.system.restart_button) == NP_TOUCH_TARGET);
    for (unsigned i = 0; i < 30; ++i) {
        ++r.sequence; r.uptime_s += 2; r.cpu_percent = 10 + (i * 7) % 30;
        r.cpu_core_percent[0] = r.cpu_percent - 6;
        r.cpu_core_percent[1] = r.cpu_percent + 6;
        np_settings_system_sync_resources(&view.system, &r);
        advance(200);
    }
    assert(lv_chart_get_point_count(view.system.cpu_chart) == SYSTEM_CPU_HISTORY_COUNT);
    if (argc > 1) screenshot(argv[1]);
    r.cpu_percent = 90; ++r.sequence;
    np_settings_system_sync_resources(&view.system, &r);
    assert(lv_anim_get(view.system.cpu_arc, NULL) != NULL);
    np_settings_system_pause(&view.system);
    np_set_visible(view.root, false);
    assert(lv_anim_get(view.system.cpu_arc, NULL) == NULL);
    np_settings_system_sync_resources(&view.system, &r);
    assert(lv_anim_get(view.system.cpu_arc, NULL) == NULL);
    np_set_visible(view.root, true);
    r.maintenance = true;
    np_settings_system_sync_resources(&view.system, &r);
    assert(lv_anim_get(view.system.cpu_arc, NULL) == NULL);
    r.sd_mounted = false; r.sd_available = false; r.storage_available = false;
    r.cpu_available = false;
    np_settings_system_sync_resources(&view.system, &r);
    assert(strcmp(lv_label_get_text(view.system.cpu_value), "--") == 0);
    assert(strcmp(lv_label_get_text(view.system.sd_status), "Ausente") == 0);
    assert(lv_bar_get_value(view.system.sd_bar) == 0);
    if (argc > 2) screenshot(argv[2]);
    np_settings_system_sync(&view.system, "test", "--", true);
    assert(lv_obj_has_state(view.system.restart_button, LV_STATE_DISABLED));
    for (unsigned i = 0; i < 100; ++i) {
        r.maintenance = false; ++r.sequence; r.cpu_available = true; r.cpu_percent = i;
        np_settings_system_sync_resources(&view.system, &r);
        np_settings_system_pause(&view.system);
        np_set_visible(view.root, false); advance(200); np_set_visible(view.root, true);
    }
    lv_obj_delete(view.root);
    advance(200);
    assert(lv_anim_count_running() == 0);
    puts("System UI: CPU rollover, unavailable data, bounded history, maintenance and 100 navigation cycles passed");
    return 0;
}
