#include "np_confirm.h"

static void disarm(void *user_data)
{
    np_confirm_t *const confirm = user_data;
    confirm->armed = false;
}

static void cancel_event(lv_event_t *event)
{
    np_confirm_t *const confirm = lv_event_get_user_data(event);
    np_modal_hide(&confirm->modal);
}

static void confirm_event(lv_event_t *event)
{
    np_confirm_t *const confirm = lv_event_get_user_data(event);
    if (!confirm->armed || !np_modal_is_visible(&confirm->modal)) return;
    /* At most one command per accepted confirmation, including double taps. */
    confirm->armed = false;
    if (confirm->action != NULL && confirm->action(confirm->user_data)) {
        np_modal_hide(&confirm->modal);
    } else {
        confirm->armed = true;
    }
}

void np_confirm_create(np_confirm_t *confirm, lv_obj_t *parent,
                        const char *title, const char *detail,
                        const char *action_label,
                        np_confirm_action_cb_t action, void *user_data)
{
    if (confirm == NULL || parent == NULL) return;
    *confirm = (np_confirm_t){.action = action, .user_data = user_data, .armed = true};
    np_modal_create(&confirm->modal, parent, 252, 150, 520, 300,
                     NULL, np_c_text(), title, "Confirme para continuar");
    np_modal_set_close_callback(&confirm->modal, disarm, confirm);
    np_label(confirm->modal.content, detail, NP_FONT_MD, np_c_text_2(),
              24, 24, 472, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *const cancel = np_button(confirm->modal.content, 24, 150, 180, 48,
                                       "Cancelar", false);
    lv_obj_t *const accept = np_button(confirm->modal.content, 280, 150, 216, 48,
                                       action_label, true);
    lv_obj_add_event_cb(cancel, cancel_event, LV_EVENT_CLICKED, confirm);
    lv_obj_add_event_cb(accept, confirm_event, LV_EVENT_CLICKED, confirm);
    np_modal_show(&confirm->modal);
}
