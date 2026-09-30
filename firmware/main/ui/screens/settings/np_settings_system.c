#include "np_settings_system.h"

#define SETTINGS_SYSTEM_MODAL_X 192
#define SETTINGS_SYSTEM_MODAL_Y 116
#define SETTINGS_SYSTEM_MODAL_W 640
#define SETTINGS_SYSTEM_MODAL_H 368

static lv_obj_t *system_value(lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
                         const char *label, const char *value, bool inline_value)
{
    np_label(parent, label, NP_FONT_SM, np_c_text_3(), x, y,
             inline_value ? 156 : width,
             LV_TEXT_ALIGN_LEFT);
    return np_label(parent, value, NP_FONT_MD, np_c_text(),
             inline_value ? x + 164 : x, inline_value ? y - 3 : y + 24,
             inline_value ? width - 164 : width,
             inline_value ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT);
}

static void row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        np_settings_system_show(lv_event_get_user_data(event));
    }
}

void np_settings_system_create(np_settings_system_t *system, lv_obj_t *parent)
{
    if (system == NULL || parent == NULL) return;
    *system = (np_settings_system_t){0};
    np_modal_create(&system->modal, parent,
                    SETTINGS_SYSTEM_MODAL_X, SETTINGS_SYSTEM_MODAL_Y,
                    SETTINGS_SYSTEM_MODAL_W, SETTINGS_SYSTEM_MODAL_H,
                    NP_ICON_SETTINGS, np_c_text_2(),
                    "Sistema", "Informacoes do dispositivo");
    lv_obj_t *const content = system->modal.content;
    if (content == NULL) return;

    system_value(content, 32, 33, 250, "Display", "1024x600 · RGB565", false);
    system_value(content, 336, 33, 250, "Touch", "Capacitivo", false);
    system->firmware = system_value(content, 32, 107, 250, "Firmware", "Carregando...", false);
    system->temperature = system_value(content, 336, 107, 250, "Temperatura do chip", "Nao disponivel", false);
    np_vline(content, 320, 27, 134);

    np_label(content, "Atualizacoes por manutencao", NP_FONT_SM, np_c_text_3(),
             32, 174, 576, LV_TEXT_ALIGN_LEFT);
    system->restart_button = np_button(content, 32, 220, 576, 48,
                                       "Reiniciar painel", false);
}

void np_settings_system_show(np_settings_system_t *system)
{
    if (system != NULL) np_modal_show(&system->modal);
}

void np_settings_system_hide(np_settings_system_t *system)
{
    if (system != NULL) np_modal_hide(&system->modal);
}

void np_settings_system_bind_row(np_settings_system_t *system, lv_obj_t *row)
{
    if (system == NULL || row == NULL) return;
    lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, system);
}

void np_settings_system_sync(np_settings_system_t *system, const char *firmware,
                              const char *temperature, bool restarting)
{
    if (system == NULL || system->firmware == NULL) return;
    np_set_text(system->firmware, firmware != NULL && firmware[0] != '\0' ? firmware : "Carregando...");
    np_set_text(system->temperature, temperature);
    if (restarting) lv_obj_add_state(system->restart_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(system->restart_button, LV_STATE_DISABLED);
}

void np_settings_system_scene_create(np_settings_system_view_t *view, lv_obj_t *parent)
{
    if (view == NULL || parent == NULL || view->root != NULL) return;
    view->root = np_scene(parent);
    np_set_visible(view->root, false);
    view->header = np_header(view->root);
    view->back_button = np_button(view->root, NP_HEADER_NAV_X, 12, NP_HEADER_NAV_W, NP_TOUCH_TARGET,
                                   "Voltar", false);

    lv_obj_t *info = np_panel(view->root, 24, 80, 552, 496);
    np_label(info, NP_ICON_SETTINGS, NP_FONT_ICON, np_c_text_2(),
              24, 26, 40, LV_TEXT_ALIGN_CENTER);
    np_label(info, "Sistema", NP_FONT_TITLE, np_c_text(),
              80, 16, 448, LV_TEXT_ALIGN_LEFT);
    np_label(info, "Informações do dispositivo", NP_FONT_SM, np_c_text_2(),
              80, 60, 448, LV_TEXT_ALIGN_LEFT);
    np_hline(info, 24, 104, 504);
    system_value(info, 24, 140, 504, "Display", "1024x600 · RGB565", true);
    np_hline(info, 24, 188, 504);
    system_value(info, 24, 224, 504, "Touch", "Capacitivo", true);
    np_hline(info, 24, 272, 504);
    view->system.temperature = system_value(info, 24, 308, 504,
                                              "Temperatura", "Nao disponivel", true);
    np_hline(info, 24, 356, 504);
    view->system.firmware = system_value(info, 24, 392, 504,
                                           "Firmware", "Carregando...", true);

    lv_obj_t *actions = np_panel(view->root, 592, 80, 408, 496);
    lv_obj_t *badge = np_fill(actions, 24, 16, 64, 64, np_c_accent(),
                               LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(badge, NP_ICON_SETTINGS, NP_FONT_ICON_BADGE, np_c_text_on_accent(),
              0, (64 - NP_FONT_ICON_BADGE->line_height) / 2, 64, LV_TEXT_ALIGN_CENTER);
    np_label(actions, "Ações", NP_FONT_TITLE, np_c_text(),
              112, 16, 272, LV_TEXT_ALIGN_LEFT);
    np_label(actions, "Manutenção do dispositivo", NP_FONT_SM, np_c_text_2(),
              112, 60, 272, LV_TEXT_ALIGN_LEFT);
    view->update_button = np_button(actions, 24, 156, 360, 96,
                                      "Atualizar sistema", true);
    lv_obj_add_state(view->update_button, LV_STATE_DISABLED);
    np_label(actions, "Atualizacoes por manutencao", NP_FONT_SM, np_c_text_3(),
              24, 268, 360, LV_TEXT_ALIGN_CENTER);
    view->system.restart_button = np_button(actions, 24, 324, 360, 96,
                                              "Reiniciar dispositivo", false);
}
