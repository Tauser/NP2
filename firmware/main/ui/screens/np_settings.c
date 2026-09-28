/* NovaPanel Settings - Dark Graphite 1024x600
 *
 * Layout simplificado:
 *   - Header compartilhado (drawer oculto por padrao)
 *   - Um unico card "Geral" em duas colunas
 *
 * Coluna A:
 *   - Brilho da tela
 *   - Volume geral
 *   - Modo noturno
 *
 * Coluna B:
 *   - Wi-Fi
 *   - Fuso horario
 *   - Sistema >  (abre modal)
 *
 * Tema removido.
 * Sistema deixa de ocupar um card permanente e passa a ser modal.
 *
 * Mantem compatibilidade com o contrato atual de np_settings_view_t:
 * left_card / middle_card / right_card apontam para o mesmo card Geral,
 * evitando quebrar o fluxo staged/lazy existente no product_ui.
 */
#include <stdio.h>

#include "np_components.h"
#include "np_screens.h"
#include "np_tokens.h"

#ifndef NP_ICON_VOLUME_UP
#define NP_ICON_VOLUME_UP "\xEE\x81\x90" /* U+E050 volume_up */
#endif

#define SETTINGS_GENERAL_X          24
#define SETTINGS_GENERAL_Y          76
#define SETTINGS_GENERAL_W          976
#define SETTINGS_GENERAL_H          500

#define SETTINGS_DIVIDER_X          560

#define SETTINGS_COL_A_X            28
#define SETTINGS_COL_A_TEXT_X       84
#define SETTINGS_COL_A_TRACK_W      414

#define SETTINGS_COL_B_X            596
#define SETTINGS_COL_B_W            352

#define SETTINGS_BRIGHTNESS         60
#define SETTINGS_VOLUME             65

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static lv_obj_t *settings_panel(lv_obj_t *parent,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
    return np_panel(parent, x, y, w, h);
}

static lv_obj_t *settings_option_row(lv_obj_t *parent,
                                     int32_t y,
                                     const char *icon,
                                     lv_color_t icon_color,
                                     const char *label,
                                     const char *value,
                                     bool show_chevron)
{
    lv_obj_t *row = np_fill(parent,
                            SETTINGS_COL_B_X, y,
                            SETTINGS_COL_B_W, 84,
                            np_c_surface_raised(), LV_OPA_COVER,
                            NP_RADIUS_CONTROL);

    np_label(row, icon, NP_FONT_ICON, icon_color,
             18, 28, 28, LV_TEXT_ALIGN_CENTER);

    np_label(row, label, NP_FONT_MD, np_c_text(),
             58, 18, 142, LV_TEXT_ALIGN_LEFT);

    if (value != NULL && value[0] != '\0') {
        np_label(row, value, NP_FONT_SM, np_c_text_2(),
                 58, 48, show_chevron ? 230 : 262,
                 LV_TEXT_ALIGN_LEFT);
    }

    if (show_chevron) {
        np_label(row, ">", NP_FONT_LG, np_c_text_2(),
                 306, 27, 24, LV_TEXT_ALIGN_CENTER);
    }

    return row;
}

/* -------------------------------------------------------------------------- */
/* Card Geral                                                                 */
/* -------------------------------------------------------------------------- */

static lv_obj_t *settings_general_card(np_settings_view_t *view)
{
    lv_obj_t *card = settings_panel(view->root,
                                    SETTINGS_GENERAL_X,
                                    SETTINGS_GENERAL_Y,
                                    SETTINGS_GENERAL_W,
                                    SETTINGS_GENERAL_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             24, 20, 30, LV_TEXT_ALIGN_CENTER);

    np_label(card, "Geral", NP_FONT_LG, np_c_text(),
             68, 16, 300, LV_TEXT_ALIGN_LEFT);

    np_hline(card, 24, 64, SETTINGS_GENERAL_W - 48);

    /* Coluna A */
    np_settings_display_sound_slider(card,
                    104,
                    NP_ICON_UV,
                    "Brilho da tela",
                    SETTINGS_BRIGHTNESS,
                    &view->controls.brightness_slider,
                    &view->controls.brightness_value,
                    &view->controls.brightness_bubble,
                    &view->controls.brightness_bubble_value);

    np_settings_display_sound_slider(card,
                    214,
                    NP_ICON_VOLUME_UP,
                    "Volume geral",
                    SETTINGS_VOLUME,
                    &view->controls.volume_slider,
                    &view->controls.volume_value,
                    &view->controls.volume_bubble,
                    &view->controls.volume_bubble_value);

    np_hline(card,
             SETTINGS_COL_A_X,
             310,
             470);

    np_settings_display_sound_night(card, 84, 352, 432, &view->controls);

    /* Separador central */
    np_vline(card,
             SETTINGS_DIVIDER_X,
             88,
             380);

    /* Coluna B */
    view->wifi_row = settings_option_row(card,
                                         104,
                                         NP_ICON_WIFI,
                                         np_c_positive(),
                                         "Wi-Fi",
                                         "--",
                                         true);
    view->wifi_value = lv_obj_get_child(view->wifi_row, 2);
    lv_obj_add_flag(view->wifi_row, LV_OBJ_FLAG_CLICKABLE);

    view->timezone_row = settings_option_row(card,
                                              194,
                                              NP_ICON_LOCATION,
                                              np_c_text_2(),
                                              "Fuso horario",
                                              "--",
                                              true);
    view->timezone_value = lv_obj_get_child(view->timezone_row, 2);
    lv_obj_add_flag(view->timezone_row, LV_OBJ_FLAG_CLICKABLE);

    view->notifications_row = settings_option_row(card,
                            284, NP_ICON_NOTIFICATIONS, np_c_text_2(),
                            "Notificacoes", "--", true);
    view->notifications_value = lv_obj_get_child(view->notifications_row, 2);
    lv_obj_add_flag(view->notifications_row, LV_OBJ_FLAG_CLICKABLE);
    view->system_row = settings_option_row(card,
                                           374,
                                           NP_ICON_SETTINGS,
                                           np_c_text_2(),
                                           "Sistema",
                                           "Informacoes e manutencao",
                                           true);
    lv_obj_add_flag(view->system_row, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

/* -------------------------------------------------------------------------- */
/* Build / compatibilidade com staged reveal                                  */
/* -------------------------------------------------------------------------- */

np_settings_view_t np_settings_begin(lv_obj_t *parent)
{
    np_settings_view_t view = {0};

    view.root = np_scene(parent);
    np_set_visible(view.root, false);

    /* np_header() mantem o drawer fechado por padrao. */
    view.header = np_header(view.root);
    view.home_button = NULL;

    return view;
}

bool np_settings_build_next_card(np_settings_view_t *view)
{
    if (view == NULL || view->root == NULL) {
        return false;
    }

    if (view->left_card == NULL) {
        view->left_card = settings_general_card(view);

        /*
         * A nova tela tem apenas um card principal.
         * Os handles antigos apontam para o mesmo card para preservar
         * compatibilidade com codigo staged existente.
         */
        view->middle_card = view->left_card;
        view->right_card = view->left_card;

        np_set_visible(view->left_card, false);
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
    if (view == NULL) {
        return;
    }

    if (view->left_card != NULL) {
        np_set_visible(view->left_card, false);
    }

    np_settings_system_hide(&view->system);
    np_settings_notifications_hide(&view->notifications);
    np_settings_timezone_hide(&view->timezone);
    np_settings_wifi_hide(&view->wifi);
}

lv_obj_t *np_settings_create(lv_obj_t *parent)
{
    return np_settings_build(parent).root;
}
