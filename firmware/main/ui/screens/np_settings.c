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

#define SETTINGS_SYSTEM_MODAL_X     192
#define SETTINGS_SYSTEM_MODAL_Y     116
#define SETTINGS_SYSTEM_MODAL_W     640
#define SETTINGS_SYSTEM_MODAL_H     368

#define SETTINGS_NOTIFICATIONS_MODAL_X 172
#define SETTINGS_NOTIFICATIONS_MODAL_Y 84
#define SETTINGS_NOTIFICATIONS_MODAL_W 680
#define SETTINGS_NOTIFICATIONS_MODAL_H 432

static lv_obj_t *s_system_modal_scrim = NULL;
static lv_obj_t *s_notifications_modal_scrim = NULL;

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static lv_obj_t *settings_panel(lv_obj_t *parent,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
    return np_panel(parent, x, y, w, h);
}

static void settings_toggle(lv_obj_t *parent, int32_t x, int32_t y, bool on)
{
    lv_obj_t *track = np_fill(parent, x, y, 52, 28,
                              on ? np_c_accent() : np_c_hairline(),
                              LV_OPA_COVER, 14);

    np_dot(track, on ? 27 : 3, 3, 22,
           on ? np_c_text_on_accent() : np_c_text_2());
}

static void settings_slider(lv_obj_t *parent,
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
/* Modal Sistema                                                              */
/* -------------------------------------------------------------------------- */

static void settings_modal_close_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *scrim = lv_event_get_user_data(event);
    if (scrim != NULL) {
        np_set_visible(scrim, false);
    }
}

static void settings_modal_open_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *scrim = lv_event_get_user_data(event);
    if (scrim != NULL) {
        lv_obj_move_foreground(scrim);
        np_set_visible(scrim, true);
    }
}

static void settings_system_value(lv_obj_t *modal,
                                  int32_t x,
                                  int32_t y,
                                  int32_t w,
                                  const char *label,
                                  const char *value)
{
    np_label(modal, label, NP_FONT_SM, np_c_text_3(),
             x, y, w, LV_TEXT_ALIGN_LEFT);

    np_label(modal, value, NP_FONT_MD, np_c_text(),
             x, y + 24, w, LV_TEXT_ALIGN_LEFT);
}

