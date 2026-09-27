#include "np_feedback.h"

#include "np_tokens.h"

static lv_color_t feedback_color(np_feedback_kind_t kind)
{
    switch (kind) {
    case NP_FEEDBACK_SUCCESS: return np_c_positive();
    case NP_FEEDBACK_WARNING: return np_c_warning();
    case NP_FEEDBACK_ERROR: return np_c_negative();
    case NP_FEEDBACK_INFO:
    default: return np_c_accent();
    }
}

static const char *feedback_icon(np_feedback_kind_t kind)
{
    switch (kind) {
    case NP_FEEDBACK_SUCCESS: return "✓";
    case NP_FEEDBACK_WARNING: return "!";
    case NP_FEEDBACK_ERROR: return "×";
    case NP_FEEDBACK_INFO:
    default: return "i";
    }
}

static void feedback_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *target = lv_timer_get_user_data(timer);
    if (target != NULL) np_set_visible(target, false);
    lv_timer_pause(timer);
}

np_feedback_t np_feedback_create(lv_obj_t *parent)
{
    np_feedback_t feedback = {0};
    feedback.root = np_group(parent, 0, 0, NP_SCREEN_W, NP_SCREEN_H);
    lv_obj_remove_flag(feedback.root, LV_OBJ_FLAG_CLICKABLE);

    feedback.toast = np_fill(feedback.root, 744, 512, 256, 64,
                             np_c_surface_raised(), LV_OPA_COVER,
                             NP_RADIUS_CONTROL);
    feedback.toast_icon = np_label(feedback.toast, "i", NP_FONT_MD,
                                   np_c_accent(), 16, 18, 24,
                                   LV_TEXT_ALIGN_CENTER);
    feedback.toast_title = np_label(feedback.toast, "", NP_FONT_SM,
                                    np_c_text(), 52, 12, 188,
                                    LV_TEXT_ALIGN_LEFT);
    feedback.toast_detail = np_label(feedback.toast, "", NP_FONT_SM,
                                     np_c_text_2(), 52, 34, 188,
                                     LV_TEXT_ALIGN_LEFT);

    feedback.osd = np_fill(feedback.root, 360, 432, 304, 100,
                            np_c_surface_raised(), LV_OPA_COVER,
                            NP_RADIUS_SURFACE);
    feedback.osd_icon = np_label(feedback.osd, "", NP_FONT_ICON,
                                 np_c_text_2(), 20, 18, 28,
                                 LV_TEXT_ALIGN_CENTER);
    feedback.osd_title = np_label(feedback.osd, "", NP_FONT_MD,
                                  np_c_text(), 60, 16, 160,
                                  LV_TEXT_ALIGN_LEFT);
    feedback.osd_value = np_label(feedback.osd, "", NP_FONT_LG,
                                  np_c_accent(), 220, 14, 64,
                                  LV_TEXT_ALIGN_RIGHT);

    feedback.banner = np_fill(feedback.root, 24, 76, 976, 48,
                               np_c_surface_raised(), LV_OPA_COVER,
                               NP_RADIUS_CONTROL);
    feedback.banner_icon = np_label(feedback.banner, "i", NP_FONT_MD,
                                    np_c_accent(), 16, 13, 24,
                                    LV_TEXT_ALIGN_CENTER);
    feedback.banner_text = np_label(feedback.banner, "", NP_FONT_SM,
                                    np_c_text(), 52, 15, 900,
                                    LV_TEXT_ALIGN_LEFT);
    feedback.toast_timer = lv_timer_create(feedback_timer_cb, 2000U, feedback.toast);
    feedback.osd_timer = lv_timer_create(feedback_timer_cb, 2000U, feedback.osd);
    if (feedback.toast_timer != NULL) lv_timer_pause(feedback.toast_timer);
    if (feedback.osd_timer != NULL) lv_timer_pause(feedback.osd_timer);
    np_set_visible(feedback.toast, false);
    np_set_visible(feedback.osd, false);
    np_set_visible(feedback.banner, false);
    return feedback;
}

static void feedback_schedule(lv_timer_t *timer, uint32_t duration_ms)
{
    if (timer == NULL) return;
    lv_timer_set_period(timer, duration_ms);
    lv_timer_reset(timer);
    lv_timer_resume(timer);
}

void np_feedback_show_toast(np_feedback_t *feedback, np_feedback_kind_t kind,
                            const char *title, const char *detail,
                            uint32_t duration_ms)
{
    if (feedback == NULL || feedback->toast == NULL) return;
    const uint32_t duration = duration_ms != 0U ? duration_ms :
        (kind == NP_FEEDBACK_ERROR ? 4000U :
         kind == NP_FEEDBACK_WARNING ? 3000U : 2000U);
    np_set_text(feedback->toast_icon, feedback_icon(kind));
    np_set_text_color(feedback->toast_icon, feedback_color(kind));
    np_set_text(feedback->toast_title, title != NULL ? title : "");
    np_set_text(feedback->toast_detail, detail != NULL ? detail : "");
    np_set_visible(feedback->toast, true);
    np_feedback_bring_to_front(feedback);
    feedback_schedule(feedback->toast_timer, duration);
}

void np_feedback_show_osd(np_feedback_t *feedback, const char *icon,
                          const char *title, const char *value,
                          uint32_t duration_ms)
{
    if (feedback == NULL || feedback->osd == NULL) return;
    np_set_text(feedback->osd_icon, icon != NULL ? icon : "");
    np_set_text(feedback->osd_title, title != NULL ? title : "");
    np_set_text(feedback->osd_value, value != NULL ? value : "");
    np_set_visible(feedback->osd, true);
    np_feedback_bring_to_front(feedback);
    feedback_schedule(feedback->osd_timer, duration_ms != 0U ? duration_ms : 2000U);
}

void np_feedback_show_banner(np_feedback_t *feedback, np_feedback_kind_t kind,
                             const char *text)
{
    if (feedback == NULL || feedback->banner == NULL) return;
    np_set_text(feedback->banner_icon, feedback_icon(kind));
    np_set_text_color(feedback->banner_icon, feedback_color(kind));
    np_set_text(feedback->banner_text, text != NULL ? text : "");
    np_set_visible(feedback->banner, true);
    np_feedback_bring_to_front(feedback);
}

void np_feedback_hide_banner(np_feedback_t *feedback)
{
    if (feedback != NULL) np_set_visible(feedback->banner, false);
}

void np_feedback_hide_toast(np_feedback_t *feedback)
{
    if (feedback == NULL) return;
    np_set_visible(feedback->toast, false);
    if (feedback->toast_timer != NULL) lv_timer_pause(feedback->toast_timer);
}

void np_feedback_hide_osd(np_feedback_t *feedback)
{
    if (feedback == NULL) return;
    np_set_visible(feedback->osd, false);
    if (feedback->osd_timer != NULL) lv_timer_pause(feedback->osd_timer);
}

void np_feedback_hide_all(np_feedback_t *feedback)
{
    np_feedback_hide_toast(feedback);
    np_feedback_hide_osd(feedback);
    np_feedback_hide_banner(feedback);
}

void np_feedback_bring_to_front(np_feedback_t *feedback)
{
    if (feedback != NULL && feedback->root != NULL) lv_obj_move_foreground(feedback->root);
}

void np_feedback_destroy(np_feedback_t *feedback)
{
    if (feedback == NULL) return;
    if (feedback->toast_timer != NULL) lv_timer_delete(feedback->toast_timer);
    if (feedback->osd_timer != NULL) lv_timer_delete(feedback->osd_timer);
    *feedback = (np_feedback_t){0};
}
