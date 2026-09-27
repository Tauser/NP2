/* NovaPanel Settings - Dark Graphite 1024x600.
 *
 * Layout de Configurações:
 *   Header compartilhado                          0..64
 *   Tela e som       24,76   976x260
 *   Conectividade    24,352  480x224
 *   Sistema         520,352  480x224
 *
 * O drawer pertence ao np_header() e permanece oculto por padrao.
 * Esta tela nao cria rail/menu lateral permanente.
 */
#include <stdio.h>

#include "np_components.h"
#include "np_screens.h"
#include "np_tokens.h"

#ifndef NP_ICON_VOLUME_UP
#define NP_ICON_VOLUME_UP "\xEE\x81\x90" /* U+E050 volume_up */
#endif

#define SETTINGS_LEFT_X       24
#define SETTINGS_RIGHT_X      520
#define SETTINGS_TOP_Y        76
#define SETTINGS_GAP          16

#define SETTINGS_LEFT_W       976
#define SETTINGS_RIGHT_W      480
#define SETTINGS_CONTENT_H    260

#define SETTINGS_NETWORK_H    224
#define SETTINGS_SYSTEM_Y     (SETTINGS_TOP_Y + SETTINGS_CONTENT_H + SETTINGS_GAP)
#define SETTINGS_SYSTEM_H     224

#define SETTINGS_TOP_DIVIDER_X 524

#define SETTINGS_BRIGHTNESS   78
#define SETTINGS_VOLUME       65

static lv_obj_t *settings_panel(lv_obj_t *parent,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
    return np_panel(parent, x, y, w, h);
}

static void settings_section_title(lv_obj_t *parent,
                                   const char *icon,
                                   const char *title,
                                   const char *subtitle,
                                   int32_t line_w)
{
    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(),
             24, 24, 28, LV_TEXT_ALIGN_CENTER);
    np_label(parent, title, NP_FONT_LG, np_c_text(),
             68, 20, line_w - 68, LV_TEXT_ALIGN_LEFT);

    const bool has_subtitle = subtitle != NULL && subtitle[0] != '\0';
    if (has_subtitle) {
        np_label(parent, subtitle, NP_FONT_SM, np_c_text_2(),
                 68, 52, line_w - 68, LV_TEXT_ALIGN_LEFT);
    }

    np_hline(parent, 24, has_subtitle ? 84 : 68, line_w - 48);
}

static void settings_toggle(lv_obj_t *parent, int32_t x, int32_t y, bool on)
{
    np_fill(parent, x, y, 52, 28,
            on ? np_c_accent() : np_c_hairline(), LV_OPA_COVER, 14);
    np_dot(parent, x + (on ? 27 : 3), y + 3, 22,
           on ? np_c_text_on_accent() : np_c_text_2());
}

static void settings_slider(lv_obj_t *parent,
                            int32_t y,
                            const char *icon,
                            const char *label,
                            uint8_t percent)
{
    const int32_t icon_x = 28;
    const int32_t text_x = 88;
    const int32_t track_w = 408;
    const int32_t fill_w = (track_w * percent) / 100;
    char pct[8] = {0};

    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(),
             icon_x, y + 6, 28, LV_TEXT_ALIGN_CENTER);
    np_label(parent, label, NP_FONT_MD, np_c_text(),
             text_x, y, 270, LV_TEXT_ALIGN_LEFT);

    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    np_label(parent, pct, NP_FONT_MD, np_c_text_2(),
             420, y, 76, LV_TEXT_ALIGN_RIGHT);

    np_fill(parent, text_x, y + 42, track_w, 8,
            np_c_hairline(), LV_OPA_COVER, 4);
    np_fill(parent, text_x, y + 42, fill_w, 8,
            np_c_accent(), LV_OPA_COVER, 4);
    np_dot(parent, text_x + fill_w - 10, y + 36, 20, np_c_accent());
}

