/*
 * First local hardware bring-up for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
 *
 * The order in this file follows docs/RESTART-HARDWARE-BRINGUP.md. It is not a
 * product UI or a network bootstrap; it is the smallest observable baseline
 * from which timing, touch orientation and memory behavior can be measured.
 */
#include <stdbool.h>

#include "bsp/display.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "bsp/touch.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "lvgl.h"

#include "board_bringup.h"
#include "ui/screens/product_ui.h"

static const char *const TAG = "np2_bringup";
static portMUX_TYPE s_health_lock = portMUX_INITIALIZER_UNLOCKED;
static board_bringup_health_t s_health;
static void *s_lvgl_psram_pool;

#define NP2_EXPECTED_PSRAM_BYTES (32U * 1024U * 1024U)
#define NP2_BOOT_BACKLIGHT_PERCENT 60
#define NP2_LVGL_TASK_STACK_BYTES (16U * 1024U)
#define NP2_LVGL_TASK_PRIORITY 8U
#define NP2_LVGL_PSRAM_POOL_BYTES (128U * 1024U)

static esp_err_t extend_lvgl_object_pool(void)
{
    if (s_lvgl_psram_pool != NULL) return ESP_OK;

    void *const pool = heap_caps_malloc(NP2_LVGL_PSRAM_POOL_BYTES,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pool == NULL) {
        ESP_LOGE(TAG, "LVGL PSRAM pool allocation failed (%u bytes)",
                 (unsigned)NP2_LVGL_PSRAM_POOL_BYTES);
        return ESP_ERR_NO_MEM;
    }
    if (lv_mem_add_pool(pool, NP2_LVGL_PSRAM_POOL_BYTES) == NULL) {
        heap_caps_free(pool);
        ESP_LOGE(TAG, "LVGL could not register PSRAM pool");
        return ESP_ERR_NO_MEM;
    }

    s_lvgl_psram_pool = pool;
    lv_mem_monitor_t memory = {0};
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "LVGL object pools total=%u free=%u largest=%u PSRAM_added=%u",
             (unsigned)memory.total_size, (unsigned)memory.free_size,
             (unsigned)memory.free_biggest_size,
             (unsigned)NP2_LVGL_PSRAM_POOL_BYTES);
    return ESP_OK;
}

