#include <stdio.h>
#include "np_settings_display_sound.h"
/* Extracted from np_settings.c, shared with the legacy screen. */
#define SETTINGS_COL_A_X 28
#define SETTINGS_COL_A_TEXT_X 84
#define SETTINGS_COL_A_TRACK_W 414
void np_settings_display_sound_slider(lv_obj_t *parent,
                            int32_t y,
                            const char *icon,
                            const char *label,
                            uint8_t percent,
                            lv_obj_t **out_slider,
                            lv_obj_t **out_value,
                            lv_obj_t **out_bubble,
                            lv_obj_t **out_bubble_value)
{
    char pct[8] = {0};

    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(),
             SETTINGS_COL_A_X, y + 3, 30, LV_TEXT_ALIGN_CENTER);

    np_label(parent, label, NP_FONT_MD, np_c_text(),
             SETTINGS_COL_A_TEXT_X, y, 260, LV_TEXT_ALIGN_LEFT);

    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    lv_obj_t *value = np_label(parent, pct, NP_FONT_MD, np_c_text_2(),
                                430, y, 68, LV_TEXT_ALIGN_RIGHT);

    /* Nao ha wrapper para slider no catalogo atual. A configuracao local
     * preserva a geometria do controle estatico anterior e so cria um
     * objeto interativo quando a Settings e aberta. */
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_pos(slider, SETTINGS_COL_A_TEXT_X, y + 34);
    lv_obj_set_size(slider, SETTINGS_COL_A_TRACK_W, 20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, np_c_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, np_c_accent(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_width(slider, 20, LV_PART_KNOB);
    lv_obj_set_style_height(slider, 20, LV_PART_KNOB);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);

    lv_obj_t *bubble = np_fill(parent, SETTINGS_COL_A_TEXT_X, y, 56, 28,
                                np_c_accent_bg(), LV_OPA_COVER,
                                NP_RADIUS_CONTROL);
    lv_obj_t *bubble_value = np_label(bubble, pct, NP_FONT_SM, np_c_text(),
                                      0, 5, 56, LV_TEXT_ALIGN_CENTER);
    np_set_visible(bubble, false);

    if (out_slider != NULL) {
        *out_slider = slider;
    }
    if (out_value != NULL) {
        *out_value = value;
    }
    if (out_bubble != NULL) {
        *out_bubble = bubble;
    }
    if (out_bubble_value != NULL) {
        *out_bubble_value = bubble_value;
    }
}

void np_settings_display_sound_night(lv_obj_t *parent, int32_t text_x,
    int32_t y, int32_t switch_x, np_display_sound_controls_t *controls)
{
    np_label(parent, "Modo noturno", NP_FONT_MD, np_c_text(),
             text_x, y, 220, LV_TEXT_ALIGN_LEFT);

    controls->night_detail = np_label(parent, "22:00 - 06:00 · brilho ate 15%", NP_FONT_SM,
                                  np_c_text_2(), text_x, y + 32, 350,
                                  LV_TEXT_ALIGN_LEFT);
    controls->night_switch = lv_switch_create(parent);
    lv_obj_set_pos(controls->night_switch, switch_x, y + 2);
    lv_obj_set_size(controls->night_switch, 52, 28);
    lv_obj_set_style_bg_color(controls->night_switch, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(controls->night_switch, np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(controls->night_switch, np_c_text(), LV_PART_KNOB);

}
np_settings_display_sound_view_t np_settings_display_sound_build(lv_obj_t *parent)
{
    np_settings_display_sound_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    view.back_button = np_button(view.root, NP_HEADER_NAV_X, 12, NP_HEADER_NAV_W, NP_TOUCH_TARGET,
                                  "Voltar", false);
    lv_obj_t *panel = np_panel(view.root, 24, 80, 976, 496);
    lv_obj_t *badge = np_fill(panel, 24, 16, 64, 64, np_c_accent(),
                               LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(badge, NP_ICON_DISPLAY, NP_FONT_ICON, np_c_text_on_accent(),
              0, (64 - NP_FONT_ICON->line_height) / 2, 64, LV_TEXT_ALIGN_CENTER);
    np_label(panel, "Tela e som", NP_FONT_TITLE, np_c_text(),
              112, 16, 800, LV_TEXT_ALIGN_LEFT);
    np_label(panel, "Ajuste o brilho, volume e modo noturno da tela", NP_FONT_SM,
              np_c_text_2(), 112, 60, 800, LV_TEXT_ALIGN_LEFT);
    np_hline(panel, 24, 104, 928);
    np_settings_display_sound_slider(panel, 156, NP_ICON_UV, "Brilho da tela", 0,
        &view.controls.brightness_slider, &view.controls.brightness_value,
        &view.controls.brightness_bubble, &view.controls.brightness_bubble_value);
    np_hline(panel, 28, 274, 470);
    np_settings_display_sound_slider(panel, 324, NP_ICON_VOLUME_UP, "Volume geral", 0,
        &view.controls.volume_slider, &view.controls.volume_value,
        &view.controls.volume_bubble, &view.controls.volume_bubble_value);
    np_vline(panel, 520, 104, 368);
    np_label(panel, NP_ICON_NIGHT, NP_FONT_ICON, np_c_text_2(),
              548, 159, 30, LV_TEXT_ALIGN_CENTER);
    np_settings_display_sound_night(panel, 596, 156, 896, &view.controls);
    return view;
}
