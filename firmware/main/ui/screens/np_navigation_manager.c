#include "np_navigation_manager.h"

#include <inttypes.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *const TAG = "np_navigation";

static void hold_frame(np_navigation_manager_t *manager)
{
    lv_display_t *display = lv_obj_get_display(manager->screen);
    lv_timer_t *refresh = lv_display_get_refr_timer(display);
    if (refresh == NULL) return;
    /* Keep the last physical frame while the outgoing tree is hidden and
     * cleaned. Invalidation alone can resume the LVGL refresh timer. */
    lv_display_enable_invalidation(display, false);
    lv_timer_pause(refresh);
    manager->frame_held = true;
}

static void release_frame(np_navigation_manager_t *manager)
{
    if (!manager->frame_held) return;
    lv_display_t *display = lv_obj_get_display(manager->screen);
    lv_timer_t *refresh = lv_display_get_refr_timer(display);
    lv_display_enable_invalidation(display, true);
    lv_obj_invalidate(manager->screen);
    if (refresh != NULL) {
        lv_timer_resume(refresh);
        lv_timer_ready(refresh);
    }
    manager->frame_held = false;
}

static uint32_t object_count(lv_obj_t *root)
{
    if (root == NULL) return 0;
    uint32_t count = 1;
    const uint32_t children = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < children; ++i) {
        count += object_count(lv_obj_get_child(root, i));
    }
    return count;
}

static uint32_t timer_count(void)
{
    uint32_t count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer != NULL;
         timer = lv_timer_get_next(timer)) ++count;
    return count;
}

static void trace(np_navigation_manager_t *manager, const char *phase,
                  int64_t started_us)
{
    lv_mem_monitor_t lv_heap = {0};
    lv_mem_monitor(&lv_heap);
    ESP_LOGI(TAG, "%s state=%u from=%" PRIuPTR " to=%" PRIuPTR
             " handler_generation=%" PRIu32 " page_generation=%" PRIu32
             " internal_free=%u internal_largest=%u lv_free=%u lv_largest=%u"
             " objects=%" PRIu32 " timers=%" PRIu32 " elapsed_us=%" PRId64
             " transition_us=%" PRId64,
             phase, (unsigned)manager->phase, manager->current_page, manager->destination,
             manager->handler_generation, manager->page_generation,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)lv_heap.free_size, (unsigned)lv_heap.free_biggest_size,
             object_count(manager->screen), timer_count(),
             esp_timer_get_time() - started_us,
             manager->requested_us == 0 ? 0 : esp_timer_get_time() - manager->requested_us);
}

void np_navigation_init(np_navigation_manager_t *manager, lv_obj_t *screen,
                        uintptr_t initial_page, const np_navigation_ops_t *ops,
                        void *context)
{
    *manager = (np_navigation_manager_t){
        .ops = *ops,
        .context = context,
        .screen = screen,
        .current_page = initial_page,
        .destination = initial_page,
        .phase = NP_NAV_IDLE,
    };
}

bool np_navigation_request(np_navigation_manager_t *manager, uintptr_t destination)
{
    if (manager->phase != NP_NAV_IDLE || destination == manager->current_page) return false;
    manager->destination = destination;
    manager->requested_us = esp_timer_get_time();
    manager->phase = NP_NAV_LEAVE;
    trace(manager, "REQUEST", manager->requested_us);
    return true;
}

void np_navigation_cycle_begin(void *context)
{
    np_navigation_manager_t *manager = context;
    ++manager->handler_generation;
    if (manager->phase == NP_NAV_LEAVE) {
        const int64_t started = esp_timer_get_time();
        hold_frame(manager);
        manager->ops.leave(manager->context, manager->current_page);
        manager->phase = NP_NAV_CLEAN;
        trace(manager, "LEAVE", started);
    } else if (manager->phase == NP_NAV_WAIT_NEXT_PASS &&
               manager->handler_generation != manager->cleaned_handler_generation) {
        const int64_t started = esp_timer_get_time();
        manager->phase = NP_NAV_BUILD;
        const bool built = manager->ops.build(manager->context, manager->destination);
        trace(manager, built ? "BUILD" : "BUILD_FAILED", started);
        if (built) {
            const int64_t entered = esp_timer_get_time();
            manager->phase = NP_NAV_ENTER;
            manager->ops.enter(manager->context, manager->destination);
            manager->current_page = manager->destination;
            ++manager->page_generation;
            release_frame(manager);
            trace(manager, "ENTER", entered);
        } else {
            if (manager->ops.failed != NULL)
                manager->ops.failed(manager->context, manager->destination);
            release_frame(manager);
        }
        manager->phase = NP_NAV_IDLE;
    }
}

void np_navigation_cycle_end(void *context)
{
    np_navigation_manager_t *manager = context;
    if (manager->phase != NP_NAV_CLEAN) return;
    const int64_t started = esp_timer_get_time();
    manager->ops.clean(manager->context, manager->current_page);
    manager->cleaned_handler_generation = manager->handler_generation;
    manager->phase = NP_NAV_WAIT_NEXT_PASS;
    trace(manager, "CLEAN_WAIT_NEXT_PASS", started);
}
