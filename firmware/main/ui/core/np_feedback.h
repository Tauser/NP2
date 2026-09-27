#pragma once

#include <stdint.h>

#include "np_components.h"

typedef enum {
    NP_FEEDBACK_SUCCESS = 0,
    NP_FEEDBACK_INFO,
    NP_FEEDBACK_WARNING,
    NP_FEEDBACK_ERROR,
} np_feedback_kind_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *toast;
    lv_obj_t *toast_icon;
    lv_obj_t *toast_title;
    lv_obj_t *toast_detail;
    lv_obj_t *osd;
    lv_obj_t *osd_icon;
    lv_obj_t *osd_title;
    lv_obj_t *osd_value;
    lv_obj_t *banner;
    lv_obj_t *banner_icon;
    lv_obj_t *banner_text;
    lv_timer_t *toast_timer;
    lv_timer_t *osd_timer;
} np_feedback_t;

np_feedback_t np_feedback_create(lv_obj_t *parent);
void np_feedback_show_toast(np_feedback_t *feedback, np_feedback_kind_t kind,
                            const char *title, const char *detail,
                            uint32_t duration_ms);
void np_feedback_show_osd(np_feedback_t *feedback, const char *icon,
                          const char *title, const char *value,
                          uint32_t duration_ms);
void np_feedback_show_banner(np_feedback_t *feedback, np_feedback_kind_t kind,
                             const char *text);
void np_feedback_hide_banner(np_feedback_t *feedback);
void np_feedback_hide_toast(np_feedback_t *feedback);
void np_feedback_hide_osd(np_feedback_t *feedback);
void np_feedback_hide_all(np_feedback_t *feedback);
void np_feedback_bring_to_front(np_feedback_t *feedback);
void np_feedback_destroy(np_feedback_t *feedback);
