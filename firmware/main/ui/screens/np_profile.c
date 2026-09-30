#include "np_profile.h"

#include <string.h>

static lv_color_t avatar_color(uint8_t index)
{
    switch (index) {
    case 1: return np_c_positive();
    case 2: return np_c_negative();
    case 3: return np_c_text_2();
    default: return np_c_accent();
    }
}

static void initials_for(const char *name, char out[4])
{
    out[0] = '-'; out[1] = '-'; out[2] = '\0';
    if (name == NULL || name[0] == '\0') return;
    uint8_t found = 0U;
    bool start = true;
    for (const unsigned char *p = (const unsigned char *)name; *p && found < 2U; ++p) {
        if (*p == ' ') { start = true; continue; }
        if (!start) continue;
        unsigned char ch = *p;
        if (ch == 0xc3U && p[1] != 0U) {
            const unsigned char accent = *++p;
            ch = (accent >= 0x80U && accent <= 0x85U) ||
                 (accent >= 0xa0U && accent <= 0xa5U) ? 'A' :
                 accent == 0x87U || accent == 0xa7U ? 'C' :
                 (accent >= 0x88U && accent <= 0x8bU) ||
                 (accent >= 0xa8U && accent <= 0xabU) ? 'E' :
                 (accent >= 0x8cU && accent <= 0x8fU) ||
                 (accent >= 0xacU && accent <= 0xafU) ? 'I' :
                 (accent >= 0x92U && accent <= 0x96U) ||
                 (accent >= 0xb2U && accent <= 0xb6U) ? 'O' :
                 (accent >= 0x99U && accent <= 0x9cU) ||
                 (accent >= 0xb9U && accent <= 0xbcU) ? 'U' : '?';
        }
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');
        out[found++] = ch < 0x80U ? (char)ch : '?';
        start = false;
    }
    if (found == 1U) out[1] = '\0';
}

/* Identity has no source in the current application model. Do not borrow
 * the weather provider's location or manufacture a user from the mockup. */
