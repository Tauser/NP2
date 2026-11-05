#pragma once
#include "np_spotify.h"
/* A card owns its horizontal drag even at an end: never navigate the page.
 * Controls and modal content retain exclusive ownership from pointer-down. */
/* -1: control/modal; 0: outside; 1: card surface. Resolve at pointer-down. */
static inline int np_home_widget_owner(lv_obj_t *object, lv_obj_t *weather,
                                     const np_spotify_view_t *spotify)
{
    bool control = false;
    for (lv_obj_t *a = object; a; a = lv_obj_get_parent(a)) {
        if (a == spotify->device_modal.scrim) return -1;
        if (a == spotify->root || a == weather) return control ? -1 : 1;
        if (lv_obj_check_type(a, &lv_slider_class) || a == spotify->play ||
            a == spotify->previous || a == spotify->next || a == spotify->devices) control = true;
    }
    return 0;
}
static inline int np_home_widget_destination(int32_t dx, int32_t dy)
{
    int32_t x = dx < 0 ? -dx : dx, y = dy < 0 ? -dy : dy;
    if (x < 72 || x < y * 2) return -1;
    return dx < 0 ? 1 : 0;
}
#define NP_HOME_WIDGET_COUNT 3
/* The page a horizontal drag leads to from `current`, clamped to the first and last page: a drag
 * at an end returns `current`, so the card still owns it and the main page does not change. */
static inline int np_home_widget_step(int current, int32_t dx, int32_t dy)
{
    if (np_home_widget_destination(dx, dy) < 0) return -1;
    int next = dx < 0 ? current + 1 : current - 1;
    if (next < 0) next = 0;
    if (next >= NP_HOME_WIDGET_COUNT) next = NP_HOME_WIDGET_COUNT - 1;
    return next;
}
