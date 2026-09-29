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

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    lv_obj_t *persistence_status;
    np_settings_notifications_t notifications;
} np_settings_notifications_view_t;

np_settings_notifications_view_t np_settings_notifications_scene_build(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
