#pragma once

#include "app_state.h"
#include "np_components.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *summary;
    lv_obj_t *empty;
    lv_obj_t *close_button;
    lv_obj_t *mark_all_button;
    lv_obj_t *list;
    lv_obj_t *rows[APP_NOTIFICATION_HISTORY_MAX];
    lv_obj_t *row_titles[APP_NOTIFICATION_HISTORY_MAX];
    lv_obj_t *row_details[APP_NOTIFICATION_HISTORY_MAX];
    lv_obj_t *row_times[APP_NOTIFICATION_HISTORY_MAX];
    lv_obj_t *row_unread[APP_NOTIFICATION_HISTORY_MAX];
    uint32_t row_ids[APP_NOTIFICATION_HISTORY_MAX];
} np_notification_center_t;

np_notification_center_t np_notification_center_create(lv_obj_t *parent);
void np_notification_center_sync(np_notification_center_t *view,
                                 const app_notification_center_projection_t *state,
                                 uint32_t current_unix_s);

#ifdef __cplusplus
}
#endif