static esp_err_t verify_psram(void)
{
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM was not initialized by startup configuration");
        return ESP_ERR_INVALID_STATE;
    }

    const size_t psram_bytes = esp_psram_get_size();
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI(TAG, "PSRAM=%u bytes, free=%u; internal free=%u",
             (unsigned int)psram_bytes, (unsigned int)psram_free,
             (unsigned int)internal_free);

    if (psram_bytes != NP2_EXPECTED_PSRAM_BYTES) {
        ESP_LOGE(TAG, "Unexpected PSRAM size: got=%u expected=%u",
                 (unsigned int)psram_bytes, NP2_EXPECTED_PSRAM_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static void display_render_ready_cb(lv_event_t *event)
{
    (void)event;
    portENTER_CRITICAL(&s_health_lock);
    s_health.first_frame_presented = true;
    s_health.last_ui_progress_ms = (uint64_t)esp_timer_get_time() / 1000U;
    portEXIT_CRITICAL(&s_health_lock);
}

/* A static screen is healthy too. This timer runs on the LVGL owner and
 * observes its progress without forcing redraws or claiming a new frame. */
static void ui_health_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    portENTER_CRITICAL(&s_health_lock);
    s_health.last_ui_progress_ms = (uint64_t)esp_timer_get_time() / 1000U;
    portEXIT_CRITICAL(&s_health_lock);
}

esp_err_t board_bringup_start(void)
{
    esp_err_t err = verify_psram();
    if (err != ESP_OK) {
        return err;
    }

    const esp_lv_adapter_config_t adapter_cfg = {
        .task_stack_size = NP2_LVGL_TASK_STACK_BYTES,
        .task_priority = NP2_LVGL_TASK_PRIORITY,
        .task_core_id = -1,
        .ui_cycle_begin = product_ui_cycle_begin,
        .ui_cycle_end = product_ui_cycle_end,
        .tick_period_ms = 1,
        /* A 100 Hz FreeRTOS tick is 10 ms. A 1 ms wait becomes zero ticks
         * in the adapter worker and can spin the LVGL task without yielding. */
        .task_min_delay_ms = portTICK_PERIOD_MS,
        .task_max_delay_ms = 2 * portTICK_PERIOD_MS,
        .stack_in_psram = false,
        .auto_sleep = {
            .enable = false,
            .mode = ESP_LV_ADAPTER_AUTO_SLEEP_MODE_DISABLED,
        },
    };

    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_cfg), TAG, "LVGL adapter init failed");

    bsp_lcd_handles_t lcd = {0};
    ESP_RETURN_ON_ERROR(bsp_display_new_with_handles(NULL, &lcd), TAG, "DSI panel init failed");
    ESP_RETURN_ON_ERROR(bsp_display_backlight_off(), TAG, "Backlight off failed");
    /* EK79007's BSP panel has no disp_on_off implementation; init is sufficient. */

    const esp_lv_adapter_display_config_t display_cfg = {
        .panel = lcd.panel,
        .panel_io = lcd.io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,
            .rotation = ESP_LV_ADAPTER_ROTATE_180,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = false,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
            .mono_layout = ESP_LV_ADAPTER_MONO_LAYOUT_NONE,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .te_sync = ESP_LV_ADAPTER_TE_SYNC_DISABLED(),
    };
    lv_display_t *const display = esp_lv_adapter_register_display(&display_cfg);
    if (display == NULL) {
        ESP_LOGE(TAG, "LVGL display registration failed");
        return ESP_FAIL;
    }

    /*
     * The adapter maps pointer coordinates for its rotated display. Applying
     * the GT911 180-degree mirror here as well rotates input twice: a physical
     * top-left touch then reaches the bottom-right widget. Keep the controller
     * in its native orientation and let the display adapter own this mapping.
     */
    const bsp_touch_config_t touch_transform = {
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_touch_handle_t touch = NULL;
    ESP_RETURN_ON_ERROR(bsp_touch_new(&touch_transform, &touch), TAG, "GT911 init failed");

    const esp_lv_adapter_touch_config_t touch_cfg =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
    lv_indev_t *const touch_indev = esp_lv_adapter_register_touch(&touch_cfg);
    if (touch_indev == NULL) {
        ESP_LOGE(TAG, "LVGL touch registration failed");
        return ESP_FAIL;
    }

    /*
     * Build the initial object tree while the adapter mutex is idle. Starting
     * the worker first can race the first touch poll and leave main waiting
     * indefinitely for the mutex before it has installed any screen.
     */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_lock(-1), TAG, "LVGL lock failed");
    err = extend_lvgl_object_pool();
    if (err == ESP_OK) err = product_ui_create(display, touch_indev);
    if (err == ESP_OK) {
        lv_display_add_event_cb(display, display_render_ready_cb, LV_EVENT_RENDER_READY, NULL);
        if (lv_timer_create(ui_health_timer_cb, 250U, NULL) == NULL) {
            err = ESP_ERR_NO_MEM;
        }
    }
    esp_lv_adapter_unlock();
    if (err != ESP_OK) {
        return err;
    }

    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "LVGL task start failed");

    /*
     * The first frame is flushed through the normal LVGL worker. Forcing it
     * synchronously can deadlock the EK79007 triple-partial pipeline before
     * its frame-complete path is active.
     */
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(NP2_BOOT_BACKLIGHT_PERCENT), TAG,
                        "Backlight enable failed");

    portENTER_CRITICAL(&s_health_lock);
    s_health.display_ready = true;
    portEXIT_CRITICAL(&s_health_lock);

    ESP_LOGI(TAG, "Direct LVGL UI active: RGB565, rotation=180, triple-partial, 3 FBs");
    return ESP_OK;
}

void board_bringup_get_health(board_bringup_health_t *out_health)
{
    if (out_health == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_health_lock);
    *out_health = s_health;
    portEXIT_CRITICAL(&s_health_lock);
}
