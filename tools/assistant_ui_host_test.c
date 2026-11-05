#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/screens/np_assistant.h"

static uint8_t pixels[1024 * 600 * 2];
static lv_point_t point;
static bool down;
static unsigned taps, closes;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data)
{ (void)a; (void)data; lv_display_flush_ready(d); }
static void advance(unsigned ms)
{ for (unsigned i = 0; i < ms; i += 10) { lv_tick_inc(10); lv_timer_handler(); } }
static void read_touch(lv_indev_t *d, lv_indev_data_t *data)
{ (void)d; data->point = point; data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED; }
static void touch(lv_indev_t *d, int x, int y, bool pressed)
{ point = (lv_point_t){x, y}; down = pressed; lv_indev_read(d); advance(40); }
static void action(np_assistant_tap_t tap, void *user)
{ assert(user == &taps); if (tap == NP_ASSISTANT_TAP_ORB) ++taps; else ++closes; }
static void bounds(np_assistant_view_t *v)
{
    lv_obj_update_layout(v->root);
    lv_area_t a, b;
    lv_obj_get_coords(v->root, &a); lv_obj_get_coords(v->orb, &b);
    assert(a.x1 >= 16 && a.x2 < 1008 && a.y1 >= 16 && a.y2 < 584);
    assert(b.x1 >= 16 && b.x2 < 1008 && b.y1 >= 16 && b.y2 < 584);
    assert(a.x2 < b.x1 || b.x2 < a.x1 || a.y2 < b.y1 || b.y2 < a.y1);
    assert(lv_obj_get_width(v->orb) == 68 && lv_obj_get_height(v->orb) == 68);
}
static void drag(lv_indev_t *d, np_assistant_view_t *v, int x, int y)
{
    touch(d, v->x + 34, v->y + 34, true);
    touch(d, x, y, true); touch(d, x, y, false);
    assert(taps == 1 && closes == 0); bounds(v);
}
int main(int argc, char **argv)
{
    assert(argc == 2); lv_init();
    lv_display_t *display = lv_display_create(1024, 600);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    lv_indev_t *indev = lv_indev_create(); lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display); lv_indev_set_read_cb(indev, read_touch);
    np_assistant_view_t v = {0};
    assert(np_assistant_build(&v, lv_layer_top(), action, &taps));
    np_assistant_set_available(&v, true); advance(100);
    assert(lv_obj_has_flag(v.root, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(v.orb, LV_OBJ_FLAG_HIDDEN));
    /* Touches directly over an eye must reach the circle as well. */
    touch(indev, v.x + 25, v.y + 30, true); touch(indev, v.x + 25, v.y + 30, false);
    assert(taps == 1); np_assistant_set_open(&v, true);
    assert(np_assistant_owns_touch(&v, v.left_eye));
    assert(np_assistant_owns_touch(&v, v.close));
    assert(!np_assistant_owns_touch(&v, lv_screen_active()));
    drag(indev, &v, -100, -100); assert(v.x == 16 && v.y == 16);
    drag(indev, &v, 1200, 800); assert(v.x == 940 && v.y == 516);
    drag(indev, &v, 500, 300);
    const int32_t x = v.x, y = v.y;
    xiaozhi_projection_t p = {0};
    for (unsigned state = 0; state <= XIAOZHI_STATE_STOPPING; ++state) {
        p.state = (xiaozhi_state_t)state; ++p.revision;
        strcpy(p.activation_code, "123456");
        np_assistant_sync(&v, &p, true); advance(5100);
        bounds(&v); assert(v.x == x && v.y == y);
        assert(lv_obj_get_height(v.left_eye) > 0 && lv_obj_get_height(v.left_eye) <= 22);
    }
    p.state = XIAOZHI_STATE_LISTENING; ++p.revision;
    strcpy(p.assistant_text, "Arraste o circulo. A conversa acompanha voce.");
    np_assistant_sync(&v, &p, true); advance(200);
    FILE *file = fopen(argv[1], "wb"); assert(file);
    assert(fwrite(pixels, 1, sizeof(pixels), file) == sizeof(pixels)); fclose(file);
    lv_area_t close_area; lv_obj_get_coords(v.close, &close_area);
    touch(indev, close_area.x1 + 24, close_area.y1 + 24, true);
    touch(indev, close_area.x1 + 24, close_area.y1 + 24, false); assert(closes == 1);
    np_assistant_set_open(&v, false); assert(!lv_obj_has_flag(v.orb, LV_OBJ_FLAG_HIDDEN));
    np_assistant_set_available(&v, false); assert(lv_obj_has_flag(v.orb, LV_OBJ_FLAG_HIDDEN));
    np_assistant_destroy(&v); advance(100);
    lv_mem_monitor_t before, after; lv_mem_monitor(&before);
    for (unsigned i = 0; i < 100; ++i) {
        assert(np_assistant_build(&v, lv_layer_top(), action, &taps));
        np_assistant_set_available(&v, true); advance(100);
        np_assistant_destroy(&v); advance(100);
    }
    lv_mem_monitor(&after); assert(after.free_size >= before.free_size);
    /* Parent deletion also cancels the timer before view storage can disappear. */
    lv_obj_t *parent = lv_obj_create(lv_layer_top());
    assert(np_assistant_build(&v, parent, action, &taps));
    lv_obj_delete(parent); assert(v.eye_timer == NULL && v.orb == NULL && v.root == NULL);
    np_assistant_destroy(&v);
    advance(200);
    puts("Assistant UI PASS: real pointer tap/drag, clamping, no voice action on drag, capsule bounds, all eye states, 100 lifecycles");
    return 0;
}
