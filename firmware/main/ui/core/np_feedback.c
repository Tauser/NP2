#include "np_feedback.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "np_tokens.h"

/* -------------------------------------------------------------------------- */
/* Geometria                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Toast:
 * - alinhado ao canto superior direito
 * - abaixo do header
 * - largura suficiente para titulo + detalhe sem parecer "debug overlay"
 */
#define FEEDBACK_TOAST_X          600
#define FEEDBACK_TOAST_Y           76
#define FEEDBACK_TOAST_W          400
#define FEEDBACK_TOAST_H           84

#define FEEDBACK_TOAST_ACCENT_W      4
#define FEEDBACK_TOAST_ICON_X       20
#define FEEDBACK_TOAST_ICON_Y       22
#define FEEDBACK_TOAST_ICON_SIZE    40
#define FEEDBACK_TOAST_TEXT_X       76
#define FEEDBACK_TOAST_TEXT_W      300

/*
 * OSD:
 * - realmente centralizado
 * - possui barra visual nao interativa
 */
#define FEEDBACK_OSD_X            332
#define FEEDBACK_OSD_Y            220
#define FEEDBACK_OSD_W            360
#define FEEDBACK_OSD_H            148
#define FEEDBACK_OSD_PAD           24
#define FEEDBACK_OSD_TRACK_X       24
#define FEEDBACK_OSD_TRACK_Y       98
#define FEEDBACK_OSD_TRACK_W      312
#define FEEDBACK_OSD_TRACK_H       10

/*
 * Banner:
 * - usa a faixa livre do header entre a marca e os controles
 * - evita cobrir os cards da Home
 */
#define FEEDBACK_BANNER_X         250
#define FEEDBACK_BANNER_Y          10
#define FEEDBACK_BANNER_W         370
#define FEEDBACK_BANNER_H          44
#define FEEDBACK_BANNER_ACCENT_W    3

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
    case NP_FEEDBACK_SUCCESS: return NP_ICON_CHECK;
    case NP_FEEDBACK_WARNING: return NP_ICON_WARNING;
    case NP_FEEDBACK_ERROR: return NP_ICON_CLOSE;
    case NP_FEEDBACK_INFO:
    default: return NP_ICON_INFO;
    }
}

