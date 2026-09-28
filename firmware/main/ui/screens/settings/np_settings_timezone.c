#include "np_settings_timezone.h"

#include <ctype.h>
#include <string.h>

#define TZ_X 192
#define TZ_Y 52
#define TZ_W 640
#define TZ_H 496
#define TZ_ROW_H 58
#define TZ_REGION_COUNT 12U

typedef struct {
    const char *label;
    const char *prefix;
} timezone_region_t;

static const timezone_region_t s_regions[TZ_REGION_COUNT] = {
    {"Todas", NULL},
    {"America", "America/"},
    {"Europa", "Europe/"},
    {"Africa", "Africa/"},
    {"Asia", "Asia/"},
    {"Australia", "Australia/"},
    {"Pacifico", "Pacific/"},
    {"Atlantico", "Atlantic/"},
    {"Indico", "Indian/"},
    {"Antartida", "Antarctica/"},
    {"Artico", "Arctic/"},
    {"Outros", "Etc/"},
};

static const char s_region_options[] =
    "Todas\nAmerica\nEuropa\nAfrica\nAsia\nAustralia\nPacifico\n"
    "Atlantico\nIndico\nAntartida\nArtico\nOutros";

static uint8_t region_for_index(uint16_t index)
{
    timezone_catalog_entry_t entry = {0};
    if (!timezone_catalog_get(index, &entry)) return 0U;
    for (uint8_t region = 1U; region < TZ_REGION_COUNT; ++region) {
        const size_t prefix_length = strlen(s_regions[region].prefix);
        if (entry.iana_length >= prefix_length &&
            memcmp(entry.iana, s_regions[region].prefix, prefix_length) == 0) {
            return region;
        }
    }
    return 0U;
}

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
        if ((timezone->selected_region == 0U ||
             timezone->region_by_index[i] == timezone->selected_region) && matches(i, query)) {
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

static void region_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    np_settings_timezone_t *const timezone = lv_event_get_user_data(event);
    if (timezone == NULL) return;
    timezone->selected_region = (uint8_t)lv_dropdown_get_selected(timezone->region);
    filter(timezone, lv_textarea_get_text(timezone->search));
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
    np_label(timezone->modal.content, "Regiao", NP_FONT_SM, np_c_text_2(),
             24, 12, 96, LV_TEXT_ALIGN_LEFT);
    timezone->region = lv_dropdown_create(timezone->modal.content);
    lv_obj_remove_style_all(timezone->region);
    lv_obj_set_pos(timezone->region, 134, 0);
    lv_obj_set_size(timezone->region, TZ_W - 158, NP_INPUT_H);
    np_apply_input_style(timezone->region);
    lv_obj_set_style_bg_color(timezone->region, np_c_surface_raised(), LV_PART_SELECTED);
    lv_obj_set_style_text_font(timezone->region, np_font_text_with_icons(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(timezone->region, np_c_text(), LV_PART_SELECTED);
    lv_dropdown_set_options_static(timezone->region, s_region_options);
    lv_dropdown_set_symbol(timezone->region, NP_ICON_ARROW_DOWN);
    np_label(timezone->modal.content, "Cidade", NP_FONT_SM, np_c_text_2(),
             24, 72, 96, LV_TEXT_ALIGN_LEFT);
    timezone->search = np_text_input(timezone->modal.content, 134, 56,
                                     TZ_W - 158, NP_INPUT_H,
                                     "Buscar cidade...", NP_ICON_SEARCH);
    timezone->list = lv_obj_create(timezone->modal.content);
    lv_obj_remove_style_all(timezone->list);
    lv_obj_set_pos(timezone->list, 24, 120);
    lv_obj_set_size(timezone->list, TZ_W - 48, 270);
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
    lv_obj_add_event_cb(timezone->region, region_event, LV_EVENT_VALUE_CHANGED,
                        timezone);
    lv_obj_add_event_cb(timezone->list, scroll_event, LV_EVENT_SCROLL, timezone);
    for (uint16_t i = 0; i < TIMEZONE_CATALOG_COUNT; ++i) {
        timezone->region_by_index[i] = region_for_index(i);
    }
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
    timezone->selected_region = timezone->region_by_index[selected];
    lv_dropdown_set_selected(timezone->region, timezone->selected_region);
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
