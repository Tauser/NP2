#pragma once
#include "agenda_service.h"
#include "np_components.h"
#include "np_keyboard.h"
#include "np_modal.h"
#define NP_AGENDA_ROWS 6U
typedef struct {
    lv_obj_t *root, *status, *empty, *page_label, *editor_title, *message;
    lv_obj_t *status_dot, *count, *empty_hint, *empty_icon;
    lv_obj_t *notice, *cancel;
    np_modal_t editor;
    lv_obj_t *filters[3];
    lv_obj_t *new_event, *new_task, *sync, *link, *previous, *next;
    lv_obj_t *rows[NP_AGENDA_ROWS], *titles[NP_AGENDA_ROWS], *details[NP_AGENDA_ROWS];
    lv_obj_t *times[NP_AGENDA_ROWS], *dates[NP_AGENDA_ROWS];
    lv_obj_t *kinds[NP_AGENDA_ROWS];
    uint32_t row_ids[NP_AGENDA_ROWS];
    lv_obj_t *title, *date, *end_date, *start, *end, *reminder;
    lv_obj_t *start_label, *end_label, *end_time_label, *reminder_label;
    lv_obj_t *save, *remove, *complete;
    agenda_projection_t state;
    agenda_item_t draft;
    uint32_t pending_request;
    uint32_t notice_expiry;
    uint8_t page, filter, visible_count;
    bool creating, delete_armed, persistence_feedback, notice_active, connection_info;
    np_keyboard_t *keyboard;
} np_agenda_view_t;
np_agenda_view_t np_agenda_build(lv_obj_t *parent);
void np_agenda_bind(np_agenda_view_t *view, np_keyboard_t *keyboard);
void np_agenda_sync(np_agenda_view_t *view, const agenda_projection_t *state);
