#include "np_modal.h"

static void close_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_modal_hide(lv_event_get_user_data(event));
}

void np_modal_create(np_modal_t *modal, lv_obj_t *parent,
                     int32_t x, int32_t y, int32_t width, int32_t height,
                     const char *icon, lv_color_t icon_color,
                     const char *title, const char *subtitle)
{
    if (modal == NULL || parent == NULL || width <= 0 || height <= 0) return;

    *modal = (np_modal_t){0};
    modal->scrim = np_fill(parent, 0, 0, NP_SCREEN_W, NP_SCREEN_H,
                            np_c_bg(), LV_OPA_70, 0);
    lv_obj_add_flag(modal->scrim, LV_OBJ_FLAG_CLICKABLE);
    modal->panel = np_panel(modal->scrim, x, y, width, height);

    const bool has_icon = icon != NULL && icon[0] != '\0';
    const int32_t title_x = has_icon ? 76 : 24;
    if (has_icon) {
        lv_obj_t *tile = np_fill(modal->panel, 24, 16, 40, 40,
                                  np_c_accent_bg(), LV_OPA_COVER,
                                  NP_RADIUS_CONTROL);
        modal->icon = np_label(tile, icon, NP_FONT_ICON, icon_color,
                               0, 8, 40, LV_TEXT_ALIGN_CENTER);
    }
    modal->title = np_label(modal->panel, title != NULL ? title : "",
                            NP_FONT_LG, np_c_text(), title_x, 17,
                            width - title_x - 84, LV_TEXT_ALIGN_LEFT);
    modal->subtitle = np_label(modal->panel, subtitle != NULL ? subtitle : "",
                               NP_FONT_SM, np_c_text_2(), title_x, 47,
                               width - title_x - 84, LV_TEXT_ALIGN_LEFT);
    np_set_visible(modal->subtitle, subtitle != NULL && subtitle[0] != '\0');
    modal->close_button = np_icon_button(modal->panel, width - 64, 12,
                                         44, NP_ICON_CLOSE);
    lv_obj_add_event_cb(modal->close_button, close_button_event_cb,
                        LV_EVENT_CLICKED, modal);
    np_hline(modal->panel, 24, 76, width - 48);
    modal->content = np_group(modal->panel, 0, 77, width, height - 77);
    np_set_visible(modal->scrim, false);
}

void np_modal_set_close_callback(np_modal_t *modal,
                                 np_modal_close_cb_t callback,
                                 void *user_data)
{
    if (modal == NULL) return;
    modal->before_hide = callback;
    modal->before_hide_user_data = user_data;
}

void np_modal_show(np_modal_t *modal)
{
    if (modal == NULL || modal->scrim == NULL) return;
    lv_obj_move_foreground(modal->scrim);
    np_set_visible(modal->scrim, true);
}

void np_modal_hide(np_modal_t *modal)
{
    if (modal == NULL || modal->scrim == NULL ||
        !np_modal_is_visible(modal)) return;
    if (modal->before_hide != NULL) {
        modal->before_hide(modal->before_hide_user_data);
    }
    np_set_visible(modal->scrim, false);
}

bool np_modal_is_visible(const np_modal_t *modal)
{
    return modal != NULL && modal->scrim != NULL &&
           !lv_obj_has_flag(modal->scrim, LV_OBJ_FLAG_HIDDEN);
}
