/* NovaPanel Settings - 1024x600, carregada em etapas pela task LVGL. */
#include <stdio.h>

#include "np_components.h"
#include "np_screens.h"
#include "np_tokens.h"

#define SETTINGS_X             24
#define SETTINGS_Y             82
#define SETTINGS_H             500
#define SETTINGS_GAP           16
#define SETTINGS_LEFT_W        312
#define SETTINGS_MIDDLE_W      312
#define SETTINGS_RIGHT_W       312
#define SETTINGS_MIDDLE_X      (SETTINGS_X + SETTINGS_LEFT_W + SETTINGS_GAP)
#define SETTINGS_RIGHT_X       (SETTINGS_MIDDLE_X + SETTINGS_MIDDLE_W + SETTINGS_GAP)

#define SETTINGS_BRIGHTNESS    78
#define SETTINGS_VOLUME        65

static lv_obj_t *settings_panel(lv_obj_t *parent,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *frame = np_fill(parent, x, y, w, h,
                              np_c_hairline(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    return np_fill(frame, 1, 1, w - 2, h - 2,
                   np_c_surface(), LV_OPA_COVER, NP_RADIUS_SURFACE - 1);
}

static void settings_toggle(lv_obj_t *parent, int32_t x, int32_t y, bool on)
{
    np_fill(parent, x, y, 50, 26,
            on ? np_c_accent() : np_c_hairline(), LV_OPA_COVER, 13);
    np_dot(parent, x + (on ? 27 : 3), y + 3, 20,
           on ? np_c_text_on_accent() : np_c_text_2());
}

static void settings_slider(lv_obj_t *parent, int32_t y,
                            const char *label, uint8_t percent)
{
    const int32_t track_x = 22;
    const int32_t track_w = 266;
    const int32_t fill_w = (track_w * percent) / 100;
    char pct[8] = {0};

    np_label(parent, label, NP_FONT_MD, np_c_text(),
             track_x, y, 190, LV_TEXT_ALIGN_LEFT);
    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    np_label(parent, pct, NP_FONT_MD, np_c_text_2(),
             226, y, 62, LV_TEXT_ALIGN_RIGHT);
    np_fill(parent, track_x, y + 36, track_w, 8,
            np_c_hairline(), LV_OPA_COVER, 4);
    np_fill(parent, track_x, y + 36, fill_w, 8,
            np_c_accent(), LV_OPA_COVER, 4);
    np_dot(parent, track_x + fill_w - 10, y + 30, 20, np_c_accent());
}

static void settings_theme_button(lv_obj_t *parent, int32_t x,
                                  const char *text, bool active)
{
    lv_obj_t *button = np_fill(parent, x, 334, 84, 42,
                               active ? np_c_accent() : np_c_surface_raised(),
                               LV_OPA_COVER, NP_RADIUS_CONTROL);
    if (!active) {
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, np_c_hairline(), 0);
    }
    np_label(button, text, NP_FONT_SM,
             active ? np_c_text_on_accent() : np_c_text_2(),
             0, 12, 84, LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *settings_profile_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root, SETTINGS_X, SETTINGS_Y,
                                    SETTINGS_LEFT_W, SETTINGS_H);

    np_dot(card, 22, 22, 54, np_c_accent());
    np_label(card, "RL", NP_FONT_LG, np_c_text_on_accent(),
             22, 36, 54, LV_TEXT_ALIGN_CENTER);
    np_label(card, "Rafael Lopes", NP_FONT_LG, np_c_text(),
             90, 28, 190, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Perfil padrao", NP_FONT_SM, np_c_text_2(),
             90, 60, 180, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 22, 100, 268);

    np_label(card, "Rede", NP_FONT_MD, np_c_text_3(),
             22, 120, 150, LV_TEXT_ALIGN_LEFT);
    np_label(card, NP_ICON_WIFI, NP_FONT_ICON, np_c_positive(),
             22, 156, 30, LV_TEXT_ALIGN_LEFT);
    np_label(card, "NovaNet 5G", NP_FONT_LG, np_c_text(),
             60, 152, 190, LV_TEXT_ALIGN_LEFT);
    np_label(card, "192.168.0.114 - -52 dBm", NP_FONT_SM, np_c_text_2(),
             60, 184, 220, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Gerenciar redes >", NP_FONT_SM, np_c_accent(),
             22, 216, 200, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 22, 250, 268);

    np_label(card, "Bluetooth", NP_FONT_LG, np_c_text(),
             22, 272, 180, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Desativado", NP_FONT_SM, np_c_text_2(),
             22, 304, 160, LV_TEXT_ALIGN_LEFT);
    settings_toggle(card, 238, 276, false);
    np_hline(card, 22, 350, 268);

    np_label(card, "Fuso horario", NP_FONT_MD, np_c_text_3(),
             22, 372, 180, LV_TEXT_ALIGN_LEFT);
    np_label(card, "America/Sao_Paulo", NP_FONT_LG, np_c_text(),
             22, 404, 260, LV_TEXT_ALIGN_LEFT);
    np_label(card, "UTC-3 - NTP automatico", NP_FONT_SM, np_c_text_2(),
             22, 436, 240, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Trocar fuso >", NP_FONT_SM, np_c_accent(),
             22, 468, 160, LV_TEXT_ALIGN_LEFT);
    return card;
}

static lv_obj_t *settings_display_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root, SETTINGS_MIDDLE_X, SETTINGS_Y,
                                    SETTINGS_MIDDLE_W, SETTINGS_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             22, 24, 30, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Tela e som", NP_FONT_LG, np_c_text(),
             62, 22, 190, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 22, 72, 268);

    settings_slider(card, 110, "Brilho da tela", SETTINGS_BRIGHTNESS);
    settings_slider(card, 212, "Volume geral", SETTINGS_VOLUME);
    np_hline(card, 22, 292, 268);

    np_label(card, "Tema", NP_FONT_MD, np_c_text(),
             22, 314, 120, LV_TEXT_ALIGN_LEFT);
    settings_theme_button(card, 22, "Grafite", true);
    settings_theme_button(card, 114, "Carvao", false);
    settings_theme_button(card, 206, "Ambar", false);
    np_hline(card, 22, 408, 268);

    np_label(card, "Modo noturno", NP_FONT_MD, np_c_text(),
             22, 432, 170, LV_TEXT_ALIGN_LEFT);
    np_label(card, "22:00 - 06:00", NP_FONT_SM, np_c_text_2(),
             22, 464, 170, LV_TEXT_ALIGN_LEFT);
    settings_toggle(card, 240, 430, true);
    return card;
}

