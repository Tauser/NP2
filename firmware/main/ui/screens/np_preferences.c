#include "np_preferences.h"

np_preferences_view_t np_preferences_build(lv_obj_t *parent)
{
    static const char *const titles[NP_PREFERENCES_ITEM_COUNT] = {
        "Tela e som", "Wi-Fi", "Fuso horário", "Notificações", "Sistema",
    };
    static const char *const details[NP_PREFERENCES_ITEM_COUNT] = {
        "Brilho, volume e modo noturno", "Rede e conectividade",
        "Região e horário local", "Alertas e som do painel",
        "Informações e manutenção",
    };
    static const char *const icons[NP_PREFERENCES_ITEM_COUNT] = {
        NP_ICON_VOLUME_UP, NP_ICON_WIFI, NP_ICON_CALENDAR,
        NP_ICON_NOTIFICATIONS, NP_ICON_SETTINGS,
    };
    np_preferences_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    view.profile_button = np_button(view.root, 250, 12, 160, NP_TOUCH_TARGET,
                                     "Perfil", false);
    np_label(view.root, "Preferências", NP_FONT_TITLE, np_c_text(),
              24, 84, 976, LV_TEXT_ALIGN_LEFT);
    for (uint8_t i = 0; i < NP_PREFERENCES_ITEM_COUNT; ++i) {
        lv_obj_t *row = np_panel(view.root, 24, 136 + i * 88, 976, 80);
        view.rows[i] = row;
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        np_label(row, icons[i], NP_FONT_ICON, np_c_accent(),
                  24, 28, 32, LV_TEXT_ALIGN_CENTER);
        np_label(row, titles[i], NP_FONT_MD, np_c_text(),
                  80, 12, 780, LV_TEXT_ALIGN_LEFT);
        np_label(row, details[i], NP_FONT_SM, np_c_text_2(),
                  80, 46, 780, LV_TEXT_ALIGN_LEFT);
        np_label(row, NP_ICON_ARROW_RIGHT, NP_FONT_ICON, np_c_text_2(),
                  920, 28, 32, LV_TEXT_ALIGN_CENTER);
    }
    return view;
}
