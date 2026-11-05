#pragma once
#include "np_components.h"
#include "xiaozhi_service.h"

typedef enum { NP_ASSISTANT_TAP_ORB = 0, NP_ASSISTANT_TAP_CLOSE } np_assistant_tap_t;
typedef void (*np_assistant_action_cb_t)(np_assistant_tap_t, void *);

/* LVGL task only. Stable address until destroy: callbacks own no I/O.
 * Placement lives in RAM; dragging never writes flash or posts voice. */
typedef struct {
    lv_obj_t *root, *orb, *left_eye, *right_eye, *status, *code, *text, *close;
    lv_timer_t *eye_timer;
    xiaozhi_projection_t state;
    uint32_t rendered_revision, eye_epoch;
    np_assistant_action_cb_t action;
    void *user;
    lv_point_t press_point, press_origin;
    int32_t x, y;
    bool open, available, pressed, dragged;
} np_assistant_view_t;

bool np_assistant_build(np_assistant_view_t *, lv_obj_t *, np_assistant_action_cb_t, void *);
void np_assistant_destroy(np_assistant_view_t *);
void np_assistant_set_available(np_assistant_view_t *, bool);
void np_assistant_set_open(np_assistant_view_t *, bool);
bool np_assistant_owns_touch(const np_assistant_view_t *, const lv_obj_t *);
void np_assistant_sync(np_assistant_view_t *, const xiaozhi_projection_t *, bool online);
