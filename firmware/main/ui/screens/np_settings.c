/* NovaPanel Settings - Dark Graphite 1024x600.
 *
 * Layout aprovado:
 *   Header compartilhado                          0..64
 *   Tela e som       24,76   540x500
 *   Conectividade   580,76   420x190
 *   Sistema         580,282  420x294
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
#define SETTINGS_RIGHT_X      580
#define SETTINGS_TOP_Y        76
#define SETTINGS_GAP          16

#define SETTINGS_LEFT_W       540
#define SETTINGS_RIGHT_W      420
#define SETTINGS_CONTENT_H    500

#define SETTINGS_NETWORK_H    174
#define SETTINGS_SYSTEM_Y     (SETTINGS_TOP_Y + SETTINGS_NETWORK_H + SETTINGS_GAP)
#define SETTINGS_SYSTEM_H     310

#define SETTINGS_BRIGHTNESS   78
#define SETTINGS_VOLUME       65

static lv_obj_t *settings_panel(lv_obj_t *parent,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *frame = np_fill(parent, x, y, w, h,
                              np_c_hairline(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    return np_fill(frame, 1, 1, w - 2, h - 2,
                   np_c_surface(), LV_OPA_COVER, NP_RADIUS_SURFACE - 1);
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

    if (subtitle != NULL && subtitle[0] != '\0') {
        np_label(parent, subtitle, NP_FONT_SM, np_c_text_2(),
                 68, 52, line_w - 68, LV_TEXT_ALIGN_LEFT);
    }

    np_hline(parent, 24, 84, line_w - 48);
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
                            const char *helper,
                            uint8_t percent)
{
    const int32_t icon_x = 24;
    const int32_t text_x = 68;
    const int32_t track_w = 424;
    const int32_t fill_w = (track_w * percent) / 100;
    char pct[8] = {0};

    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(),
             icon_x, y + 2, 28, LV_TEXT_ALIGN_CENTER);
    np_label(parent, label, NP_FONT_MD, np_c_text(),
             text_x, y, 270, LV_TEXT_ALIGN_LEFT);

    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    np_label(parent, pct, NP_FONT_MD, np_c_text_2(),
             430, y, 62, LV_TEXT_ALIGN_RIGHT);

    np_fill(parent, text_x, y + 40, track_w, 8,
            np_c_hairline(), LV_OPA_COVER, 4);
    np_fill(parent, text_x, y + 40, fill_w, 8,
            np_c_accent(), LV_OPA_COVER, 4);
    np_dot(parent, text_x + fill_w - 10, y + 34, 20, np_c_accent());

    if (helper != NULL && helper[0] != '\0') {
        np_label(parent, helper, NP_FONT_SM, np_c_text_2(),
                 text_x, y + 62, 360, LV_TEXT_ALIGN_LEFT);
    }
}

static void settings_theme_button(lv_obj_t *parent,
                                  int32_t x, int32_t y, int32_t w,
                                  const char *text, bool active)
{
    lv_obj_t *button = np_fill(parent, x, y, w, 48,
                               active ? np_c_accent() : np_c_surface_raised(),
                               LV_OPA_COVER, NP_RADIUS_CONTROL);

    if (!active) {
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, np_c_hairline(), 0);
    }

    np_label(button, text, NP_FONT_MD,
             active ? np_c_text_on_accent() : np_c_text_2(),
             0, 13, w, LV_TEXT_ALIGN_CENTER);
}

static lv_obj_t *settings_display_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_LEFT_X, SETTINGS_TOP_Y,
                                    SETTINGS_LEFT_W, SETTINGS_CONTENT_H);

    settings_section_title(card,
                           NP_ICON_SETTINGS,
                           "Tela e som",
                           "Ajuste brilho, volume e preferencias de visualizacao.",
                           SETTINGS_LEFT_W);

    settings_slider(card, 118,
                    NP_ICON_UV,
                    "Brilho da tela",
                    "Ajusta o brilho do display.",
                    SETTINGS_BRIGHTNESS);

    settings_slider(card, 224,
                    NP_ICON_VOLUME_UP,
                    "Volume geral",
                    "Ajusta o volume do sistema.",
                    SETTINGS_VOLUME);

    np_hline(card, 24, 310, SETTINGS_LEFT_W - 48);

    np_label(card, "Tema", NP_FONT_MD, np_c_text(),
             68, 330, 160, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Escolha o tema visual da interface.", NP_FONT_SM, np_c_text_2(),
             68, 360, 350, LV_TEXT_ALIGN_LEFT);

    settings_theme_button(card, 68, 392, 136, "Grafite", true);
    settings_theme_button(card, 216, 392, 136, "Carvão", false);
    settings_theme_button(card, 364, 392, 136, "Âmbar", false);

    np_hline(card, 24, 454, SETTINGS_LEFT_W - 48);

    np_label(card, "Modo noturno", NP_FONT_MD, np_c_text(),
             68, 468, 220, LV_TEXT_ALIGN_LEFT);
    np_label(card, "22:00 - 06:00", NP_FONT_SM, np_c_text_2(),
             290, 471, 150, LV_TEXT_ALIGN_LEFT);
    settings_toggle(card, 460, 464, true);

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
             24, y, 28, LV_TEXT_ALIGN_CENTER);
    np_label(card, label, NP_FONT_MD, np_c_text(),
             68, y, 118, LV_TEXT_ALIGN_LEFT);
    np_label(card, value, NP_FONT_MD, np_c_text(),
             190, y, 200, LV_TEXT_ALIGN_RIGHT);

    if (detail != NULL && detail[0] != '\0') {
        np_label(card, detail, NP_FONT_SM, np_c_text_2(),
                 190, y + 24, 200, LV_TEXT_ALIGN_RIGHT);
    }
}

static lv_obj_t *settings_connectivity_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_RIGHT_X, SETTINGS_TOP_Y,
                                    SETTINGS_RIGHT_W, SETTINGS_NETWORK_H);

    np_label(card, NP_ICON_WIFI, NP_FONT_ICON, np_c_accent(),
             24, 18, 28, LV_TEXT_ALIGN_CENTER);
    np_label(card, "Conectividade", NP_FONT_LG, np_c_text(),
             68, 14, 230, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Rede, Bluetooth e fuso horário.", NP_FONT_SM, np_c_text_2(),
             68, 46, 300, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 24, 72, SETTINGS_RIGHT_W - 48);

    settings_connection_row(card, 86,
                            NP_ICON_WIFI, np_c_positive(),
                            "Wi-Fi", "NovaNet 5G", "192.168.0.114 · -52 dBm");

    settings_connection_row(card, 126,
                            NP_ICON_BLUETOOTH, np_c_accent(),
                            "Bluetooth", "Desativado", NULL);

    settings_connection_row(card, 150,
                            NP_ICON_CALENDAR, np_c_text_2(),
                            "Fuso", "America/Sao_Paulo", NULL);

    return card;
}

static void settings_system_row(lv_obj_t *card,
                                int32_t y,
                                const char *label,
                                const char *value)
{
    np_label(card, label, NP_FONT_SM, np_c_text_2(),
             24, y, 118, LV_TEXT_ALIGN_LEFT);
    np_label(card, value, NP_FONT_SM, np_c_text(),
             144, y, 246, LV_TEXT_ALIGN_RIGHT);
}

static lv_obj_t *settings_system_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_RIGHT_X, SETTINGS_SYSTEM_Y,
                                    SETTINGS_RIGHT_W, SETTINGS_SYSTEM_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             24, 18, 28, LV_TEXT_ALIGN_CENTER);
    np_label(card, "Sistema", NP_FONT_LG, np_c_text(),
             68, 14, 180, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Informações do dispositivo e manutenção.", NP_FONT_SM, np_c_text_2(),
             68, 46, 300, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 24, 72, SETTINGS_RIGHT_W - 48);

    settings_system_row(card, 84,  "Display",      "1024x600 · RGB565");
    settings_system_row(card, 112, "Touch",        "Capacitivo · OK");
    settings_system_row(card, 140, "Firmware",     "NovaOS v1.3");
    settings_system_row(card, 168, "Ativo há",     "14 d 06 h 22 min");
    settings_system_row(card, 196, "Temperatura",  "48 °C");

    np_hline(card, 24, 220, SETTINGS_RIGHT_W - 48);

    lv_obj_t *update = np_button(card, 24, 230, SETTINGS_RIGHT_W - 48, 40,
                                 "Atualizar sistema", true);
    lv_obj_clear_flag(update, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *restart = np_button(card, 24, 278, SETTINGS_RIGHT_W - 48, 30,
                                  "Reiniciar dispositivo", false);
    lv_obj_clear_flag(restart, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

np_settings_view_t np_settings_build(lv_obj_t *parent)
{
    np_settings_view_t view = {0};
    view.root = np_scene(parent);

    /* A tela continua criada oculta para o fluxo lazy/staged do product_ui. */
    np_set_visible(view.root, false);

    /* np_header() ja cria o drawer fechado. Nao forcar rail ativo aqui. */
    view.header = np_header(view.root);
    view.home_button = NULL;

    /* Mantemos os mesmos handles para nao quebrar np_settings_view_t e o
     * staged reveal existente: left / middle / right. */
    view.left_card = settings_display_card(view.root);
    view.middle_card = settings_connectivity_card(view.root);
    view.right_card = settings_system_card(view.root);

    np_set_visible(view.left_card, false);
    np_set_visible(view.middle_card, false);
    np_set_visible(view.right_card, false);

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