static void settings_system_row(lv_obj_t *card, int32_t y,
                                const char *label, const char *value)
{
    np_label(card, label, NP_FONT_SM, np_c_text_2(),
             22, y, 106, LV_TEXT_ALIGN_LEFT);
    np_label(card, value, NP_FONT_SM, np_c_text(),
             128, y, 162, LV_TEXT_ALIGN_RIGHT);
}

static lv_obj_t *settings_system_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root, SETTINGS_RIGHT_X, SETTINGS_Y,
                                    SETTINGS_RIGHT_W, SETTINGS_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             22, 24, 30, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Sistema", NP_FONT_LG, np_c_text(),
             62, 22, 190, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 22, 72, 268);

    settings_system_row(card, 104, "Wi-Fi", "NovaNet 5G");
    settings_system_row(card, 144, "IP", "192.168.0.114");
    settings_system_row(card, 184, "Display", "1024x600 RGB565");
    settings_system_row(card, 224, "Touch", "Capacitivo OK");
    settings_system_row(card, 264, "Temperatura", "48 C");
    settings_system_row(card, 304, "Firmware", "NovaOS v1.3");
    np_hline(card, 22, 342, 268);

    lv_obj_t *update = np_button(card, 22, 364, 268, 52,
                                 "Atualizar sistema", true);
    lv_obj_clear_flag(update, LV_OBJ_FLAG_CLICKABLE);
    np_label(card, "Reiniciar dispositivo", NP_FONT_MD, np_c_text_2(),
             62, 450, 210, LV_TEXT_ALIGN_LEFT);
    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             22, 448, 30, LV_TEXT_ALIGN_LEFT);
    return card;
}

np_settings_view_t np_settings_begin(lv_obj_t *parent)
{
    np_settings_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    return view;
}

bool np_settings_build_next_card(np_settings_view_t *view)
{
    if (view == NULL || view->root == NULL) return false;
    if (view->left_card == NULL) {
        view->left_card = settings_profile_card(view->root);
        np_set_visible(view->left_card, false);
        return true;
    }
    if (view->middle_card == NULL) {
        view->middle_card = settings_display_card(view->root);
        np_set_visible(view->middle_card, false);
        return true;
    }
    if (view->right_card == NULL) {
        view->right_card = settings_system_card(view->root);
        np_set_visible(view->right_card, false);
        return true;
    }
    return false;
}

np_settings_view_t np_settings_build(lv_obj_t *parent)
{
    np_settings_view_t view = np_settings_begin(parent);
    while (np_settings_build_next_card(&view)) {
    }
    return view;
}

void np_settings_reset_stages(np_settings_view_t *view)
{
    if (view == NULL) return;
    np_set_visible(view->left_card, false);
    np_set_visible(view->middle_card, false);
    np_set_visible(view->right_card, false);
}

lv_obj_t *np_settings_create(lv_obj_t *parent)
{
    return np_settings_build(parent).root;
}
