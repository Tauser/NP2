#pragma once

#include "np_modal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    np_modal_t modal;
    lv_obj_t *general_switch;
    lv_obj_t *sound_switch;
    lv_obj_t *system_switch;
    lv_obj_t *test_button;
} np_settings_notifications_t;

void np_settings_notifications_create(np_settings_notifications_t *notifications,
                                      lv_obj_t *parent);
void np_settings_notifications_show(np_settings_notifications_t *notifications);
void np_settings_notifications_hide(np_settings_notifications_t *notifications);
void np_settings_notifications_bind_row(np_settings_notifications_t *notifications,
                                        lv_obj_t *row);
void np_settings_notifications_sync(np_settings_notifications_t *notifications,
                                    bool general_enabled, bool sound_enabled,
                                    bool system_alerts_enabled);

#ifdef __cplusplus
}
#endif
