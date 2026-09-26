/* NovaPanel Settings - Dark Graphite 1024x600.
 *
 * A tela e montada oculta e os cards sao revelados em etapas pelo
 * product_ui. Isso evita um primeiro frame com dezenas de glifos ao mesmo
 * tempo, que pode monopolizar a task LVGL e disparar o task watchdog.
 */
#include <stdio.h>

#include "np_components.h"
#include "np_screens.h"
#include "np_tokens.h"

#ifndef NP_ICON_VOLUME_UP
#define NP_ICON_VOLUME_UP "\xEE\x81\x90" /* U+E050 volume_up */
#endif

#define SETTINGS_RAIL_X       24
#define SETTINGS_RAIL_Y       88
#define SETTINGS_RAIL_W       72
#define SETTINGS_RAIL_H       488

#define SETTINGS_CONTENT_X    112
#define SETTINGS_CONTENT_Y    88
#define SETTINGS_CONTENT_H    488
#define SETTINGS_GAP          16
#define SETTINGS_CARD_LEFT_W  282
#define SETTINGS_CARD_MID_W   282
#define SETTINGS_CARD_RIGHT_W 292

#define SETTINGS_CARD_MID_X   (SETTINGS_CONTENT_X + SETTINGS_CARD_LEFT_W + SETTINGS_GAP)
#define SETTINGS_CARD_RIGHT_X (SETTINGS_CARD_MID_X + SETTINGS_CARD_MID_W + SETTINGS_GAP)

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

static lv_obj_t *settings_nav_item(lv_obj_t *rail, int32_t y,
                                   const char *icon, bool active)
{
    lv_obj_t *slot = active
        ? np_fill(rail, 8, y, 56, 56, np_c_accent(), LV_OPA_COVER, NP_RADIUS_TILE)
        : np_fill(rail, 8, y, 56, 56, np_c_surface(), LV_OPA_TRANSP, NP_RADIUS_TILE);

    np_label(slot, icon, NP_FONT_ICON,
             active ? np_c_text_on_accent() : np_c_text_2(),
             0, 16, 56, LV_TEXT_ALIGN_CENTER);

    lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    return slot;
}

static lv_obj_t *settings_rail(lv_obj_t *root)
{
    lv_obj_t *rail = settings_panel(root,
                                    SETTINGS_RAIL_X, SETTINGS_RAIL_Y,
                                    SETTINGS_RAIL_W, SETTINGS_RAIL_H);

    lv_obj_t *home_button = settings_nav_item(rail, 16,  NP_ICON_HOME, false);
    (void)settings_nav_item(rail, 94,  NP_ICON_WEATHER, false);
    (void)settings_nav_item(rail, 172, NP_ICON_MARKET, false);
    (void)settings_nav_item(rail, 250, NP_ICON_CALENDAR, false);
    (void)settings_nav_item(rail, 410, NP_ICON_SETTINGS, true);

    return home_button;
}

static void settings_slider(lv_obj_t *parent,
                            int32_t y,
                            const char *icon,
                            const char *label,
                            uint8_t percent)
{
    const int32_t track_x = 24;
    const int32_t track_w = 232;
    const int32_t fill_w = (track_w * percent) / 100;

    np_label(parent, icon, NP_FONT_ICON, np_c_text_2(),
             24, y, 28, LV_TEXT_ALIGN_LEFT);
    np_label(parent, label, NP_FONT_MD, np_c_text(),
             58, y - 1, 150, LV_TEXT_ALIGN_LEFT);

    char pct[8] = {0};
    (void)snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    np_label(parent, pct, NP_FONT_SM, np_c_text_2(),
             208, y + 2, 48, LV_TEXT_ALIGN_RIGHT);

    np_fill(parent, track_x, y + 40, track_w, 8,
            np_c_hairline(), LV_OPA_COVER, 4);
    np_fill(parent, track_x, y + 40, fill_w, 8,
            np_c_accent(), LV_OPA_COVER, 4);

    const int32_t knob_x = track_x + fill_w - 9;
    np_dot(parent, knob_x, y + 35, 18, np_c_accent());
}

