#include "np_settings_notifications.h"

#define SETTINGS_NOTIFICATIONS_MODAL_X 172
#define SETTINGS_NOTIFICATIONS_MODAL_Y 84
#define SETTINGS_NOTIFICATIONS_MODAL_W 680
#define SETTINGS_NOTIFICATIONS_MODAL_H 432

static lv_obj_t *notification_switch(lv_obj_t *parent, int32_t x, int32_t y)
{
    lv_obj_t *const sw = lv_switch_create(parent);
    lv_obj_set_pos(sw, x, y);
    lv_obj_set_size(sw, 52, 28);
    lv_obj_set_style_bg_color(sw, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, np_c_text(), LV_PART_KNOB);
    return sw;
}

static lv_obj_t *notification_item(lv_obj_t *parent, int32_t y,
                                   const char *icon, lv_color_t icon_color,
                                   const char *title, const char *detail)
{
    const int32_t width = SETTINGS_NOTIFICATIONS_MODAL_W - 48;
    lv_obj_t *const row = np_fill(parent, 24, y, width, 68,
                                  np_c_surface_raised(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
    lv_obj_t *const icon_tile = np_fill(row, 14, 13, 42, 42,
                                        np_c_surface(), LV_OPA_COVER,
                                        NP_RADIUS_CONTROL);
    np_label(icon_tile, icon, NP_FONT_ICON, icon_color, 0, 9, 42,
             LV_TEXT_ALIGN_CENTER);
    np_label(row, title, NP_FONT_MD, np_c_text(), 72, 9, 350,
             LV_TEXT_ALIGN_LEFT);
    np_label(row, detail, NP_FONT_SM, np_c_text_2(), 72, 37, 410,
             LV_TEXT_ALIGN_LEFT);
    return row;
}

static void row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        np_settings_notifications_show(lv_event_get_user_data(event));
    }
}

static void set_switch(lv_obj_t *sw, bool enabled)
{
    if (sw == NULL) return;
    if (enabled) lv_obj_add_state(sw, LV_STATE_CHECKED);
    else lv_obj_remove_state(sw, LV_STATE_CHECKED);
}

void np_settings_notifications_create(np_settings_notifications_t *notifications,
                                      lv_obj_t *parent)
{
    if (notifications == NULL || parent == NULL) return;
    *notifications = (np_settings_notifications_t){0};
    np_modal_create(&notifications->modal, parent,
                    SETTINGS_NOTIFICATIONS_MODAL_X, SETTINGS_NOTIFICATIONS_MODAL_Y,
                    SETTINGS_NOTIFICATIONS_MODAL_W, SETTINGS_NOTIFICATIONS_MODAL_H,
                    NP_ICON_NOTIFICATIONS, np_c_accent(),
                    "Notificacoes", "Alertas e som do painel");
    lv_obj_t *const content = notifications->modal.content;
    if (content == NULL) return;

    lv_obj_t *const general = notification_item(content, 19,
        NP_ICON_NOTIFICATIONS, np_c_accent(),
        "Notificacoes gerais", "Exibe alertas nao criticos");
    notifications->general_switch = notification_switch(general, 552, 20);
    lv_obj_t *const sound = notification_item(content, 95,
        NP_ICON_VOLUME_UP, np_c_positive(),
        "Som das notificacoes", "Usa o volume geral configurado");
    notifications->sound_switch = notification_switch(sound, 552, 20);
    lv_obj_t *const system = notification_item(content, 171,
        NP_ICON_WARNING, np_c_warning(),
        "Alertas do sistema", "Rede, armazenamento e atualizacoes");
    notifications->system_switch = notification_switch(system, 552, 20);
    np_label(content, "Teste no volume atual", NP_FONT_SM, np_c_text_3(),
             24, 273, 280, LV_TEXT_ALIGN_LEFT);
    notifications->test_button = np_button(content, 442, 261, 214, 52,
                                            "Testar som", true);
}

void np_settings_notifications_show(np_settings_notifications_t *notifications)
{
    if (notifications != NULL) np_modal_show(&notifications->modal);
}

void np_settings_notifications_hide(np_settings_notifications_t *notifications)
{
    if (notifications != NULL) np_modal_hide(&notifications->modal);
}

void np_settings_notifications_bind_row(np_settings_notifications_t *notifications,
                                        lv_obj_t *row)
{
    if (notifications == NULL || row == NULL) return;
    lv_obj_add_event_cb(row, row_event_cb, LV_EVENT_CLICKED, notifications);
}

void np_settings_notifications_sync(np_settings_notifications_t *notifications,
                                    bool general_enabled, bool sound_enabled,
                                    bool system_alerts_enabled)
{
    if (notifications == NULL) return;
    set_switch(notifications->general_switch, general_enabled);
    set_switch(notifications->sound_switch, sound_enabled);
    set_switch(notifications->system_switch, system_alerts_enabled);
}
