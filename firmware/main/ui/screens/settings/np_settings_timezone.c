#include "np_settings_timezone.h"

#include <ctype.h>
#include <string.h>

#define TZ_X 192
#define TZ_Y 52
#define TZ_W 640
#define TZ_H 496
#define TZ_ROW_H 58
static lv_font_t s_apply_font;

static const char s_region_options[] = "Brasil";

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

static void offset_label(uint16_t index, char *out, size_t size, const char *prefix)
{
    int16_t minutes = 0;
    if (!timezone_catalog_standard_offset(index, &minutes)) {
        lv_snprintf(out, size, "Indisponível"); return;
    }
    int magnitude = minutes < 0 ? -minutes : minutes;
    if (magnitude % 60 == 0)
        lv_snprintf(out, size, "%s%+d", prefix, minutes / 60);
    else
        lv_snprintf(out, size, "%s%c%d:%02d", prefix,
            minutes < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
}

/* Fold common Latin diacritics and spaces for city/country searches. Bounded
 * stack buffers only; no allocation per key and no locale/global TZ mutation. */
static void normalize(const char *source, char *out, size_t size)
{
    size_t n = 0;
    for (size_t i = 0; source[i] && n + 1 < size; ++i) {
        unsigned char ch = (unsigned char)source[i];
        if (ch == ' ' || ch == '_') continue;
        if (ch == 0xc3 && source[i + 1]) {
            unsigned char next = (unsigned char)source[++i];
            if ((next >= 0x80 && next <= 0x85) || (next >= 0xa0 && next <= 0xa5)) ch = 'a';
            else if (next == 0x87 || next == 0xa7) ch = 'c';
            else if ((next >= 0x88 && next <= 0x8b) || (next >= 0xa8 && next <= 0xab)) ch = 'e';
            else if ((next >= 0x8c && next <= 0x8f) || (next >= 0xac && next <= 0xaf)) ch = 'i';
            else if (next == 0x91 || next == 0xb1) ch = 'n';
            else if ((next >= 0x92 && next <= 0x96) || (next >= 0xb2 && next <= 0xb6)) ch = 'o';
            else if ((next >= 0x99 && next <= 0x9c) || (next >= 0xb9 && next <= 0xbc)) ch = 'u';
            else ch = next;
        }
        out[n++] = (char)tolower(ch);
    }
    out[n] = '\0';
}

static bool matches(uint16_t index, const char *normalized_query)
{
    if (normalized_query == NULL || normalized_query[0] == '\0') return true;
    char iana[64] = {0}, posix[72] = {0}, utc[32], gmt[32], text[320], normalized[320];
    (void)timezone_catalog_copy_iana(index, iana, sizeof(iana));
    (void)timezone_catalog_copy_posix(index, posix, sizeof(posix));
    offset_label(index, utc, sizeof(utc), "UTC");
    offset_label(index, gmt, sizeof(gmt), "GMT");
    lv_snprintf(text, sizeof(text), "%s %s %s %s %s %s", iana, posix,
        timezone_catalog_country(index), utc, gmt,
        index == TIMEZONE_CATALOG_BRASILIA ? "Brasilia Brazil" :
        index == TIMEZONE_CATALOG_NEW_YORK ? "Nova York United States" :
        index == TIMEZONE_CATALOG_LONDON ? "Londres United Kingdom" :
        index == TIMEZONE_CATALOG_SAO_PAULO ? "Brazil" : "");
    normalize(text, normalized, sizeof(normalized));
    return strstr(normalized, normalized_query) != NULL;
}

static void update_selection(np_settings_timezone_view_t *view)
{
    char iana[64] = {0}, offset[32] = {0};
    (void)timezone_catalog_copy_iana(view->timezone.selected_index, iana, sizeof(iana));
    offset_label(view->timezone.selected_index, offset, sizeof(offset), "UTC");
    np_set_text(view->selected_title, iana);
    np_set_text(view->selected_offset, offset);
    const bool dirty = view->timezone.selected_index != view->applied_index;
    np_set_text(view->subtitle, dirty ? "Seleção pronta para aplicar" : "Fuso horário do sistema");
    np_set_text(view->persistence_status, view->pending || view->awaiting_projection
        ? "Salvando preferência..." : view->last_result != ESP_OK
        ? "Falha ao salvar · tente aplicar novamente" : dirty
        ? "Toque em Aplicar para confirmar" : "Preferência salva");
    if (!view->awaiting_projection)
        lv_obj_remove_state(view->apply_button, LV_STATE_DISABLED);
    else lv_obj_add_state(view->apply_button, LV_STATE_DISABLED);
}

static void set_row_text(lv_obj_t *label, char *buffer, size_t size, const char *text)
{
    if (strcmp(buffer, text) == 0) return;
    lv_snprintf(buffer, size, "%s", text);
    lv_label_set_text_static(label, buffer);
}

static void refresh_rows(np_settings_timezone_t *timezone)
{
    const int32_t scroll = lv_obj_get_scroll_y(timezone->list);
    const uint8_t height = timezone->row_height != 0 ? timezone->row_height : TZ_ROW_H;
    const uint16_t first = scroll > 0 ? (uint16_t)(scroll / height) : 0U;
    timezone->viewport_start = first;
    for (uint8_t i = 0; i < NP_SETTINGS_TIMEZONE_ROW_POOL; ++i) {
        np_settings_timezone_row_t *row = &timezone->rows[i];
        const uint16_t position = first + i;
        const bool visible = position < timezone->filtered_count;
        np_set_visible(row->root, visible);
        if (!visible) continue;
        row->catalog_index = timezone->filtered[position];
        char label[64] = {0}, iana[64] = {0};
        format_label(row->catalog_index, label, sizeof(label));
        (void)timezone_catalog_copy_iana(row->catalog_index, iana, sizeof(iana));
        const bool selected = row->catalog_index == timezone->selected_index;
        if (timezone->scene != NULL) {
            char offset[32], detail[128];
            offset_label(row->catalog_index, offset, sizeof(offset), "UTC");
            const char *country = row->catalog_index == TIMEZONE_CATALOG_SAO_PAULO
                ? "Brasília, São Paulo" : row->catalog_index == TIMEZONE_CATALOG_BRASILIA
                ? "Brasília" : timezone_catalog_country(row->catalog_index);
            lv_snprintf(detail, sizeof(detail), "%s · %s", offset,
                country[0] ? country : label);
            set_row_text(row->title, timezone->scene->row_titles[i],
                sizeof(timezone->scene->row_titles[i]), iana);
            set_row_text(row->detail, timezone->scene->row_details[i],
                sizeof(timezone->scene->row_details[i]), detail);
            np_set_bg_color(row->root, selected ? np_c_accent_bg() : np_c_surface());
            lv_obj_set_style_border_width(row->root, 1, 0);
            lv_obj_set_style_border_side(row->root, selected ? LV_BORDER_SIDE_FULL : LV_BORDER_SIDE_BOTTOM, 0);
            const char *icon = selected ? NP_ICON_CHECK : NP_ICON_ARROW_RIGHT;
            if (strcmp(lv_label_get_text(row->radio), icon) != 0)
                lv_label_set_text_static(row->radio, icon);
        } else {
            np_set_text(row->title, label); np_set_text(row->detail, iana);
            lv_obj_set_style_border_color(row->radio, selected ? np_c_accent() : np_c_text_3(), 0);
            np_set_visible(lv_obj_get_child(row->radio, 0), selected);
        }
        lv_obj_set_y(row->root, position * height);
        lv_obj_set_style_border_color(row->root, selected ? np_c_accent() : np_c_hairline(), 0);
    }
    if (timezone->scene != NULL) {
        np_set_visible(timezone->scene->empty_label, timezone->filtered_count == 0);
        update_selection(timezone->scene);
    }
}

static void filter(np_settings_timezone_t *timezone, const char *query)
{
    char normalized_query[128];
    normalize(query != NULL ? query : "", normalized_query, sizeof(normalized_query));
    timezone->filtered_count = 0;
    for (uint16_t i = 0; i < TIMEZONE_CATALOG_COUNT; ++i) {
        /* Temporary product scope; keep persisted catalog indices intact. */
        if (strcmp(timezone_catalog_country(i), "Brasil") != 0) continue;
        if (matches(i, normalized_query) &&
            timezone->filtered_count < NP_SETTINGS_TIMEZONE_RESULT_CAPACITY) {
            timezone->filtered[timezone->filtered_count++] = i;
        }
    }
    lv_obj_set_y(timezone->spacer, timezone->filtered_count * (timezone->row_height != 0 ? timezone->row_height : TZ_ROW_H));
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
    if (timezone->scene != NULL) np_keyboard_hide(timezone->scene->keyboard);
    if (timezone->scene != NULL || timezone->select_callback == NULL ||
        timezone->select_callback(timezone->select_user_data, row->catalog_index) == ESP_OK) {
        timezone->selected_index = row->catalog_index;
        if (timezone->scene != NULL) timezone->scene->has_draft = true;
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
             24, 0, TZ_W - 48, LV_TEXT_ALIGN_LEFT);
    timezone->region = np_form_dropdown(timezone->modal.content, 24, 24,
                                        TZ_W - 48, NP_INPUT_H);
    lv_dropdown_set_options_static(timezone->region, s_region_options);
    np_label(timezone->modal.content, "Cidade", NP_FONT_SM, np_c_text_2(),
             24, 80, TZ_W - 48, LV_TEXT_ALIGN_LEFT);
    timezone->search = np_form_text_input(timezone->modal.content, 24, 104,
                                          TZ_W - 48, NP_INPUT_H,
                                          "Buscar cidade...", NP_ICON_SEARCH);
    lv_textarea_set_max_length(timezone->search, 48);
    timezone->list = lv_obj_create(timezone->modal.content);
    lv_obj_remove_style_all(timezone->list);
    lv_obj_set_pos(timezone->list, 24, 168);
    lv_obj_set_size(timezone->list, TZ_W - 48, 224);
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
        /* Decorative children must not swallow the row's selection tap. */
        lv_obj_remove_flag(row->radio, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(lv_obj_get_child(row->radio, 0), LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row->root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row->root, row_event, LV_EVENT_CLICKED, row);
    }
    lv_obj_add_event_cb(timezone->search, search_event, LV_EVENT_VALUE_CHANGED,
                        timezone);
    lv_obj_add_event_cb(timezone->region, region_event, LV_EVENT_VALUE_CHANGED,
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
    if (!timezone_catalog_is_valid(selected)) selected = TIMEZONE_CATALOG_SAO_PAULO;
    timezone->selected_index = selected;
    lv_dropdown_set_selected(timezone->region, 0U);
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

static void scene_search_clicked(lv_event_t *event)
{
    np_settings_timezone_view_t *view = lv_event_get_user_data(event);
    np_keyboard_focus(view->keyboard, view->timezone.search, NP_KEYBOARD_MODE_TEXT);
}

static void scene_apply_clicked(lv_event_t *event)
{
    np_settings_timezone_view_t *view = lv_event_get_user_data(event);
    if (view->awaiting_projection || view->timezone.select_callback == NULL) return;
    /* Confirming the current choice is idempotent and must not wear flash. */
    if (view->timezone.selected_index == view->applied_index && view->last_result == ESP_OK) {
        view->has_draft = false;
        np_keyboard_hide(view->keyboard);
        update_selection(view);
        return;
    }
    const esp_err_t result = view->timezone.select_callback(
        view->timezone.select_user_data, view->timezone.selected_index);
    view->last_result = result;
    if (result == ESP_OK) {
        view->awaiting_projection = true;
        view->submitted_index = view->timezone.selected_index;
        view->pending = true;
        np_keyboard_hide(view->keyboard);
    }
    update_selection(view);
}

static void scene_badge(lv_obj_t *panel, const char *icon)
{
    lv_obj_t *badge = np_fill(panel, 16, 16, 64, 64, np_c_accent(),
        LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_label(badge, icon, NP_FONT_ICON_BADGE, np_c_text_on_accent(),
        0, (64 - NP_FONT_ICON_BADGE->line_height) / 2, 64, LV_TEXT_ALIGN_CENTER);
}

void np_settings_timezone_scene_create(np_settings_timezone_view_t *view,
    lv_obj_t *parent, np_keyboard_t *keyboard,
    np_settings_timezone_select_cb_t callback, void *user_data)
{
    if (view == NULL || parent == NULL || view->root != NULL) return;
    *view = (np_settings_timezone_view_t){.keyboard = keyboard};
    view->root = np_scene(parent);
    np_set_visible(view->root, false);
    view->header = np_header(view->root);
    view->back_button = np_button(view->root, NP_HEADER_NAV_X, 12, NP_HEADER_NAV_W, NP_TOUCH_TARGET, "Voltar", false);
    lv_obj_t *left = np_panel(view->root, 24, 80, 600, 496);
    lv_obj_t *right = np_panel(view->root, 640, 80, 360, 496);
    scene_badge(left, NP_ICON_CLOCK);
    np_label(left, "Fuso horário", NP_FONT_TITLE, np_c_text(), 96, 16, 480, LV_TEXT_ALIGN_LEFT);
    np_label(left, "Fusos horários do Brasil", NP_FONT_SM, np_c_text_2(), 96, 60, 480, LV_TEXT_ALIGN_LEFT);
    np_settings_timezone_t *tz = &view->timezone;
    tz->scene = view;
    tz->row_height = 54;
    tz->select_callback = callback;
    tz->select_user_data = user_data;
    tz->search = np_form_text_input(left, 16, 96, 568, NP_INPUT_H,
        "Buscar cidade ou GMT...", NP_ICON_SEARCH);
    lv_textarea_set_max_length(tz->search, 48);
    tz->list = lv_obj_create(left);
    lv_obj_remove_style_all(tz->list);
    lv_obj_set_pos(tz->list, 16, 156);
    lv_obj_set_size(tz->list, 568, 324); /* six visible, seven recycled */
    lv_obj_set_scroll_dir(tz->list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(tz->list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_remove_flag(tz->list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    tz->spacer = np_fill(tz->list, 0, 0, 1, 1, np_c_surface(), LV_OPA_TRANSP, 0);
    for (uint8_t i = 0; i < NP_SETTINGS_TIMEZONE_ROW_POOL; ++i) {
        np_settings_timezone_row_t *row = &tz->rows[i];
        row->owner = tz;
        row->root = np_fill(tz->list, 0, i * 54, 568, 52,
            np_c_surface(), LV_OPA_COVER, NP_RADIUS_CONTROL);
        row->title = np_label(row->root, "", NP_FONT_MD, np_c_text(), 64, 3, 456, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_height(row->title, NP_FONT_MD->line_height);
        lv_label_set_long_mode(row->title, LV_LABEL_LONG_CLIP);
        row->detail = np_label(row->root, "", NP_FONT_SM, np_c_text_2(), 64, 30, 456, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_height(row->detail, NP_FONT_SM->line_height);
        lv_label_set_long_mode(row->detail, LV_LABEL_LONG_CLIP);
        lv_label_set_text_static(row->title, view->row_titles[i]);
        lv_label_set_text_static(row->detail, view->row_details[i]);
        np_label(row->root, NP_ICON_GLOBE, NP_FONT_ICON, np_c_text_2(),
            16, (52 - NP_FONT_ICON->line_height) / 2, 32, LV_TEXT_ALIGN_CENTER);
        row->radio = np_label(row->root, NP_ICON_ARROW_RIGHT, NP_FONT_ICON,
            np_c_text_2(), 528, (52 - NP_FONT_ICON->line_height) / 2, 32, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text_static(row->radio, NP_ICON_ARROW_RIGHT);
        lv_obj_add_flag(row->root, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row->root, row_event, LV_EVENT_CLICKED, row);
    }
    view->empty_label = np_label(left, "Nenhum fuso encontrado", NP_FONT_SM, np_c_text_3(),
        32, 188, 536, LV_TEXT_ALIGN_CENTER);
    scene_badge(right, NP_ICON_GLOBE);
    np_label(right, "Selecionado", NP_FONT_LG, np_c_text(), 96, 20, 248, LV_TEXT_ALIGN_LEFT);
    view->subtitle = np_label(right, "Fuso horário do sistema", NP_FONT_SM, np_c_text_2(),
        96, 58, 248, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *details = np_panel(right, 16, 108, 328, 260);
    np_set_bg_color(details, np_c_surface_raised());
    lv_obj_set_style_border_width(details, 1, 0);
    lv_obj_set_style_border_color(details, np_c_hairline(), 0);
    np_label(details, NP_ICON_GLOBE, NP_FONT_ICON, np_c_text_2(), 16, 25, 32, LV_TEXT_ALIGN_CENTER);
    np_label(details, "Fuso horário", NP_FONT_SM, np_c_text_2(), 64, 20, 248, LV_TEXT_ALIGN_LEFT);
    view->selected_title = np_label(details, "", NP_FONT_MD, np_c_text(), 64, 46, 248, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view->selected_title, 48); /* long IANA names can wrap */
    np_hline(details, 64, 100, 248);
    np_label(details, NP_ICON_CLOCK, NP_FONT_ICON, np_c_text_2(), 16, 116, 32, LV_TEXT_ALIGN_CENTER);
    np_label(details, "Offset UTC padrão", NP_FONT_SM, np_c_text_2(), 64, 114, 248, LV_TEXT_ALIGN_LEFT);
    view->selected_offset = np_label(details, "", NP_FONT_LG, np_c_text(), 64, 140, 248, LV_TEXT_ALIGN_LEFT);
    np_hline(details, 64, 182, 248);
    np_label(details, NP_ICON_ROUTER, NP_FONT_ICON, np_c_text_2(), 16, 198, 32, LV_TEXT_ALIGN_CENTER);
    np_label(details, "Sincronização de horário", NP_FONT_SM, np_c_text_2(), 64, 194, 248, LV_TEXT_ALIGN_LEFT);
    view->sync_status = np_label(details, "Aguardando NTP", NP_FONT_MD, np_c_text(), 64, 220, 224, LV_TEXT_ALIGN_LEFT);
    view->sync_dot = np_dot(details, 306, 228, 8, np_c_text_3());
    view->apply_button = np_button(right, 16, 404, 328, 60, NP_ICON_CHECK_CIRCLE "  Aplicar", true);
    s_apply_font = *NP_FONT_LG;
    s_apply_font.fallback = NP_FONT_ICON;
    lv_obj_set_style_text_font(lv_obj_get_child(view->apply_button, 0), &s_apply_font, 0);
    view->persistence_status = np_label(right, "", NP_FONT_SM, np_c_text_3(),
        16, 472, 328, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(tz->search, search_event, LV_EVENT_VALUE_CHANGED, tz);
    lv_obj_add_event_cb(tz->list, scroll_event, LV_EVENT_SCROLL, tz);
    lv_obj_add_event_cb(view->apply_button, scene_apply_clicked, LV_EVENT_CLICKED, view);
    np_keyboard_bind(keyboard, tz->search, NP_KEYBOARD_MODE_TEXT);
    lv_obj_add_event_cb(tz->search, scene_search_clicked, LV_EVENT_CLICKED, view);
    np_settings_timezone_scene_enter(view, TIMEZONE_CATALOG_SAO_PAULO);
}

void np_settings_timezone_scene_enter(np_settings_timezone_view_t *view, uint16_t selected)
{
    if (view == NULL || view->root == NULL) return;
    if (!timezone_catalog_is_valid(selected)) selected = TIMEZONE_CATALOG_SAO_PAULO;
    np_keyboard_hide(view->keyboard);
    lv_obj_remove_state(view->timezone.search, LV_STATE_FOCUSED);
    np_set_visible(view->header.drawer_scrim, false);
    view->timezone.selected_index = selected;
    view->applied_index = selected;
    view->has_draft = false;
    view->awaiting_projection = false;
    view->initialized = true;
    lv_textarea_set_text(view->timezone.search, "");
    filter(&view->timezone, "");
}

void np_settings_timezone_scene_sync(np_settings_timezone_view_t *view, uint16_t applied,
    bool persistence_pending, esp_err_t result, bool time_trusted)
{
    if (view == NULL || view->root == NULL || !timezone_catalog_is_valid(applied)) return;
    np_set_text(view->sync_status, time_trusted ? "NTP sincronizado" : "Aguardando NTP");
    np_set_bg_color(view->sync_dot, time_trusted ? np_c_positive() : np_c_text_3());
    /* A queued request is not a durable write. Ignore the previous projection
     * until app_loop publishes the requested preference. */
    if (view->awaiting_projection && applied != view->submitted_index) return;
    view->awaiting_projection = false;
    if (!view->has_draft) view->timezone.selected_index = applied;
    view->applied_index = applied;
    if (view->timezone.selected_index == applied) view->has_draft = false;
    view->pending = persistence_pending;
    view->last_result = result;
    refresh_rows(&view->timezone);
}