static lv_obj_t *settings_left_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_CONTENT_X, SETTINGS_CONTENT_Y,
                                    SETTINGS_CARD_LEFT_W, SETTINGS_CONTENT_H);

    np_label(card, "Conectividade", NP_FONT_LG, np_c_text(),
             24, 22, 220, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 24, 64, 232);

    np_label(card, NP_ICON_WIFI, NP_FONT_ICON, np_c_accent(),
             24, 96, 28, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Wi-Fi e rede", NP_FONT_MD, np_c_text(),
             60, 94, 190, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Gerenciar conexoes", NP_FONT_SM, np_c_text_2(),
             60, 124, 190, LV_TEXT_ALIGN_LEFT);

    np_hline(card, 24, 164, 232);

    np_label(card, NP_ICON_BLUETOOTH, NP_FONT_ICON, np_c_text_2(),
             24, 196, 28, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Bluetooth", NP_FONT_MD, np_c_text(),
             60, 194, 190, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Gerenciar dispositivos", NP_FONT_SM, np_c_text_2(),
             60, 224, 190, LV_TEXT_ALIGN_LEFT);

    np_hline(card, 24, 264, 232);

    np_label(card, NP_ICON_CALENDAR, NP_FONT_ICON, np_c_text_2(),
             24, 296, 28, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Hora e fuso", NP_FONT_MD, np_c_text(),
             60, 294, 190, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Data, hora e timezone", NP_FONT_SM, np_c_text_2(),
             60, 324, 190, LV_TEXT_ALIGN_LEFT);

    return card;
}

static lv_obj_t *settings_middle_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_CARD_MID_X, SETTINGS_CONTENT_Y,
                                    SETTINGS_CARD_MID_W, SETTINGS_CONTENT_H);

    np_label(card, "Tela e som", NP_FONT_LG, np_c_text(),
             24, 22, 180, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 24, 64, 232);

    /* Conforme solicitado: somente brilho da tela e volume geral. */
    settings_slider(card, 112, NP_ICON_UV, "Brilho da tela", SETTINGS_BRIGHTNESS);
    settings_slider(card, 238, NP_ICON_VOLUME_UP, "Volume geral", SETTINGS_VOLUME);

    return card;
}

static void settings_system_row(lv_obj_t *card,
                                int32_t y,
                                const char *label,
                                const char *value)
{
    np_label(card, label, NP_FONT_SM, np_c_text_2(),
             24, y, 104, LV_TEXT_ALIGN_LEFT);
    np_label(card, value, NP_FONT_SM, np_c_text(),
             128, y, 138, LV_TEXT_ALIGN_RIGHT);
}

static lv_obj_t *settings_right_card(lv_obj_t *root)
{
    lv_obj_t *card = settings_panel(root,
                                    SETTINGS_CARD_RIGHT_X, SETTINGS_CONTENT_Y,
                                    SETTINGS_CARD_RIGHT_W, SETTINGS_CONTENT_H);

    np_label(card, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
             24, 24, 28, LV_TEXT_ALIGN_LEFT);
    np_label(card, "Sistema", NP_FONT_LG, np_c_text(),
             58, 22, 180, LV_TEXT_ALIGN_LEFT);
    np_hline(card, 24, 64, 242);

    settings_system_row(card, 104, "Display",  "1024x600 RGB565");
    settings_system_row(card, 146, "Placa",    "ESP32-P4");
    settings_system_row(card, 188, "Firmware", "NovaPanel");

    np_hline(card, 24, 236, 242);

    np_button(card, 24, 274, 242, 52, "Diagnostico", false);
    np_button(card, 24, 342, 242, 52, "Reiniciar", false);

    return card;
}

np_settings_view_t np_settings_build(lv_obj_t *parent)
{
    np_settings_view_t view = {0};

    view.root = np_scene(parent);

    /* Fundamental: construir a arvore fora da composicao visivel. */
    np_set_visible(view.root, false);

    view.header = np_header(view.root);
    view.home_button = settings_rail(view.root);

    view.left_card = settings_left_card(view.root);
    view.middle_card = settings_middle_card(view.root);
    view.right_card = settings_right_card(view.root);

    /* O product_ui revela um card por vez. */
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
