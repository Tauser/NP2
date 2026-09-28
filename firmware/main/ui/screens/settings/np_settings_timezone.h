#pragma once

#include "esp_err.h"
#include "np_modal.h"
#include "timezone_catalog.h"

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
