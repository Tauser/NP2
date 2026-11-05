#pragma once

#include <stdbool.h>
#include "np_components.h"

/* Third page of the Home widget slot: the live picture of the first saved camera and nothing else.
 * The video session runs only while the page is shown (np_home_camera_sync with visible=true). */
typedef struct {
    lv_obj_t *root;
    lv_obj_t *image;
    lv_obj_t *status;
    uint32_t frame_sequence;
} np_home_camera_view_t;

np_home_camera_view_t np_home_camera_build(lv_obj_t *parent);

/* Starts the session when the page becomes visible, stops it when it stops being, and shows the
 * newest frame while it plays. camera_ipv4 is the first saved camera, or an empty string. */
void np_home_camera_sync(np_home_camera_view_t *view, bool visible, const char *camera_ipv4);

/* Ends the session if this module started one. Safe on a view that was already destroyed. */
void np_home_camera_stop(void);

/* Shows the newest frame if there is one. Cheap when nothing new arrived or no session runs. */
void np_home_camera_present(np_home_camera_view_t *view);
