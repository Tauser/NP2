#include "np_settings_timezone.h"

#include <ctype.h>
#include <string.h>

#define TZ_X 192
#define TZ_Y 52
#define TZ_W 640
#define TZ_H 496
#define TZ_ROW_H 58

static void format_label(uint16_t index, char *out, size_t size)
{
    static const char *const legacy[] = {
        "Sao Paulo (GMT-3)",
        "Brasilia (GMT-3)",
        "Buenos Aires (GMT-3)",
        "Nova York (GMT-5)",
        "Londres (GMT+0)",
    };
    if (index < 5U) { lv_snprintf(out, size, "%s", legacy[index]); return; }
    char iana[48] = {0};
    if (!timezone_catalog_copy_iana(index, iana, sizeof(iana))) { lv_snprintf(out, size, "Fuso horario"); return; }
    char *name = strrchr(iana, '/');
    name = name == NULL ? iana : name + 1;
    for (char *p = name; *p != '\0'; ++p) if (*p == '_') *p = ' ';
    lv_snprintf(out, size, "%s", name);
}

static bool matches(uint16_t index, const char *query)
{
    if (query == NULL || query[0] == '\0') return true;
    char iana[48] = {0}; char posix[56] = {0};
    (void)timezone_catalog_copy_iana(index, iana, sizeof(iana));
    (void)timezone_catalog_copy_posix(index, posix, sizeof(posix));
    char text[108] = {0};
    lv_snprintf(text, sizeof(text), "%s %s", iana, posix);
    for (char *p = text; *p != '\0'; ++p) {
        if (*p == '_') *p = ' ';
    }
    for (char *start = text; *start; ++start) {
        const char *left = start; const char *right = query;
        while (*left && *right && tolower((unsigned char)*left) == tolower((unsigned char)*right)) { ++left; ++right; }
        if (*right == '\0') return true;
    }
    return false;
}

static void refresh_rows(np_settings_timezone_t *timezone)
{
    const int32_t first = lv_obj_get_scroll_y(timezone->list) / TZ_ROW_H;
    for (uint8_t i = 0; i < NP_SETTINGS_TIMEZONE_ROW_POOL; ++i) {
        np_settings_timezone_row_t *row = &timezone->rows[i];
        const uint16_t position = (uint16_t)(first + i);
        const bool visible = position < timezone->filtered_count;
        np_set_visible(row->root, visible);
        if (!visible) continue;
        row->catalog_index = timezone->filtered[position];
        char label[48] = {0}; char iana[48] = {0};
        format_label(row->catalog_index, label, sizeof(label));
        (void)timezone_catalog_copy_iana(row->catalog_index, iana, sizeof(iana));
        np_set_text(row->title, label); np_set_text(row->detail, iana);
        lv_obj_set_y(row->root, position * TZ_ROW_H);
        lv_obj_set_style_border_color(row->root,
                                      row->catalog_index == timezone->selected_index
                                          ? np_c_accent() : np_c_hairline(), 0);
        const bool selected = row->catalog_index == timezone->selected_index;
        lv_obj_set_style_border_color(row->radio,
                                      selected ? np_c_accent() : np_c_text_3(), 0);
        np_set_visible(lv_obj_get_child(row->radio, 0), selected);
    }
}

static void filter(np_settings_timezone_t *timezone, const char *query)
{
    timezone->filtered_count = 0;
    for (uint16_t i = 0; i < TIMEZONE_CATALOG_COUNT; ++i) {
        if (matches(i, query)) {
            timezone->filtered[timezone->filtered_count++] = i;
        }
    }
    lv_obj_set_y(timezone->spacer, timezone->filtered_count * TZ_ROW_H);
    lv_obj_scroll_to_y(timezone->list, 0, LV_ANIM_OFF);
    refresh_rows(timezone);
}

static void search_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    filter(lv_event_get_user_data(event),
           lv_textarea_get_text(lv_event_get_target(event)));
}

static void scroll_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_SCROLL) return;
    refresh_rows(lv_event_get_user_data(event));
}
static void row_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_timezone_row_t *const row = lv_event_get_user_data(event);
    np_settings_timezone_t *const timezone = row == NULL ? NULL : row->owner;
    if (row == NULL || timezone == NULL) return;
    if (timezone->select_callback == NULL ||
        timezone->select_callback(timezone->select_user_data, row->catalog_index) == ESP_OK) {
        timezone->selected_index = row->catalog_index;
        format_label(row->catalog_index, timezone->selected_label,
                     sizeof(timezone->selected_label));
        refresh_rows(timezone);
    }
}
static void row_open(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_timezone_t *const timezone = lv_event_get_user_data(event);
    if (timezone != NULL) np_settings_timezone_show(timezone, timezone->selected_index);
}

