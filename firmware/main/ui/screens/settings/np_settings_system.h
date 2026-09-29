#pragma once

#include "np_modal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    np_modal_t modal;
    lv_obj_t *firmware;
    lv_obj_t *temperature;
    lv_obj_t *restart_button;
} np_settings_system_t;

void np_settings_system_create(np_settings_system_t *system, lv_obj_t *parent);
void np_settings_system_show(np_settings_system_t *system);
void np_settings_system_hide(np_settings_system_t *system);
void np_settings_system_bind_row(np_settings_system_t *system, lv_obj_t *row);
void np_settings_system_sync(np_settings_system_t *system, const char *firmware,
                              const char *temperature, bool restarting);

/* The legacy modal and the scene share their information/action handles. */
typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    lv_obj_t *update_button;
    np_settings_system_t system;
} np_settings_system_view_t;

/* Lazy, idempotent construction. Re-enter by showing this same root. */
void np_settings_system_scene_create(np_settings_system_view_t *view, lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
