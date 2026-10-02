/* Reusable flat modal for Product UI. It owns visual lifecycle only. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "np_components.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*np_modal_close_cb_t)(void *user_data);

typedef struct {
    lv_obj_t *scrim;
    lv_obj_t *panel;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *close_button;
    lv_obj_t *content;
    void *close_button_user_data;
    np_modal_close_cb_t before_hide;
    void *before_hide_user_data;
} np_modal_t;

/* The caller owns the storage for the modal for its whole LVGL lifetime. */
void np_modal_create(np_modal_t *modal, lv_obj_t *parent,
                     int32_t x, int32_t y, int32_t width, int32_t height,
                     const char *icon, lv_color_t icon_color,
                     const char *title, const char *subtitle);

void np_modal_set_close_callback(np_modal_t *modal,
                                 np_modal_close_cb_t callback,
                                 void *user_data);
/* Rebind after a containing view with an embedded modal is returned by value. */
void np_modal_rebind(np_modal_t *modal);
void np_modal_show(np_modal_t *modal);
void np_modal_hide(np_modal_t *modal);
bool np_modal_is_visible(const np_modal_t *modal);

#ifdef __cplusplus
}
#endif
