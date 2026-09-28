#include "np_preferences.h"

np_preferences_view_t np_preferences_build(lv_obj_t *parent)
{
    static const char *const titles[NP_PREFERENCES_ITEM_COUNT] = {
        "Tela e som", "Wi-Fi", "Fuso horário", "Notificações", "Sistema",
    };
    static const char *const details[NP_PREFERENCES_ITEM_COUNT] = {
        "Ajuste brilho, volume e visualização", "Conecte ou gerencie redes",
        "Defina sua região e horário", "Gerencie alertas e avisos",
        "Informações e manutenção",
    };
    static const char *const icons[NP_PREFERENCES_ITEM_COUNT] = {
        NP_ICON_DISPLAY, NP_ICON_WIFI, NP_ICON_CLOCK,
        NP_ICON_NOTIFICATIONS, NP_ICON_SETTINGS,
    };
    np_preferences_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    view.profile_button = np_button(view.root, 250, 12, 160, NP_TOUCH_TARGET,
                                     "Perfil", false);
    lv_obj_t *panel = np_panel(view.root, NP_SP_24, 80,
                                NP_SCREEN_W - 2 * NP_SP_24, 496);
    lv_obj_t *badge = np_fill(panel, NP_SP_24, NP_SP_16, 64, 64,
                               np_c_accent(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(badge, NP_ICON_SETTINGS, NP_FONT_ICON_BADGE, np_c_text_on_accent(),
              0, (64 - NP_FONT_ICON_BADGE->line_height) / 2,
              64, LV_TEXT_ALIGN_CENTER);
    np_label(panel, "Preferências", NP_FONT_TITLE, np_c_text(),
              112, NP_SP_16, 800, LV_TEXT_ALIGN_LEFT);
    np_label(panel, "Personalize sua experiência no NovaPanel", NP_FONT_SM,
              np_c_text_2(), 112, 60, 800, LV_TEXT_ALIGN_LEFT);
    for (uint8_t i = 0; i < NP_PREFERENCES_ITEM_COUNT; ++i) {
        lv_obj_t *row = np_fill(panel, NP_SP_24, 104 + i * 74, 928, 68,
                                 np_c_surface_subtle(), LV_OPA_COVER,
                                 NP_RADIUS_CONTROL);
        view.rows[i] = row;
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, np_c_hairline(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(row, np_c_accent_bg(),
                                  LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, np_c_accent(),
                                      LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_t *tile = np_fill(row, NP_SP_16, 10, 48, 48,
                                  np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
        /* Decorative objects must let the entire row receive the touch. */
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        const lv_color_t color = i == 3U ? np_c_negative()
                                   : (i == 1U || i == 2U) ? np_c_accent()
                                                         : np_c_text_2();
        np_label(tile, icons[i], NP_FONT_ICON, color,
                  0, (48 - NP_FONT_ICON->line_height) / 2,
                  48, LV_TEXT_ALIGN_CENTER);
        np_label(row, titles[i], NP_FONT_MD, np_c_text(),
                  88, 8, 744, LV_TEXT_ALIGN_LEFT);
        np_label(row, details[i], NP_FONT_SM, np_c_text_2(),
                  88, 38, 744, LV_TEXT_ALIGN_LEFT);
        np_label(row, NP_ICON_ARROW_RIGHT, NP_FONT_ICON, np_c_text_2(),
                  880, (68 - NP_FONT_ICON->line_height) / 2,
                  32, LV_TEXT_ALIGN_CENTER);
    }
    return view;
}
