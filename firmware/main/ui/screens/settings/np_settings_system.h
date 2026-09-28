#pragma once

#include "np_modal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    np_modal_t modal;
} np_settings_system_t;

void np_settings_system_create(np_settings_system_t *system, lv_obj_t *parent);
void np_settings_system_show(np_settings_system_t *system);
void np_settings_system_hide(np_settings_system_t *system);
void np_settings_system_bind_row(np_settings_system_t *system, lv_obj_t *row);

#ifdef __cplusplus
}
#endif
