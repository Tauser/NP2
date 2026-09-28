#include "np_form.h"

static lv_font_t s_text_with_icons;
static bool s_text_with_icons_ready;

const lv_font_t *np_form_text_font(void)
{
    if (!s_text_with_icons_ready) {
        s_text_with_icons = *NP_FONT_SM;
        s_text_with_icons.fallback = NP_FONT_ICON;
        s_text_with_icons_ready = true;
    }
    return &s_text_with_icons;
}

void np_form_apply_field_style(lv_obj_t *field)
{
    if (field == NULL) return;
    lv_obj_set_style_bg_color(field, np_c_surface_raised(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(field, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(field, NP_RADIUS_CONTROL, LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(field, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_outline_width(field, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(field, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(field, np_form_text_font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(field, np_c_text(), LV_PART_MAIN);
    lv_obj_set_style_pad_left(field, NP_SP_16, LV_PART_MAIN);
    lv_obj_set_style_pad_right(field, NP_SP_16, LV_PART_MAIN);
    lv_obj_set_style_pad_top(field, (NP_INPUT_H - NP_FONT_SM->line_height) / 2,
                             LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(field, (NP_INPUT_H - NP_FONT_SM->line_height) / 2,
                                LV_PART_MAIN);
    lv_obj_set_style_border_width(field, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(field, np_c_accent(),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(field, np_c_text_3(),
                                LV_PART_TEXTAREA_PLACEHOLDER);
}

lv_obj_t *np_form_text_input(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t w, int32_t h, const char *placeholder,
                              const char *leading_icon)
{
    if (parent == NULL) return NULL;
    lv_obj_t *const input = lv_textarea_create(parent);
    if (input == NULL) return NULL;
    lv_obj_remove_style_all(input);
    lv_obj_set_pos(input, x, y);
    lv_obj_set_size(input, w, h);
    np_form_apply_field_style(input);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_placeholder_text(input, placeholder != NULL ? placeholder : "");
    if (leading_icon != NULL && leading_icon[0] != '\0') {
        lv_obj_set_style_pad_left(input, NP_SP_48, LV_PART_MAIN);
        lv_obj_t *const icon = lv_label_create(input);
        if (icon != NULL) {
            lv_obj_set_style_text_font(icon, NP_FONT_ICON, 0);
            lv_obj_set_style_text_color(icon, np_c_text_2(), 0);
            lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(icon, leading_icon);
            lv_obj_set_pos(icon, NP_SP_16,
                           (h - NP_FONT_ICON->line_height) / 2);
            lv_obj_set_size(icon, NP_FONT_ICON->line_height,
                            NP_FONT_ICON->line_height);
            lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    return input;
}

lv_obj_t *np_form_dropdown(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h)
{
    if (parent == NULL) return NULL;
    lv_obj_t *const dropdown = lv_dropdown_create(parent);
    if (dropdown == NULL) return NULL;
    lv_obj_remove_style_all(dropdown);
    lv_obj_set_pos(dropdown, x, y);
    lv_obj_set_size(dropdown, w, h);
    np_form_apply_field_style(dropdown);
    lv_obj_set_style_bg_color(dropdown, np_c_surface_raised(), LV_PART_SELECTED);
    lv_obj_set_style_text_font(dropdown, np_form_text_font(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(dropdown, np_c_text(), LV_PART_SELECTED);
    lv_dropdown_set_symbol(dropdown, NP_ICON_ARROW_DOWN);
    return dropdown;
}

void np_form_apply_button_style(lv_obj_t *button, np_form_button_kind_t kind)
{
    if (button == NULL) return;
    const bool primary = kind == NP_FORM_BUTTON_PRIMARY;
    const bool destructive = kind == NP_FORM_BUTTON_DESTRUCTIVE;
    const lv_color_t background = primary ? np_c_accent()
                                : destructive ? np_c_negative_bg()
                                              : np_c_surface_raised();
    const lv_color_t border = destructive ? np_c_negative() : np_c_hairline();
    const lv_color_t text = primary ? np_c_text_on_accent()
                                    : destructive ? np_c_negative() : np_c_text();

    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, background, LV_PART_MAIN);
    lv_obj_set_style_radius(button, NP_RADIUS_CONTROL, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, primary ? 0 : 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, border, LV_PART_MAIN);
    lv_obj_set_style_outline_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(button, NP_FONT_MD, LV_PART_MAIN);
    lv_obj_set_style_text_color(button, text, LV_PART_MAIN);

    lv_obj_set_style_bg_color(button, primary ? np_c_accent_active()
                                               : np_c_surface_subtle(),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, destructive ? np_c_negative()
                                                       : np_c_accent(),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, primary ? 0 : 2,
                                  LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, destructive ? np_c_negative()
                                                       : np_c_accent(),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(button, primary ? 0 : 2,
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(button, np_c_surface(),
                              LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, np_c_text_disabled(),
                                LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_border_color(button, np_c_hairline(),
                                  LV_PART_MAIN | LV_STATE_DISABLED);
}

lv_obj_t *np_form_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t w, int32_t h, const char *text,
                         np_form_button_kind_t kind)
{
    if (parent == NULL) return NULL;
    lv_obj_t *const button = lv_button_create(parent);
    if (button == NULL) return NULL;
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    np_form_apply_button_style(button, kind);
    lv_obj_t *const label = lv_label_create(button);
    if (label != NULL) {
        lv_label_set_text(label, text != NULL ? text : "");
        lv_obj_center(label);
    }
    return button;
}

lv_obj_t *np_form_icon_button(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t size, const char *icon)
{
    if (parent == NULL || size <= 0) return NULL;
    lv_obj_t *const button = lv_button_create(parent);
    if (button == NULL) return NULL;
    lv_obj_remove_style_all(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, size, size);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, np_c_accent_bg(),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(button, np_c_accent(),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_t *const label = lv_label_create(button);
    if (label != NULL) {
        lv_label_set_text(label, icon != NULL ? icon : "");
        lv_obj_set_style_text_font(label, NP_FONT_ICON, 0);
        lv_obj_set_style_text_color(label, np_c_text_2(), 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0, (size - NP_FONT_ICON->line_height) / 2);
        lv_obj_set_size(label, size, NP_FONT_ICON->line_height);
        lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    }
    return button;
}
