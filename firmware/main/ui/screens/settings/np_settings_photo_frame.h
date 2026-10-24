#pragma once
#include "np_components.h"
#include "np_modal.h"
#include "photo_frame_model.h"
#include "photo_thumbnail_service.h"

typedef struct {
    np_modal_t modal;
    lv_obj_t *enabled[2], *position[2], *size[2], *contrast[2];
    lv_obj_t *save;
} np_photo_widget_editor_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    lv_obj_t *photo_switch, *photo_idle, *photo_interval, *photo_exit, *photo_preview, *photo_widgets;
    lv_obj_t *summary, *page_label, *previous, *next, *status;
    lv_obj_t *tiles[PHOTO_PAGE_SIZE], *images[PHOTO_PAGE_SIZE];
    lv_obj_t *names[PHOTO_PAGE_SIZE], *checks[PHOTO_PAGE_SIZE], *placeholders[PHOTO_PAGE_SIZE];
    np_photo_widget_editor_t widget_editor;
    uint64_t catalog, enabled_mask;
    uint8_t page, ids[PHOTO_PAGE_SIZE];
} np_settings_photo_frame_view_t;

np_settings_photo_frame_view_t np_settings_photo_frame_build(lv_obj_t *parent);
void np_settings_photo_frame_sync(np_settings_photo_frame_view_t *view,
    const photo_frame_status_t *status, uint64_t enabled_mask, bool enabled,
    uint8_t idle_choice, uint8_t interval_choice, uint8_t exit_choice, bool saving);
void np_settings_photo_frame_set_thumbnails(np_settings_photo_frame_view_t *view,
    const photo_thumbnail_page_t *page);
void np_photo_widget_editor_open(np_settings_photo_frame_view_t *view, uint8_t bitcoin, uint8_t clock);
uint8_t np_photo_widget_editor_value(const np_photo_widget_editor_t *editor, unsigned index);