void np_settings_timezone_create(np_settings_timezone_t *timezone, lv_obj_t *parent)
{
    if (!timezone || !parent) return;
    *timezone = (np_settings_timezone_t){0};
    np_modal_create(&timezone->modal, parent, TZ_X, TZ_Y, TZ_W, TZ_H,
                    NP_ICON_CALENDAR, np_c_accent(), "Fuso horario", NULL);
    timezone->search = lv_textarea_create(timezone->modal.content);
    lv_obj_remove_style_all(timezone->search);
    lv_obj_set_pos(timezone->search, 24, 0);
    lv_obj_set_size(timezone->search, TZ_W - 48, 44);
    lv_obj_set_style_bg_color(timezone->search, np_c_surface_raised(), 0);
    lv_obj_set_style_bg_opa(timezone->search, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(timezone->search, NP_RADIUS_CONTROL, 0);
    lv_obj_set_style_border_width(timezone->search, 1, 0);
    lv_obj_set_style_border_color(timezone->search, np_c_hairline(), 0);
    lv_obj_set_style_text_font(timezone->search, NP_FONT_SM, 0);
    lv_obj_set_style_text_color(timezone->search, np_c_text(), 0);
    lv_obj_set_style_text_color(timezone->search, np_c_text_3(),
                                LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_left(timezone->search, 42, 0);
    lv_textarea_set_one_line(timezone->search, true);
    lv_textarea_set_placeholder_text(timezone->search, "Buscar cidade ou regiao...");
    np_label(timezone->modal.content, NP_ICON_SEARCH, NP_FONT_ICON,
             np_c_text_2(), 36, 10, 24, LV_TEXT_ALIGN_CENTER);
    timezone->list = lv_obj_create(timezone->modal.content);
    lv_obj_remove_style_all(timezone->list);
    lv_obj_set_pos(timezone->list, 24, 55);
    lv_obj_set_size(timezone->list, TZ_W - 48, 280);
    lv_obj_set_scroll_dir(timezone->list, LV_DIR_VER);
    timezone->spacer = np_fill(timezone->list, 0, 0, 1, 1, np_c_surface(), LV_OPA_TRANSP, 0);
    for (uint8_t i = 0; i < NP_SETTINGS_TIMEZONE_ROW_POOL; ++i) {
        np_settings_timezone_row_t *const row = &timezone->rows[i];
        row->owner = timezone;
        row->root = np_fill(timezone->list, 0, 0, TZ_W - 48, TZ_ROW_H - 3,
                            np_c_surface_raised(), LV_OPA_COVER, NP_RADIUS_CONTROL);
        lv_obj_set_style_border_width(row->root, 1, 0);
        row->title = np_label(row->root, "", NP_FONT_SM, np_c_text(),
                              16, 7, 450, LV_TEXT_ALIGN_LEFT);
        row->detail = np_label(row->root, "", NP_FONT_SM, np_c_text_2(),
                               16, 31, 470, LV_TEXT_ALIGN_LEFT);
        row->radio = np_fill(row->root, 548, 18, 20, 20,
                             np_c_surface_raised(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(row->radio, 2, 0);
        np_dot(row->radio, 4, 4, 12, np_c_accent());
        lv_obj_add_flag(row->root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row->root, row_event, LV_EVENT_CLICKED, row);
    }
    lv_obj_add_event_cb(timezone->search, search_event, LV_EVENT_VALUE_CHANGED,
                        timezone);
    lv_obj_add_event_cb(timezone->list, scroll_event, LV_EVENT_SCROLL, timezone);
    filter(timezone, "");
}

void np_settings_timezone_bind_row(np_settings_timezone_t *timezone, lv_obj_t *row)
{
    if (timezone != NULL && row != NULL) {
        lv_obj_add_event_cb(row, row_open, LV_EVENT_CLICKED, timezone);
    }
}

void np_settings_timezone_set_select_callback(np_settings_timezone_t *timezone,
                                              np_settings_timezone_select_cb_t callback,
                                              void *user_data)
{
    if (timezone == NULL) return;
    timezone->select_callback = callback;
    timezone->select_user_data = user_data;
}

void np_settings_timezone_set_close_callback(np_settings_timezone_t *timezone,
                                             np_modal_close_cb_t callback,
                                             void *user_data)
{
    if (timezone != NULL) np_modal_set_close_callback(&timezone->modal, callback, user_data);
}

void np_settings_timezone_show(np_settings_timezone_t *timezone, uint16_t selected)
{
    if (timezone == NULL) return;
    timezone->selected_index = selected;
    lv_textarea_set_text(timezone->search, "");
    filter(timezone, "");
    np_modal_show(&timezone->modal);
}

void np_settings_timezone_hide(np_settings_timezone_t *timezone)
{
    if (timezone != NULL) np_modal_hide(&timezone->modal);
}

void np_settings_timezone_sync(np_settings_timezone_t *timezone, uint16_t selected)
{
    if (timezone == NULL || np_modal_is_visible(&timezone->modal)) return;
    timezone->selected_index = selected;
    format_label(selected, timezone->selected_label, sizeof(timezone->selected_label));
}

const char *np_settings_timezone_selected_label(const np_settings_timezone_t *timezone)
{
    return timezone != NULL ? timezone->selected_label : "--";
}
