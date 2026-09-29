#pragma once

#include "esp_err.h"
#include "np_modal.h"
#include "timezone_catalog.h"
#include "np_keyboard.h"

#define NP_SETTINGS_TIMEZONE_ROW_POOL 7U

typedef esp_err_t (*np_settings_timezone_select_cb_t)(void *user_data, uint16_t index);

typedef struct {
    struct np_settings_timezone *owner;
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *radio;
    uint16_t catalog_index;
} np_settings_timezone_row_t;

typedef struct np_settings_timezone {
    np_modal_t modal;
    lv_obj_t *region;
    lv_obj_t *search;
    lv_obj_t *list;
    lv_obj_t *spacer;
    np_settings_timezone_row_t rows[NP_SETTINGS_TIMEZONE_ROW_POOL];
    uint16_t filtered[TIMEZONE_CATALOG_COUNT];
    uint8_t region_by_index[TIMEZONE_CATALOG_COUNT];
    uint16_t filtered_count;
    uint16_t selected_index;
    uint8_t selected_region;
    uint8_t row_height;
    uint16_t viewport_start;
    struct np_settings_timezone_view *scene;
    char selected_label[48];
    np_settings_timezone_select_cb_t select_callback;
    void *select_user_data;
} np_settings_timezone_t;

void np_settings_timezone_create(np_settings_timezone_t *timezone, lv_obj_t *parent);
void np_settings_timezone_bind_row(np_settings_timezone_t *timezone, lv_obj_t *row);
void np_settings_timezone_set_select_callback(np_settings_timezone_t *timezone,
                                               np_settings_timezone_select_cb_t callback,
                                               void *user_data);
void np_settings_timezone_set_close_callback(np_settings_timezone_t *timezone,
                                              np_modal_close_cb_t callback, void *user_data);
void np_settings_timezone_show(np_settings_timezone_t *timezone, uint16_t selected_index);
void np_settings_timezone_hide(np_settings_timezone_t *timezone);
void np_settings_timezone_sync(np_settings_timezone_t *timezone, uint16_t selected_index);
const char *np_settings_timezone_selected_label(const np_settings_timezone_t *timezone);

/* Stable product_ui-owned scene; callbacks never refer to a returned stack view. */
typedef struct np_settings_timezone_view {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    lv_obj_t *selected_title;
    lv_obj_t *selected_offset;
    lv_obj_t *subtitle;
    lv_obj_t *sync_status;
    lv_obj_t *sync_dot;
    lv_obj_t *apply_button;
    lv_obj_t *persistence_status;
    lv_obj_t *empty_label;
    char row_titles[NP_SETTINGS_TIMEZONE_ROW_POOL][64];
    char row_details[NP_SETTINGS_TIMEZONE_ROW_POOL][128];
    np_settings_timezone_t timezone;
    np_keyboard_t *keyboard; /* borrowed global component */
    uint16_t applied_index;
    uint16_t submitted_index;
    bool initialized;
    bool has_draft;
    bool pending;
    bool awaiting_projection;
    esp_err_t last_result;
} np_settings_timezone_view_t;

void np_settings_timezone_scene_create(np_settings_timezone_view_t *view,
    lv_obj_t *parent, np_keyboard_t *keyboard,
    np_settings_timezone_select_cb_t callback, void *user_data);
void np_settings_timezone_scene_enter(np_settings_timezone_view_t *view, uint16_t selected);
void np_settings_timezone_scene_sync(np_settings_timezone_view_t *view, uint16_t applied,
    bool persistence_pending, esp_err_t result, bool time_trusted);
