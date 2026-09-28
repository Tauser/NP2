#pragma once

#include "np_modal.h"

/* Returns true when the command was accepted; false keeps the prompt open. */
typedef bool (*np_confirm_action_cb_t)(void *user_data);

typedef struct {
    np_modal_t modal;
    np_confirm_action_cb_t action;
    void *user_data;
    bool armed;
} np_confirm_t;

/* Create only on demand. Owns presentation; the callback owns the service bridge. */
void np_confirm_create(np_confirm_t *confirm, lv_obj_t *parent,
                        const char *title, const char *detail,
                        const char *action_label,
                        np_confirm_action_cb_t action, void *user_data);