static lv_obj_t *settings_display_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_LEFT_X, SETTINGS_TOP_Y,
                                    SETTINGS_LEFT_W, SETTINGS_CONTENT_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             24, 24, 28, LV_TEXT_ALIGN_CENTER);
    np_label(card, "Tela e som", NP_FONT_LG, np_c_text(),
             68, 20, 300, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Ajuste brilho, volume e modo noturno.", NP_FONT_SM, np_c_text_2(),
             68, 52, 440, LV_TEXT_ALIGN_LEFT);

    settings_slider(card, 124,
                    NP_ICON_UV,
                    "Brilho da tela",
                    SETTINGS_BRIGHTNESS);

    settings_slider(card, 196,
                    NP_ICON_VOLUME_UP,
                    "Volume geral",
                    SETTINGS_VOLUME);

    np_vline(card, SETTINGS_TOP_DIVIDER_X, 126, 108);
    np_label(card, "Modo noturno", NP_FONT_MD, np_c_text(),
             570, 146, 220, LV_TEXT_ALIGN_LEFT);
    np_label(card, "22:00 - 06:00", NP_FONT_SM, np_c_text_2(),
             570, 178, 180, LV_TEXT_ALIGN_LEFT);
    settings_toggle(card, 900, 150, true);

    return card;
}

static void settings_connection_row(lv_obj_t *card,
                                    int32_t y,
                                    const char *icon,
                                    lv_color_t icon_color,
                                    const char *label,
                                    const char *value,
                                    const char *detail)
{
    np_label(card, icon, NP_FONT_ICON, icon_color,
             28, y, 28, LV_TEXT_ALIGN_CENTER);
    np_label(card, label, NP_FONT_MD, np_c_text(),
             88, y, 110, LV_TEXT_ALIGN_LEFT);
    np_label(card, value, NP_FONT_MD, np_c_text(),
             216, y, 200, LV_TEXT_ALIGN_RIGHT);
    np_label(card, ">", NP_FONT_LG, np_c_text_2(),
             428, y - 3, 24, LV_TEXT_ALIGN_RIGHT);

    if (detail != NULL && detail[0] != '\0') {
        np_label(card, detail, NP_FONT_SM, np_c_text_2(),
                 216, y + 24, 200, LV_TEXT_ALIGN_RIGHT);
    }
}

static lv_obj_t *settings_connectivity_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_RIGHT_X, SETTINGS_TOP_Y,
                                    SETTINGS_RIGHT_W, SETTINGS_NETWORK_H);

    settings_section_title(card, NP_ICON_WIFI, "Conectividade",
                           "Rede, Bluetooth e fuso horário.",
                           SETTINGS_RIGHT_W);

    settings_connection_row(card, 98,
                            NP_ICON_WIFI, np_c_positive(),
                            "Wi-Fi", "NovaNet 5G", "192.168.0.114 · -52 dBm");

    settings_connection_row(card, 150,
                            NP_ICON_BLUETOOTH, np_c_accent(),
                            "Bluetooth", "Desativado", NULL);

    settings_connection_row(card, 188,
                            NP_ICON_CALENDAR, np_c_text_2(),
                            "Fuso", "America/Sao_Paulo", NULL);

    return card;
}

static lv_obj_t *settings_system_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_RIGHT_X, SETTINGS_SYSTEM_Y,
                                    SETTINGS_RIGHT_W, SETTINGS_SYSTEM_H);

    settings_section_title(card, NP_ICON_SETTINGS, "Sistema",
                           "Atualização e reinício do painel.",
                           SETTINGS_RIGHT_W);

    lv_obj_t *update = np_button(card, 24, 132, 260, 52,
                                 "Atualizar", true);
    lv_obj_clear_flag(update, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *restart = np_button(card, 300, 132, 156, 52,
                                  "Reiniciar", false);
    lv_obj_clear_flag(restart, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

np_settings_view_t np_settings_begin(lv_obj_t *parent)
{
    np_settings_view_t view = {0};
    view.root = np_scene(parent);

    /* A tela continua criada oculta para o fluxo lazy/staged do product_ui. */
    np_set_visible(view.root, false);

    /* np_header() ja cria o drawer fechado. Nao forcar rail ativo aqui. */
    view.header = np_header(view.root);
    view.home_button = NULL;

    return view;
}

bool np_settings_build_next_card(np_settings_view_t *view)
{
    if (view == NULL || view->root == NULL) return false;

    if (view->left_card == NULL) {
        view->left_card = settings_display_card(view->root);
        np_set_visible(view->left_card, false);
        return true;
    }
    if (view->middle_card == NULL) {
        view->middle_card = settings_connectivity_card(view->root);
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