static lv_obj_t *profile_row(lv_obj_t *parent, int32_t y,
                              const char *icon, const char *title, const char *detail,
                              lv_obj_t **out_detail)
{
    lv_obj_t *row = np_fill(parent, NP_SP_24, y, 428, 76,
                             np_c_surface_raised(), LV_OPA_COVER,
                             NP_RADIUS_CONTROL);
    np_label(row, icon, NP_FONT_ICON, np_c_text_2(),
              16, 25, 28, LV_TEXT_ALIGN_CENTER);
    np_label(row, title, NP_FONT_MD, np_c_text(), 60, 10, 300, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *value = np_label(row, detail, NP_FONT_SM, np_c_text_2(),
                               60, 42, 300, LV_TEXT_ALIGN_LEFT);
    if (out_detail != NULL) *out_detail = value;
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
    view.avatar = np_fill(hero, 24, 30, 128, 128, np_c_accent(),
                                LV_OPA_COVER, LV_RADIUS_CIRCLE);
    view.avatar_initials = np_label(view.avatar, "--", NP_FONT_DISPLAY, np_c_text_on_accent(),
              0, 36, 128, LV_TEXT_ALIGN_CENTER);
    view.greeting = np_label(hero, "Olá!", NP_FONT_TITLE, np_c_text(),
              184, 28, 500, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(view.greeting, LV_LABEL_LONG_DOT);
    np_label(hero, "Perfil principal", NP_FONT_LG, np_c_text_2(),
              184, 76, 500, LV_TEXT_ALIGN_LEFT);
    view.identity_hint = np_label(hero, "Nome ainda não informado", NP_FONT_SM, np_c_text_3(),
              184, 120, 500, LV_TEXT_ALIGN_LEFT);
    view.edit_button = np_button(hero, 784, 66, 168, 52, "Editar", false);

    lv_obj_t *account = np_panel(view.root, 24, 284, 476, 292);
    np_label(account, "Conta", NP_FONT_LG, np_c_text(),
              24, 20, 428, LV_TEXT_ALIGN_LEFT);
    np_label(account, "Identidade do usuário", NP_FONT_SM, np_c_text_2(),
              24, 54, 428, LV_TEXT_ALIGN_LEFT);
    view.name_row = profile_row(account, 100, NP_ICON_ACCOUNT,
                                "Nome", "Não informado", &view.name_value);
    view.avatar_row = profile_row(account, 188, NP_ICON_IMAGE,
                                  "Avatar", "Iniciais do nome", &view.avatar_value);

    lv_obj_t *personal = np_panel(view.root, 524, 284, 476, 292);
    np_label(personal, "Preferências pessoais", NP_FONT_LG, np_c_text(),
              24, 20, 428, LV_TEXT_ALIGN_LEFT);
    np_label(personal, "Seu painel", NP_FONT_SM, np_c_text_2(),
              24, 54, 428, LV_TEXT_ALIGN_LEFT);
    view.initial_screen_row = profile_row(personal, 100, NP_ICON_HOME,
                                           "Tela inicial", "Home", NULL);
    view.preferences_row = profile_row(personal, 188, NP_ICON_SETTINGS, "Abrir preferências",
                                       "Configurações do sistema", NULL);
    return view;
}

void np_profile_sync(np_profile_view_t *view, bool configured,
                     const user_profile_t *profile, bool pending, esp_err_t result)
{
    if (view == NULL || view->root == NULL) return;
    const char *name = configured && profile != NULL ? profile->name : "";
    char initials[4], greeting[80], avatar_detail[32];
    initials_for(name, initials);
    if (name[0] != '\0') {
        char first[USER_PROFILE_NAME_BYTES];
        size_t length = 0U;
        while (name[length] != '\0' && name[length] != ' ' && length + 1U < sizeof(first)) {
            first[length] = name[length]; ++length;
        }
        first[length] = '\0';
        lv_snprintf(greeting, sizeof(greeting), "Olá, %s!", first);
    } else lv_snprintf(greeting, sizeof(greeting), "Olá!");
    np_set_text(view->greeting, greeting);
    np_set_text(view->avatar_initials, initials);
    np_set_text(view->name_value, name[0] != '\0' ? name : "Não informado");
    lv_snprintf(avatar_detail, sizeof(avatar_detail), "Iniciais %s", initials);
    np_set_text(view->avatar_value, avatar_detail);
    np_set_bg_color(view->avatar, avatar_color(configured ? profile->avatar_color : 0U));
    view->current_color = configured ? profile->avatar_color : 0U;
    np_set_text(view->identity_hint, pending ? "Salvando perfil..." :
                result != ESP_OK ? "Falha ao salvar · tente novamente" :
                configured ? "Perfil salvo neste dispositivo" : "Nome ainda não informado");
}

static void editor_closed(void *user_data)
{
    np_profile_view_t *view = user_data;
    if (view != NULL && view->keyboard != NULL) np_keyboard_hide(view->keyboard);
}

static void preview_editor(np_profile_view_t *view)
{
    char initials[4];
    initials_for(lv_textarea_get_text(view->name_input), initials);
    np_set_text(view->editor_initials, initials);
    np_set_bg_color(view->editor_avatar, avatar_color(view->draft_color));
    for (uint8_t i = 0; i < USER_PROFILE_COLOR_COUNT; ++i) {
        lv_obj_set_style_border_color(view->color_buttons[i],
            i == view->draft_color ? np_c_text() : np_c_hairline(), 0);
        lv_label_set_text(lv_obj_get_child(view->color_buttons[i], 0),
                          i == view->draft_color ? NP_ICON_CHECK : "");
    }
}

static void editor_input_changed(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED)
        preview_editor(lv_event_get_user_data(event));
}

static void editor_color_clicked(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_profile_view_t *view = lv_event_get_user_data(event);
    for (uint8_t i = 0; i < USER_PROFILE_COLOR_COUNT; ++i) {
        if (lv_event_get_target(event) == view->color_buttons[i]) {
            view->draft_color = i;
            preview_editor(view);
            return;
        }
    }
}

void np_profile_open_editor(np_profile_view_t *view, np_keyboard_t *keyboard,
                            bool focus_name)
{
    if (view == NULL || view->root == NULL || keyboard == NULL) return;
    if (view->editor.scrim == NULL) {
        view->keyboard = keyboard;
        np_modal_create(&view->editor, view->root, 252, 16, 520, 348,
                        NULL, np_c_accent(), "Editar perfil", "Nome e avatar local");
        np_modal_set_close_callback(&view->editor, editor_closed, view);
        np_label(view->editor.content, "Nome", NP_FONT_SM, np_c_text_2(),
                 32, 12, 456, LV_TEXT_ALIGN_LEFT);
        view->name_input = np_form_text_input(view->editor.content, 32, 37,
                                              456, 54, "Seu nome", NP_ICON_ACCOUNT);
        lv_textarea_set_max_length(view->name_input, USER_PROFILE_NAME_BYTES - 1U);
        np_keyboard_bind(keyboard, view->name_input, NP_KEYBOARD_MODE_TEXT);
        lv_obj_add_event_cb(view->name_input, editor_input_changed,
                            LV_EVENT_VALUE_CHANGED, view);
        np_label(view->editor.content, "Cor do avatar", NP_FONT_SM, np_c_text_2(),
                 32, 110, 456, LV_TEXT_ALIGN_LEFT);
        view->editor_avatar = np_fill(view->editor.content, 32, 139, 52, 52,
                                      np_c_accent(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        view->editor_initials = np_label(view->editor_avatar, "--", NP_FONT_SM,
                                          np_c_text_on_accent(), 0, 16, 52,
                                          LV_TEXT_ALIGN_CENTER);
        for (uint8_t i = 0; i < USER_PROFILE_COLOR_COUNT; ++i) {
            view->color_buttons[i] = np_form_icon_button(view->editor.content,
                140 + i * 90, 139, 52, NP_ICON_CHECK);
            lv_obj_set_style_bg_color(view->color_buttons[i], avatar_color(i), 0);
            lv_obj_set_style_bg_opa(view->color_buttons[i], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(view->color_buttons[i], 3, 0);
            lv_obj_center(lv_obj_get_child(view->color_buttons[i], 0));
            lv_obj_set_style_text_color(view->color_buttons[i],
                                         np_c_text_on_accent(), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(view->color_buttons[i], 0),
                                         np_c_text_on_accent(), 0);
            lv_obj_add_event_cb(view->color_buttons[i], editor_color_clicked,
                                LV_EVENT_CLICKED, view);
        }
        view->cancel_button = np_form_button(view->editor.content, 32, 215,
                                              218, 48, "Cancelar",
                                              NP_FORM_BUTTON_SECONDARY);
        view->save_button = np_form_button(view->editor.content, 270, 215,
                                            218, 48, "Salvar",
                                            NP_FORM_BUTTON_PRIMARY);
    }
    view->draft_color = view->current_color;
    lv_textarea_set_text(view->name_input,
        strcmp(lv_label_get_text(view->name_value), "Não informado") == 0
            ? "" : lv_label_get_text(view->name_value));
    preview_editor(view);
    np_modal_show(&view->editor);
    if (focus_name) np_keyboard_focus(keyboard, view->name_input, NP_KEYBOARD_MODE_TEXT);
}

void np_profile_close_editor(np_profile_view_t *view)
{
    if (view == NULL) return;
    if (view->keyboard != NULL) np_keyboard_hide(view->keyboard);
    np_modal_hide(&view->editor);
}

bool np_profile_editor_value(const np_profile_view_t *view, user_profile_t *out_profile)
{
    if (view == NULL || view->name_input == NULL || out_profile == NULL) return false;
    const char *text = lv_textarea_get_text(view->name_input);
    size_t length = strlen(text);
    while (length > 0U && text[length - 1U] == ' ') --length;
    size_t start = 0U;
    while (start < length && text[start] == ' ') ++start;
    if (length - start >= USER_PROFILE_NAME_BYTES) return false;
    *out_profile = (user_profile_t){0};
    memcpy(out_profile->name, text + start, length - start);
    out_profile->avatar_color = view->draft_color;
    return user_profile_is_valid(out_profile);
}
