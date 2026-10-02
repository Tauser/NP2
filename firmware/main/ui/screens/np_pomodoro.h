#pragma once

#include "app_state.h"
#include "np_components.h"

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *progress_arc;
    lv_obj_t *timer_label;
    lv_obj_t *phase_label;
    lv_obj_t *cycle_label;
    lv_obj_t *pause_button;
    lv_obj_t *pause_icon;
    lv_obj_t *play_button;
    lv_obj_t *play_icon;
    lv_obj_t *reset_button;
    lv_obj_t *reset_icon;
    lv_obj_t *preset_buttons[4];
    lv_obj_t *quick_preset_labels[4];
    lv_obj_t *completed_value;
    lv_obj_t *focused_value;
    lv_obj_t *goal_value;
    lv_obj_t *goal_progress_fill;
    int32_t goal_progress_width;
    lv_obj_t *goal_steps[4];
    lv_obj_t *goal_step_checks[4];
    lv_obj_t *goal_connectors[3];
    pomodoro_projection_t state;
} np_pomodoro_view_t;

np_pomodoro_view_t np_pomodoro_build_with_header(lv_obj_t *parent,
                                                  const np_header_t *header);
void np_pomodoro_bind(np_pomodoro_view_t *view);
void np_pomodoro_sync(np_pomodoro_view_t *view,
                      const pomodoro_projection_t *projection);