static void feedback_make_passive(lv_obj_t *obj)
{
    if (obj == NULL) return;
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void feedback_apply_surface(lv_obj_t *obj)
{
    if (obj == NULL) return;

    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, np_c_hairline(), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static void feedback_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *target = lv_timer_get_user_data(timer);
    if (target != NULL) {
        np_set_visible(target, false);
    }
    lv_timer_pause(timer);
}

static void feedback_schedule(lv_timer_t *timer, uint32_t duration_ms)
{
    if (timer == NULL) return;

    lv_timer_set_period(timer, duration_ms);
    lv_timer_reset(timer);
    lv_timer_resume(timer);
}

/*
 * Extrai o primeiro numero decimal de "65%", "Volume 65%" etc.
 * Retorna 0 para textos sem numero, como "Mudo".
 */
static uint8_t feedback_percent_from_value(const char *value)
{
    if (value == NULL) return 0U;

    uint32_t percent = 0U;
    bool found = false;

    for (const char *it = value; *it != '\0'; ++it) {
        if (*it >= '0' && *it <= '9') {
            found = true;
            percent = (uint32_t)(*it - '0');

            while (it[1] >= '0' && it[1] <= '9') {
                ++it;
                percent = percent * 10U + (uint32_t)(*it - '0');
                if (percent >= 100U) {
                    return 100U;
                }
            }
            break;
        }
    }

    return found ? (uint8_t)percent : 0U;
}

static bool feedback_is_volume_title(const char *title)
{
    return title != NULL &&
           (strstr(title, "Volume") != NULL || strstr(title, "volume") != NULL);
}

static void feedback_update_osd_bar(np_feedback_t *feedback, uint8_t percent)
{
    if (feedback == NULL || feedback->osd_indicator == NULL) return;

    if (percent > 100U) percent = 100U;

    if (percent == 0U) {
        np_set_visible(feedback->osd_indicator, false);
        return;
    }

    int32_t width = (FEEDBACK_OSD_TRACK_W * (int32_t)percent) / 100;
    if (width < 4) width = 4;

    lv_obj_set_width(feedback->osd_indicator, width);
    np_set_visible(feedback->osd_indicator, true);
}

np_feedback_t np_feedback_create(lv_obj_t *parent)
{
    np_feedback_t feedback = {0};

    feedback.root = np_group(parent, 0, 0, NP_SCREEN_W, NP_SCREEN_H);
    feedback_make_passive(feedback.root);

    /* ---------------------------------------------------------------------- */
    /* Toast                                                                  */
    /* ---------------------------------------------------------------------- */

    feedback.toast = np_fill(feedback.root,
                             FEEDBACK_TOAST_X, FEEDBACK_TOAST_Y,
                             FEEDBACK_TOAST_W, FEEDBACK_TOAST_H,
                             np_c_surface_raised(), LV_OPA_COVER,
                             NP_RADIUS_TILE);
    feedback_make_passive(feedback.toast);
    feedback_apply_surface(feedback.toast);

    feedback.toast_accent = np_fill(feedback.toast,
                                    0, 0,
                                    FEEDBACK_TOAST_ACCENT_W, FEEDBACK_TOAST_H,
                                    np_c_accent(), LV_OPA_COVER,
                                    NP_RADIUS_TILE);
    feedback_make_passive(feedback.toast_accent);

    feedback.toast_icon_bg = np_fill(feedback.toast,
                                     FEEDBACK_TOAST_ICON_X,
                                     FEEDBACK_TOAST_ICON_Y,
                                     FEEDBACK_TOAST_ICON_SIZE,
                                     FEEDBACK_TOAST_ICON_SIZE,
                                     np_c_accent(), LV_OPA_20,
                                     LV_RADIUS_CIRCLE);
    feedback_make_passive(feedback.toast_icon_bg);

    feedback.toast_icon = np_label(feedback.toast_icon_bg,
                                   NP_ICON_INFO,
                                   NP_FONT_ICON,
                                   np_c_accent(),
                                   8, 8, 24,
                                   LV_TEXT_ALIGN_CENTER);

    feedback.toast_title = np_label(feedback.toast,
                                    "",
                                    NP_FONT_MD,
                                    np_c_text(),
                                    FEEDBACK_TOAST_TEXT_X,
                                    13,
                                    FEEDBACK_TOAST_TEXT_W,
                                    LV_TEXT_ALIGN_LEFT);

    feedback.toast_detail = np_label(feedback.toast,
                                     "",
                                     NP_FONT_SM,
                                     np_c_text_2(),
                                     FEEDBACK_TOAST_TEXT_X,
                                     47,
                                     FEEDBACK_TOAST_TEXT_W,
                                     LV_TEXT_ALIGN_LEFT);

    /* ---------------------------------------------------------------------- */
    /* OSD                                                                    */
    /* ---------------------------------------------------------------------- */

    feedback.osd = np_fill(feedback.root,
                           FEEDBACK_OSD_X, FEEDBACK_OSD_Y,
                           FEEDBACK_OSD_W, FEEDBACK_OSD_H,
                           np_c_surface_raised(), LV_OPA_COVER,
                           NP_RADIUS_SURFACE);
    feedback_make_passive(feedback.osd);
    feedback_apply_surface(feedback.osd);

    feedback.osd_icon = np_label(feedback.osd,
                                 "",
                                 NP_FONT_ICON,
                                 np_c_accent(),
                                 FEEDBACK_OSD_PAD,
                                 28,
                                 28,
                                 LV_TEXT_ALIGN_CENTER);

    feedback.osd_title = np_label(feedback.osd,
                                  "",
                                  NP_FONT_MD,
                                  np_c_text(),
                                  68,
                                  26,
                                  178,
                                  LV_TEXT_ALIGN_LEFT);

    feedback.osd_value = np_label(feedback.osd,
                                  "",
                                  NP_FONT_LG,
                                  np_c_text(),
                                  252,
                                  22,
                                  84,
                                  LV_TEXT_ALIGN_RIGHT);

    feedback.osd_track = np_fill(feedback.osd,
                                 FEEDBACK_OSD_TRACK_X,
                                 FEEDBACK_OSD_TRACK_Y,
                                 FEEDBACK_OSD_TRACK_W,
                                 FEEDBACK_OSD_TRACK_H,
                                 np_c_hairline(), LV_OPA_COVER,
                                 FEEDBACK_OSD_TRACK_H / 2);
    feedback_make_passive(feedback.osd_track);

    feedback.osd_indicator = np_fill(feedback.osd,
                                     FEEDBACK_OSD_TRACK_X,
                                     FEEDBACK_OSD_TRACK_Y,
                                     4,
                                     FEEDBACK_OSD_TRACK_H,
                                     np_c_accent(), LV_OPA_COVER,
                                     FEEDBACK_OSD_TRACK_H / 2);
    feedback_make_passive(feedback.osd_indicator);
    np_set_visible(feedback.osd_indicator, false);

    /* ---------------------------------------------------------------------- */
    /* Banner compacto no header                                              */
    /* ---------------------------------------------------------------------- */

    feedback.banner = np_fill(feedback.root,
                              FEEDBACK_BANNER_X, FEEDBACK_BANNER_Y,
                              FEEDBACK_BANNER_W, FEEDBACK_BANNER_H,
                              np_c_surface_raised(), LV_OPA_COVER,
                              NP_RADIUS_CONTROL);
    feedback_make_passive(feedback.banner);
    feedback_apply_surface(feedback.banner);

    feedback.banner_accent = np_fill(feedback.banner,
                                     0, 0,
                                     FEEDBACK_BANNER_ACCENT_W,
                                     FEEDBACK_BANNER_H,
                                     np_c_accent(), LV_OPA_COVER,
                                     NP_RADIUS_CONTROL);
    feedback_make_passive(feedback.banner_accent);

    feedback.banner_icon = np_label(feedback.banner,
                                    NP_ICON_INFO,
                                    NP_FONT_ICON,
                                    np_c_accent(),
                                    16, 10, 24,
                                    LV_TEXT_ALIGN_CENTER);

    feedback.banner_text = np_label(feedback.banner,
                                    "",
                                    NP_FONT_SM,
                                    np_c_text(),
                                    52, 12, 298,
                                    LV_TEXT_ALIGN_LEFT);

    /* ---------------------------------------------------------------------- */
    /* Timers                                                                 */
    /* ---------------------------------------------------------------------- */

    feedback.toast_timer =
        lv_timer_create(feedback_timer_cb, 2000U, feedback.toast);
    feedback.osd_timer =
        lv_timer_create(feedback_timer_cb, 1400U, feedback.osd);

    if (feedback.toast_timer != NULL) {
        lv_timer_pause(feedback.toast_timer);
    }
    if (feedback.osd_timer != NULL) {
        lv_timer_pause(feedback.osd_timer);
    }

    np_set_visible(feedback.toast, false);
    np_set_visible(feedback.osd, false);
    np_set_visible(feedback.banner, false);

    return feedback;
}

void np_feedback_show_toast(np_feedback_t *feedback, np_feedback_kind_t kind,
                            const char *title, const char *detail,
                            uint32_t duration_ms)
{
    if (feedback == NULL || feedback->toast == NULL) return;

    const uint32_t duration =
        duration_ms != 0U
            ? duration_ms
            : (kind == NP_FEEDBACK_ERROR
                   ? 4000U
                   : kind == NP_FEEDBACK_WARNING ? 3000U : 2000U);

    const lv_color_t color = feedback_color(kind);
    const bool has_detail = detail != NULL && detail[0] != '\0';

    np_set_text(feedback->toast_icon, feedback_icon(kind));
    np_set_text_color(feedback->toast_icon, color);
    np_set_bg_color(feedback->toast_icon_bg, color);
    np_set_bg_color(feedback->toast_accent, color);

    np_set_text(feedback->toast_title, title != NULL ? title : "");
    lv_obj_set_y(feedback->toast_title, has_detail ? 13 : 29);

    if (has_detail) {
        np_set_text(feedback->toast_detail, detail);
        np_set_visible(feedback->toast_detail, true);
    } else {
        np_set_visible(feedback->toast_detail, false);
    }

    np_set_visible(feedback->toast, true);
    np_feedback_bring_to_front(feedback);
    feedback_schedule(feedback->toast_timer, duration);
}

void np_feedback_show_osd(np_feedback_t *feedback, const char *icon,
                          const char *title, const char *value,
                          uint32_t duration_ms)
{
    if (feedback == NULL || feedback->osd == NULL) return;

    const uint8_t percent = feedback_percent_from_value(value);
    const bool muted = feedback_is_volume_title(title) && percent == 0U;

    np_set_text(feedback->osd_icon, icon != NULL ? icon : "");
    np_set_text(feedback->osd_title, title != NULL ? title : "");
    np_set_text(feedback->osd_value,
                muted && value != NULL && value[0] != '\0'
                    ? "Mudo"
                    : (value != NULL ? value : ""));
    feedback_update_osd_bar(feedback, percent);

    np_set_visible(feedback->osd, true);
    np_feedback_bring_to_front(feedback);

    feedback_schedule(feedback->osd_timer,
                      duration_ms != 0U ? duration_ms : 1400U);
}

void np_feedback_show_banner(np_feedback_t *feedback, np_feedback_kind_t kind,
                             const char *text)
{
    if (feedback == NULL || feedback->banner == NULL) return;

    const lv_color_t color = feedback_color(kind);

    np_set_text(feedback->banner_icon, feedback_icon(kind));
    np_set_text_color(feedback->banner_icon, color);
    np_set_bg_color(feedback->banner_accent, color);
    np_set_text(feedback->banner_text, text != NULL ? text : "");

    np_set_visible(feedback->banner, true);
    np_feedback_bring_to_front(feedback);
}

void np_feedback_hide_banner(np_feedback_t *feedback)
{
    if (feedback != NULL) {
        np_set_visible(feedback->banner, false);
    }
}

void np_feedback_hide_toast(np_feedback_t *feedback)
{
    if (feedback == NULL) return;

    np_set_visible(feedback->toast, false);
    if (feedback->toast_timer != NULL) {
        lv_timer_pause(feedback->toast_timer);
    }
}

void np_feedback_hide_osd(np_feedback_t *feedback)
{
    if (feedback == NULL) return;

    np_set_visible(feedback->osd, false);
    if (feedback->osd_timer != NULL) {
        lv_timer_pause(feedback->osd_timer);
    }
}

void np_feedback_hide_all(np_feedback_t *feedback)
{
    np_feedback_hide_toast(feedback);
    np_feedback_hide_osd(feedback);
    np_feedback_hide_banner(feedback);
}

void np_feedback_bring_to_front(np_feedback_t *feedback)
{
    if (feedback != NULL && feedback->root != NULL) {
        lv_obj_move_foreground(feedback->root);
    }
}

void np_feedback_destroy(np_feedback_t *feedback)
{
    if (feedback == NULL) return;

    if (feedback->toast_timer != NULL) {
        lv_timer_delete(feedback->toast_timer);
        feedback->toast_timer = NULL;
    }

    if (feedback->osd_timer != NULL) {
        lv_timer_delete(feedback->osd_timer);
        feedback->osd_timer = NULL;
    }

    if (feedback->root != NULL) {
        lv_obj_delete(feedback->root);
    }

    *feedback = (np_feedback_t){0};
}
