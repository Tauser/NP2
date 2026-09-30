#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

typedef enum {
    NP_NAV_IDLE = 0,
    NP_NAV_LEAVE,
    NP_NAV_CLEAN,
    NP_NAV_WAIT_NEXT_PASS,
    NP_NAV_BUILD,
    NP_NAV_ENTER,
} np_nav_phase_t;

typedef struct {
    void (*leave)(void *context, uintptr_t page);
    void (*clean)(void *context, uintptr_t page);
    bool (*build)(void *context, uintptr_t page);
    void (*enter)(void *context, uintptr_t page);
} np_navigation_ops_t;

typedef struct {
    np_navigation_ops_t ops;
    void *context;
    lv_obj_t *screen;
    uintptr_t current_page;
    uintptr_t destination;
    uint32_t handler_generation;
    uint32_t page_generation;
    uint32_t cleaned_handler_generation;
    int64_t requested_us;
    np_nav_phase_t phase;
} np_navigation_manager_t;

void np_navigation_init(np_navigation_manager_t *manager, lv_obj_t *screen,
                        uintptr_t initial_page, const np_navigation_ops_t *ops,
                        void *context);
bool np_navigation_request(np_navigation_manager_t *manager, uintptr_t destination);
void np_navigation_cycle_begin(void *context);
void np_navigation_cycle_end(void *context);