static lv_obj_t *settings_build_system_modal(lv_obj_t *root)
{
    lv_obj_t *scrim = np_fill(root,
                              0, 0, NP_SCREEN_W, NP_SCREEN_H,
                              np_c_bg(), LV_OPA_70, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *modal = settings_panel(scrim,
                                     SETTINGS_SYSTEM_MODAL_X, SETTINGS_SYSTEM_MODAL_Y,
                                     SETTINGS_SYSTEM_MODAL_W, SETTINGS_SYSTEM_MODAL_H);

    np_label(modal, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             24, 22, 30, LV_TEXT_ALIGN_CENTER);

    np_label(modal, "Sistema", NP_FONT_LG, np_c_text(),
             68, 18, 300, LV_TEXT_ALIGN_LEFT);

    np_label(modal, "Informacoes do dispositivo", NP_FONT_SM, np_c_text_2(),
             68, 48, 300, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *close = np_icon_button(modal,
                                     SETTINGS_SYSTEM_MODAL_W - 68, 16,
                                     44, NP_ICON_CLOSE);
    lv_obj_add_event_cb(close,
                        settings_modal_close_event_cb,
                        LV_EVENT_CLICKED,
                        scrim);

    np_hline(modal, 24, 82, SETTINGS_SYSTEM_MODAL_W - 48);

    settings_system_value(modal, 32, 110, 250,
                          "Display", "1024x600 · RGB565");

    settings_system_value(modal, 336, 110, 250,
                          "Touch", "Capacitivo");

    settings_system_value(modal, 32, 184, 250,
                          "Firmware", "--");

    settings_system_value(modal, 336, 184, 250,
                          "Temperatura", "--");

    np_vline(modal, 320, 104, 134);

    lv_obj_t *update = np_button(modal,
                                 32, 282,
                                 276, 52,
                                 "Atualizar sistema", true);
    lv_obj_clear_flag(update, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *restart = np_button(modal,
                                  332, 282,
                                  276, 52,
                                  "Reiniciar", false);
    lv_obj_clear_flag(restart, LV_OBJ_FLAG_CLICKABLE);

    np_set_visible(scrim, false);
    return scrim;
}

static lv_obj_t *settings_notification_switch(lv_obj_t *parent,
                                              int32_t x, int32_t y)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_pos(sw, x, y);
    lv_obj_set_size(sw, 52, 28);
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, np_c_text(), LV_PART_KNOB);
    return sw;
}

static lv_obj_t *settings_notification_item(lv_obj_t *modal,
                                            int32_t y,
                                            const char *icon,
                                            lv_color_t icon_color,
                                            const char *title,
                                            const char *detail)
{
    const int32_t row_width = SETTINGS_NOTIFICATIONS_MODAL_W - 48;
    lv_obj_t *row = np_fill(modal, 24, y, row_width, 68,
                            np_c_surface_raised(), LV_OPA_COVER,
                            NP_RADIUS_CONTROL);
    lv_obj_t *icon_tile = np_fill(row, 14, 13, 42, 42,
                                  np_c_surface(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    np_label(icon_tile, icon, NP_FONT_ICON, icon_color,
             0, 9, 42, LV_TEXT_ALIGN_CENTER);
    np_label(row, title, NP_FONT_MD, np_c_text(),
             72, 9, 350, LV_TEXT_ALIGN_LEFT);
    np_label(row, detail, NP_FONT_SM, np_c_text_2(),
             72, 37, 410, LV_TEXT_ALIGN_LEFT);
    return row;
}

static lv_obj_t *settings_build_notifications_modal(lv_obj_t *root,
                                                     np_settings_view_t *view)
{
    lv_obj_t *scrim = np_fill(root, 0, 0, NP_SCREEN_W, NP_SCREEN_H,
                              np_c_bg(), LV_OPA_70, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *modal = settings_panel(scrim,
                                     SETTINGS_NOTIFICATIONS_MODAL_X,
                                     SETTINGS_NOTIFICATIONS_MODAL_Y,
                                     SETTINGS_NOTIFICATIONS_MODAL_W,
                                     SETTINGS_NOTIFICATIONS_MODAL_H);
    lv_obj_t *icon_tile = np_fill(modal, 24, 17, 44, 44,
                                  np_c_accent_bg(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    np_label(icon_tile, NP_ICON_NOTIFICATIONS, NP_FONT_ICON, np_c_accent(),
             0, 10, 44, LV_TEXT_ALIGN_CENTER);
    np_label(modal, "Notificacoes", NP_FONT_LG, np_c_text(),
             84, 16, 360, LV_TEXT_ALIGN_LEFT);
    np_label(modal, "Alertas e som do painel", NP_FONT_SM, np_c_text_2(),
             84, 45, 360, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *close = np_icon_button(modal,
                                     SETTINGS_NOTIFICATIONS_MODAL_W - 68, 16,
                                     44, NP_ICON_CLOSE);
    lv_obj_add_event_cb(close, settings_modal_close_event_cb,
                        LV_EVENT_CLICKED, scrim);
    np_hline(modal, 24, 78, SETTINGS_NOTIFICATIONS_MODAL_W - 48);

    lv_obj_t *general = settings_notification_item(
        modal, 96, NP_ICON_NOTIFICATIONS, np_c_accent(),
        "Notificacoes gerais", "Exibe alertas nao criticos");
    view->notifications_general_switch =
        settings_notification_switch(general, 552, 20);

    lv_obj_t *sound = settings_notification_item(
        modal, 172, NP_ICON_VOLUME_UP, np_c_positive(),
        "Som das notificacoes", "Usa o volume geral configurado");
    view->notifications_sound_switch =
        settings_notification_switch(sound, 552, 20);

    lv_obj_t *system = settings_notification_item(
        modal, 248, NP_ICON_WARNING, np_c_warning(),
        "Alertas do sistema", "Rede, armazenamento e atualizacoes");
    view->notifications_system_switch =
        settings_notification_switch(system, 552, 20);

    np_label(modal, "Teste no volume atual", NP_FONT_SM, np_c_text_3(),
             24, 350, 280, LV_TEXT_ALIGN_LEFT);
    view->notifications_test_button = np_button(modal,
                                                442, 338,
                                                214, 52,
                                                "Testar som", true);
    np_set_visible(scrim, false);
    return scrim;
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
    settings_slider(card,
                    104,
                    NP_ICON_UV,
                    "Brilho da tela",
                    SETTINGS_BRIGHTNESS,
                    &view->brightness_slider,
                    &view->brightness_value,
                    &view->brightness_bubble,
                    &view->brightness_bubble_value);

    settings_slider(card,
                    214,
                    NP_ICON_VOLUME_UP,
                    "Volume geral",
                    SETTINGS_VOLUME,
                    &view->volume_slider,
                    &view->volume_value,
                    &view->volume_bubble,
                    &view->volume_bubble_value);

    np_hline(card,
             SETTINGS_COL_A_X,
             310,
             470);

    np_label(card, "Modo noturno", NP_FONT_MD, np_c_text(),
             SETTINGS_COL_A_TEXT_X, 352, 220, LV_TEXT_ALIGN_LEFT);

    np_label(card, "22:00 - 06:00", NP_FONT_SM, np_c_text_2(),
             SETTINGS_COL_A_TEXT_X, 384, 180, LV_TEXT_ALIGN_LEFT);

    settings_toggle(card, 432, 354, true);

    /* Separador central */
    np_vline(card,
             SETTINGS_DIVIDER_X,
             88,
             380);

    /* Coluna B */
    (void)settings_option_row(card,
                              104,
                              NP_ICON_WIFI,
                              np_c_positive(),
                              "Wi-Fi",
                              "--",
                              true);

    (void)settings_option_row(card,
                              194,
                              NP_ICON_CALENDAR,
                              np_c_text_2(),
                              "Fuso horario",
                              "--",
                              true);

    view->notifications_row = settings_option_row(card,
                            284, NP_ICON_NOTIFICATIONS, np_c_text_2(),
                            "Notificacoes", "--", true);
    view->notifications_value = lv_obj_get_child(view->notifications_row, 2);
    lv_obj_add_flag(view->notifications_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *system_row =
        settings_option_row(card,
                            374,
                            NP_ICON_SETTINGS,
                            np_c_text_2(),
                            "Sistema",
                            "Informacoes e manutencao",
                            true);

    lv_obj_add_flag(system_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(system_row,
                        settings_modal_open_event_cb,
                        LV_EVENT_CLICKED,
                        s_system_modal_scrim);

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

    /*
     * O modal e construido uma unica vez e nasce oculto.
     * Assim o toque em Sistema nao cria arvore LVGL nova.
     */
    s_system_modal_scrim = settings_build_system_modal(view.root);
    view.notifications_modal_scrim = settings_build_notifications_modal(view.root, &view);
    s_notifications_modal_scrim = view.notifications_modal_scrim;

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

    if (s_system_modal_scrim != NULL) {
        np_set_visible(s_system_modal_scrim, false);
    }
    if (s_notifications_modal_scrim != NULL) np_set_visible(s_notifications_modal_scrim, false);
}

lv_obj_t *np_settings_create(lv_obj_t *parent)
{
    return np_settings_build(parent).root;
}
