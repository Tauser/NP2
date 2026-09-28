#include "np_profile.h"

/* Identity has no source in the current application model. Do not borrow
 * the weather provider's location or manufacture a user from the mockup. */
static lv_obj_t *profile_row(lv_obj_t *parent, int32_t y,
                              const char *title, const char *detail)
{
    lv_obj_t *row = np_fill(parent, NP_SP_24, y, 428, 76,
                             np_c_surface_raised(), LV_OPA_COVER,
                             NP_RADIUS_CONTROL);
    np_label(row, title, NP_FONT_MD, np_c_text(), 16, 10, 352, LV_TEXT_ALIGN_LEFT);
    np_label(row, detail, NP_FONT_SM, np_c_text_2(), 16, 42, 352, LV_TEXT_ALIGN_LEFT);
    np_label(row, NP_ICON_ARROW_RIGHT, NP_FONT_ICON, np_c_text_2(),
              380, 26, 24, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    return row;
}

np_profile_view_t np_profile_build(lv_obj_t *parent)
{
    np_profile_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    view.home_button = np_button(view.root, 250, 12, 160, NP_TOUCH_TARGET,
                                  "Início", false);

    lv_obj_t *hero = np_panel(view.root, 24, 80, 976, 188);
    lv_obj_t *avatar = np_fill(hero, 24, 30, 128, 128, np_c_accent(),
                                LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(avatar, "--", NP_FONT_DISPLAY, np_c_text_on_accent(),
              0, 36, 128, LV_TEXT_ALIGN_CENTER);
    np_label(hero, "Olá!", NP_FONT_TITLE, np_c_text(),
              184, 28, 500, LV_TEXT_ALIGN_LEFT);
    np_label(hero, "Perfil principal", NP_FONT_LG, np_c_text_2(),
              184, 76, 500, LV_TEXT_ALIGN_LEFT);
    np_label(hero, "Nome ainda não informado", NP_FONT_SM, np_c_text_3(),
              184, 120, 500, LV_TEXT_ALIGN_LEFT);
    view.edit_button = np_button(hero, 784, 66, 168, 52, "Editar", false);

    lv_obj_t *account = np_panel(view.root, 24, 284, 476, 292);
    np_label(account, "Conta", NP_FONT_LG, np_c_text(),
              24, 20, 428, LV_TEXT_ALIGN_LEFT);
    np_label(account, "Identidade do usuário", NP_FONT_SM, np_c_text_2(),
              24, 54, 428, LV_TEXT_ALIGN_LEFT);
    view.name_row = profile_row(account, 100, "Nome", "Não informado");
    view.avatar_row = profile_row(account, 188, "Avatar", "Iniciais após definir o nome");

    lv_obj_t *personal = np_panel(view.root, 524, 284, 476, 292);
    np_label(personal, "Preferências pessoais", NP_FONT_LG, np_c_text(),
              24, 20, 428, LV_TEXT_ALIGN_LEFT);
    np_label(personal, "Seu painel", NP_FONT_SM, np_c_text_2(),
              24, 54, 428, LV_TEXT_ALIGN_LEFT);
    view.initial_screen_row = profile_row(personal, 100, "Tela inicial", "Home");
    view.preferences_row = profile_row(personal, 188, "Abrir preferências",
                                       "Configurações do sistema");
    return view;
}
