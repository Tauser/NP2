#include "np_settings_system.h"

#define SETTINGS_SYSTEM_MODAL_X 192
#define SETTINGS_SYSTEM_MODAL_Y 116
#define SETTINGS_SYSTEM_MODAL_W 640
#define SETTINGS_SYSTEM_MODAL_H 368

static lv_obj_t *system_value(lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
                         const char *label, const char *value)
{
    np_label(parent, label, NP_FONT_SM, np_c_text_3(), x, y, width,
             LV_TEXT_ALIGN_LEFT);
    return np_label(parent, value, NP_FONT_MD, np_c_text(), x, y + 24, width,
             LV_TEXT_ALIGN_LEFT);
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

    system_value(content, 32, 33, 250, "Display", "1024x600 · RGB565");
    system_value(content, 336, 33, 250, "Touch", "Capacitivo");
    system->firmware = system_value(content, 32, 107, 250, "Firmware", "Carregando...");
    system->temperature = system_value(content, 336, 107, 250, "Temperatura do chip", "Nao disponivel");
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
    if (system == NULL || system->modal.scrim == NULL) return;
    np_set_text(system->firmware, firmware != NULL && firmware[0] != '\0' ? firmware : "Carregando...");
    np_set_text(system->temperature, temperature);
    if (restarting) lv_obj_add_state(system->restart_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(system->restart_button, LV_STATE_DISABLED);
}
