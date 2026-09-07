/*
 * Controlled Phase 2 diagnostic UI.
 *
 * Input events are emitted from the LVGL input pipeline. The callback therefore
 * runs in the LVGL task and may update only this diagnostic view; it performs
 * no network, filesystem, NVS, or flash operation.
 */
#include <stdio.h>

#include "esp_check.h"
#include "lvgl.h"

#include "diagnostic_ui.h"

#define DIAG_TARGET_SIZE 104
#define DIAG_TARGET_MARGIN 28

typedef struct {
    lv_obj_t *coordinate_label;
    lv_obj_t *state_label;
    lv_obj_t *targets[5];
    uint32_t sample_count;
} diagnostic_ui_state_t;

static diagnostic_ui_state_t s_state;

static void set_target_state(lv_obj_t *target, bool active)
{
    if (target == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(target, active ? lv_color_hex(0x126A50) : lv_color_hex(0x183554),
                               LV_PART_MAIN);
    lv_obj_set_style_border_color(target, active ? lv_color_hex(0x68E0B8) : lv_color_hex(0x6491C6),
                                   LV_PART_MAIN);
}

static void reset_targets_except(lv_obj_t *active_target)
{
    for (size_t i = 0; i < sizeof(s_state.targets) / sizeof(s_state.targets[0]); ++i) {
        set_target_state(s_state.targets[i], s_state.targets[i] == active_target);
    }
}

static void touch_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *const indev = lv_event_get_target(event);
    lv_point_t point = {0};
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        s_state.sample_count++;
        lv_label_set_text_fmt(s_state.coordinate_label, "x=%d  y=%d  amostras=%lu",
                              (int)point.x, (int)point.y,
                              (unsigned long)s_state.sample_count);

        lv_obj_t *const active_target = lv_event_get_param(event);
        reset_targets_except(active_target);
        lv_label_set_text(s_state.state_label, "TOQUE ATIVO — arraste para validar o percurso");
    } else if (code == LV_EVENT_RELEASED) {
        lv_label_set_text_fmt(s_state.state_label, "LIBERADO EM x=%d  y=%d", (int)point.x,
                              (int)point.y);
        reset_targets_except(NULL);
    }
}

static lv_obj_t *create_target(lv_obj_t *parent, const char *text, lv_align_t align,
                               int32_t x_offset, int32_t y_offset)
{
    lv_obj_t *const target = lv_button_create(parent);
    lv_obj_set_size(target, DIAG_TARGET_SIZE, DIAG_TARGET_SIZE);
    lv_obj_align(target, align, x_offset, y_offset);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(target, 3, LV_PART_MAIN);
    set_target_state(target, false);

    lv_obj_t *const label = lv_label_create(target);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(label);
    return target;
}

esp_err_t diagnostic_ui_create(lv_display_t *display, lv_indev_t *touch_indev)
{
    ESP_RETURN_ON_FALSE(display != NULL && touch_indev != NULL, ESP_ERR_INVALID_ARG,
                        "diag_ui", "Display or touch handle missing");

    s_state = (diagnostic_ui_state_t){0};

    lv_obj_t *const screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x09111F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *const title = lv_label_create(screen);
    lv_label_set_text(title, "NP2  |  DIAGNOSTICO DE TOUCH");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *const instruction = lv_label_create(screen);
    lv_label_set_text(instruction, "Toque os quatro cantos, o centro e arraste entre eles");
    lv_obj_set_style_text_color(instruction, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 52);

    s_state.coordinate_label = lv_label_create(screen);
    lv_label_set_text(s_state.coordinate_label, "x=--  y=--  amostras=0");
    lv_obj_set_style_text_color(s_state.coordinate_label, lv_color_hex(0x68E0B8), LV_PART_MAIN);
    lv_obj_align(s_state.coordinate_label, LV_ALIGN_TOP_MID, 0, 86);

    s_state.state_label = lv_label_create(screen);
    lv_label_set_text(s_state.state_label, "AGUARDANDO TOQUE");
    lv_obj_set_style_text_color(s_state.state_label, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_align(s_state.state_label, LV_ALIGN_BOTTOM_MID, 0, -22);

    s_state.targets[0] = create_target(screen, "SE", LV_ALIGN_TOP_LEFT, DIAG_TARGET_MARGIN,
                                       130);
    s_state.targets[1] = create_target(screen, "SD", LV_ALIGN_TOP_RIGHT, -DIAG_TARGET_MARGIN,
                                       130);
    s_state.targets[2] = create_target(screen, "IE", LV_ALIGN_BOTTOM_LEFT, DIAG_TARGET_MARGIN,
                                       -78);
    s_state.targets[3] = create_target(screen, "ID", LV_ALIGN_BOTTOM_RIGHT, -DIAG_TARGET_MARGIN,
                                       -78);
    s_state.targets[4] = create_target(screen, "CENTRO", LV_ALIGN_CENTER, 0, 0);

    lv_indev_add_event_cb(touch_indev, touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(touch_indev, touch_event_cb, LV_EVENT_PRESSING, NULL);
    lv_indev_add_event_cb(touch_indev, touch_event_cb, LV_EVENT_RELEASED, NULL);

    return ESP_OK;
}
