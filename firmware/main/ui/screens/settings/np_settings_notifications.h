#pragma once

#include "np_components.h"
#include "sound_melody.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *general_switch;
    lv_obj_t *sound_switch;
    lv_obj_t *system_switch;
    lv_obj_t *melodies[SOUND_CATEGORY_COUNT], *previews[SOUND_CATEGORY_COUNT];
} np_settings_notifications_t;

void np_settings_notifications_sync(np_settings_notifications_t *notifications,
                                    bool general_enabled, bool sound_enabled,
                                    bool system_alerts_enabled,
                                    const uint8_t melodies[SOUND_CATEGORY_COUNT]);

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
