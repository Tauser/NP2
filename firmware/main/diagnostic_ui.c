/*
 * Controlled Phase 2 diagnostic UI.
 *
 * Input events are emitted from the LVGL input pipeline. The callback therefore
 * runs in the LVGL task and may update only this diagnostic view; it performs
 * no network, filesystem, NVS, or flash operation. Display-event timings
 * describe LVGL rendering and flush-callback execution; they are not a DSI
 * scan-out-complete measurement.
 */
#include <stdio.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "diagnostic_ui.h"
#include "board_bringup.h"
#include "flash_coordinator.h"

#define DIAG_TARGET_SIZE 104
#define DIAG_TARGET_MARGIN 28
#define DIAG_METRIC_PERIOD_MS 1000
#define DIAG_STRESS_PERIOD_MS 50
#define DIAG_STRESS_BAR_WIDTH 260
#define DIAG_TOUCH_TARGET_COUNT 5
#define DIAG_TOUCH_REPETITIONS_REQUIRED 20

static const char *const s_target_names[DIAG_TOUCH_TARGET_COUNT] = {
    "SE", "SD", "IE", "ID", "CENTRO",
};

typedef struct {
    lv_obj_t *coordinate_label;
    lv_obj_t *state_label;
    lv_obj_t *memory_label;
    lv_obj_t *memory_campaign_label;
    lv_obj_t *render_label;
    lv_obj_t *stress_button;
    lv_obj_t *stress_button_label;
    lv_obj_t *stress_bar;
    lv_obj_t *stress_bar_label;
    lv_obj_t *flash_probe_button;
    lv_obj_t *flash_probe_button_label;
    lv_obj_t *flash_status_label;
    lv_obj_t *targets[DIAG_TOUCH_TARGET_COUNT];
    lv_obj_t *target_labels[DIAG_TOUCH_TARGET_COUNT];
    lv_obj_t *highlighted_target;
    uint32_t sample_count;
    uint8_t target_counts[DIAG_TOUCH_TARGET_COUNT];
    uint32_t render_started_at_ms;
    uint32_t flush_started_at_ms;
    uint32_t last_render_ms;
    uint32_t last_flush_callback_ms;
    uint32_t max_flush_callback_ms;
    uint32_t flushes_in_render;
    uint32_t last_flushes_per_refresh;
    uint32_t peak_flushes_under_stress;
    uint32_t peak_render_ms_under_stress;
    uint32_t peak_flush_callback_ms_under_stress;
    uint32_t last_flushes_under_stress;
    uint32_t completed_peak_render_ms;
    uint32_t completed_peak_flush_callback_ms;
    uint32_t completed_last_flushes;
    uint32_t completed_peak_flushes;
    size_t soak_start_internal_free;
    size_t soak_start_psram_free;
    size_t soak_min_internal_free;
    size_t soak_min_psram_free;
    uint16_t stress_phase;
    bool stress_active;
    bool stress_measurement_started;
    bool soak_baseline_captured;
    bool completed_campaign_available;
    bool maintenance_arming;
    bool maintenance_armed;
    bool maintenance_requested;
    bool maintenance_restore_armed;
} diagnostic_ui_state_t;

static diagnostic_ui_state_t s_state;

static void maintenance_arm_timer_cb(lv_timer_t *timer);
static void maintenance_restore_timer_cb(lv_timer_t *timer);

static void update_target_label(size_t index)
{
    lv_label_set_text_fmt(s_state.target_labels[index], "%s\n%u/%u", s_target_names[index],
                          (unsigned int)s_state.target_counts[index],
                          DIAG_TOUCH_REPETITIONS_REQUIRED);
    lv_obj_center(s_state.target_labels[index]);
}

static bool all_targets_complete(void)
{
    for (size_t i = 0; i < DIAG_TOUCH_TARGET_COUNT; ++i) {
        if (s_state.target_counts[i] < DIAG_TOUCH_REPETITIONS_REQUIRED) {
            return false;
        }
    }
    return true;
}

static void record_target_press(lv_obj_t *active_target)
{
    for (size_t i = 0; i < DIAG_TOUCH_TARGET_COUNT; ++i) {
        if (s_state.targets[i] != active_target) {
            continue;
        }

        if (s_state.target_counts[i] < DIAG_TOUCH_REPETITIONS_REQUIRED) {
            s_state.target_counts[i]++;
            update_target_label(i);
        }
        return;
    }
}

static void capture_soak_baseline(void)
{
    s_state.soak_start_internal_free =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_state.soak_start_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s_state.soak_min_internal_free = s_state.soak_start_internal_free;
    s_state.soak_min_psram_free = s_state.soak_start_psram_free;
    s_state.soak_baseline_captured = true;
}

static void update_flash_status_label(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);

    if (status.maintenance_recovery_required) {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "NVS manutencao #%lu: %s; recuperando tela",
                              (unsigned long)status.last_sequence,
                              esp_err_to_name(status.last_result));
        if (!s_state.maintenance_restore_armed) {
            s_state.maintenance_restore_armed = true;
            lv_timer_create(maintenance_restore_timer_cb, 300, NULL);
        }
    } else if (!status.ready) {
        lv_label_set_text_fmt(s_state.flash_status_label, "NVS diagnostico: indisponivel (%s)",
                              esp_err_to_name(status.init_result));
    } else if (status.busy || status.pending) {
        lv_label_set_text(s_state.flash_status_label, "NVS diagnostico: solicitacao em andamento");
    } else if (status.completed_count > 0U) {
        if (status.last_batch_writes > 0U) {
            lv_label_set_text_fmt(s_state.flash_status_label,
                                  "NVS manutencao #%lu: %s %lu writes %lums livre %lu>%lu",
                                  (unsigned long)status.last_sequence,
                                  esp_err_to_name(status.last_result),
                                  (unsigned long)status.last_batch_writes,
                                  (unsigned long)status.last_duration_ms,
                                  (unsigned long)status.last_free_entries_before,
                                  (unsigned long)status.last_free_entries_after);
        } else {
            lv_label_set_text_fmt(s_state.flash_status_label,
                                  "NVS diagnostico #%lu: %s em %lums",
                                  (unsigned long)status.last_sequence,
                                  esp_err_to_name(status.last_result),
                                  (unsigned long)status.last_duration_ms);
        }
    } else {
        lv_label_set_text(s_state.flash_status_label,
                          "NVS diagnostico pronto: uma solicitacao por minuto");
    }
}

static void update_telemetry(bool update_view)
{
    const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t internal_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    const size_t lvgl_stack_free_bytes =
        uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);

    if (s_state.soak_baseline_captured) {
        if (internal_free < s_state.soak_min_internal_free) {
            s_state.soak_min_internal_free = internal_free;
        }
        if (psram_free < s_state.soak_min_psram_free) {
            s_state.soak_min_psram_free = psram_free;
        }
    }

    if (!update_view) {
        return;
    }

    lv_label_set_text_fmt(s_state.memory_label,
                          "SRAM livre=%uK maior=%uK | PSRAM livre=%uK maior=%uK",
                          (unsigned int)(internal_free / 1024U),
                          (unsigned int)(internal_largest / 1024U),
                          (unsigned int)(psram_free / 1024U),
                          (unsigned int)(psram_largest / 1024U));
    if (s_state.soak_baseline_captured) {
        const long internal_delta_kib =
            ((long)internal_free - (long)s_state.soak_start_internal_free) / 1024L;
        const long psram_delta_kib =
            ((long)psram_free - (long)s_state.soak_start_psram_free) / 1024L;
        lv_label_set_text_fmt(s_state.memory_campaign_label,
                              "soak: SRAM %+ldK min=%uK | PSRAM %+ldK min=%uK | pilha LVGL=%uB",
                              internal_delta_kib,
                              (unsigned int)(s_state.soak_min_internal_free / 1024U),
                              psram_delta_kib,
                              (unsigned int)(s_state.soak_min_psram_free / 1024U),
                              (unsigned int)lvgl_stack_free_bytes);
    } else {
        lv_label_set_text_fmt(s_state.memory_campaign_label,
                              "soak: ative carga para capturar base | pilha LVGL=%uB",
                              (unsigned int)lvgl_stack_free_bytes);
    }
    update_flash_status_label();
    if (s_state.completed_campaign_available && !s_state.stress_active) {
        lv_label_set_text_fmt(s_state.render_label,
                              "campanha: render max=%lums | flush_cb max=%lums | ciclo=%lu | pico carga=%lu",
                              (unsigned long)s_state.completed_peak_render_ms,
                              (unsigned long)s_state.completed_peak_flush_callback_ms,
                              (unsigned long)s_state.completed_last_flushes,
                              (unsigned long)s_state.completed_peak_flushes);
    } else {
        lv_label_set_text_fmt(s_state.render_label,
                              "render=%lums | flush_cb=%lums (max=%lums) | ciclo=%lu | pico carga=%lu",
                              (unsigned long)s_state.last_render_ms,
                              (unsigned long)s_state.last_flush_callback_ms,
                              (unsigned long)s_state.max_flush_callback_ms,
                              (unsigned long)s_state.last_flushes_per_refresh,
                              (unsigned long)s_state.peak_flushes_under_stress);
    }
}

static void maintenance_arm_timer_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    s_state.maintenance_arming = false;
    const esp_err_t request_err = flash_coordinator_request_nvs_compaction_probe();
    if (request_err == ESP_OK) {
        lv_label_set_text(s_state.flash_status_label,
                          "NVS manutencao: backlight sera desligado para GC");
    } else {
        s_state.maintenance_requested = false;
        s_state.maintenance_armed = false;
        lv_obj_remove_state(s_state.stress_button, LV_STATE_DISABLED);
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "NVS manutencao recusada: %s", esp_err_to_name(request_err));
    }
}

static void maintenance_restore_timer_cb(lv_timer_t *timer)
{
    lv_timer_delete(timer);
    const esp_err_t restore_err = board_bringup_set_backlight_percent(60);
    if (restore_err == ESP_OK) {
        flash_coordinator_complete_maintenance_visual_recovery();
        s_state.maintenance_restore_armed = false;
        s_state.maintenance_armed = false;
        s_state.maintenance_requested = false;
        lv_obj_remove_state(s_state.stress_button, LV_STATE_DISABLED);
        lv_label_set_text(s_state.flash_probe_button_label, "NVS EM CARGA / MANUT.");
        update_flash_status_label();
    } else {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "NVS manutencao: falha ao restaurar luz: %s",
                              esp_err_to_name(restore_err));
    }
}

static void flash_probe_button_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_CLICKED || s_state.maintenance_arming || s_state.maintenance_requested) {
        return;
    }

    if (!s_state.stress_active && !s_state.maintenance_armed) {
        s_state.maintenance_armed = true;
        lv_label_set_text(s_state.flash_probe_button_label, "MANUT. ARMADA — TOQUE DE NOVO");
        lv_label_set_text(s_state.flash_status_label,
                          "NVS manutencao armada: proximo toque apaga a tela");
        return;
    }

    if (!s_state.stress_active) {
        s_state.maintenance_armed = false;
        s_state.maintenance_arming = true;
        s_state.maintenance_requested = true;
        lv_obj_add_state(s_state.stress_button, LV_STATE_DISABLED);
        lv_label_set_text(s_state.flash_probe_button_label, "MANUTENCAO: PREPARANDO");
        lv_label_set_text(s_state.flash_status_label,
                          "NVS manutencao: frame pronto; apagando antes do GC");
        lv_timer_create(maintenance_arm_timer_cb, 750, NULL);
        return;
    }

    const esp_err_t request_err = flash_coordinator_request_nvs_probe();
    if (request_err == ESP_OK) {
        lv_label_set_text(s_state.flash_probe_button_label, "NVS: SOLICITADO");
        lv_label_set_text(s_state.flash_status_label,
                          "NVS diagnostico: aguardando worker de flash");
    } else {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "NVS diagnostico recusado: %s", esp_err_to_name(request_err));
    }
}

static void telemetry_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_telemetry(!s_state.stress_active);
}

static void display_event_cb(lv_event_t *event)
{
    const uint32_t now_ms = lv_tick_get();

    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        s_state.flushes_in_render = 0;
        break;
    case LV_EVENT_REFR_READY:
        s_state.last_flushes_per_refresh = s_state.flushes_in_render;
        if (s_state.stress_measurement_started) {
            s_state.last_flushes_under_stress = s_state.flushes_in_render;
            if (s_state.flushes_in_render > s_state.peak_flushes_under_stress) {
                s_state.peak_flushes_under_stress = s_state.flushes_in_render;
            }
        }
        break;
    case LV_EVENT_RENDER_START:
        s_state.render_started_at_ms = now_ms;
        break;
    case LV_EVENT_RENDER_READY:
        s_state.last_render_ms = lv_tick_elaps(s_state.render_started_at_ms);
        if (s_state.stress_measurement_started &&
            s_state.last_render_ms > s_state.peak_render_ms_under_stress) {
            s_state.peak_render_ms_under_stress = s_state.last_render_ms;
        }
        break;
    case LV_EVENT_FLUSH_START:
        s_state.flush_started_at_ms = now_ms;
        s_state.flushes_in_render++;
        break;
    case LV_EVENT_FLUSH_FINISH:
        s_state.last_flush_callback_ms = lv_tick_elaps(s_state.flush_started_at_ms);
        if (s_state.last_flush_callback_ms > s_state.max_flush_callback_ms) {
            s_state.max_flush_callback_ms = s_state.last_flush_callback_ms;
        }
        if (s_state.stress_measurement_started &&
            s_state.last_flush_callback_ms > s_state.peak_flush_callback_ms_under_stress) {
            s_state.peak_flush_callback_ms_under_stress = s_state.last_flush_callback_ms;
        }
        break;
    default:
        break;
    }
}

static void set_target_state(lv_obj_t *target, bool active)
{
    if (target == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(target, active ? lv_color_hex(0x126A50) : lv_color_hex(0x183554),
                               LV_PART_MAIN);
    lv_obj_set_style_border_color(target, active ? lv_color_hex(0x68E0B8) : lv_color_hex(0x6491C6),
                                   LV_PART_MAIN);
}

static bool is_diagnostic_target(lv_obj_t *candidate)
{
    for (size_t i = 0; i < DIAG_TOUCH_TARGET_COUNT; ++i) {
        if (s_state.targets[i] == candidate) {
            return true;
        }
    }
    return false;
}

static void reset_targets_except(lv_obj_t *active_target)
{
    if (!is_diagnostic_target(active_target)) {
        active_target = NULL;
    }
    if (s_state.highlighted_target == active_target) {
        return;
    }

    if (s_state.highlighted_target != NULL) {
        set_target_state(s_state.highlighted_target, false);
    }
    if (active_target != NULL) {
        set_target_state(active_target, true);
    }
    s_state.highlighted_target = active_target;
}

static void touch_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *const indev = lv_event_get_target(event);
    lv_point_t point = {0};
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        s_state.sample_count++;

        if (code == LV_EVENT_PRESSED) {
            if (!s_state.stress_active) {
                lv_label_set_text_fmt(s_state.coordinate_label, "x=%d  y=%d  amostras=%lu",
                                      (int)point.x, (int)point.y,
                                      (unsigned long)s_state.sample_count);
            }
            lv_obj_t *const active_target = lv_event_get_param(event);
            reset_targets_except(active_target);
            record_target_press(active_target);
            if (all_targets_complete() && !s_state.stress_active) {
                lv_label_set_text(s_state.state_label,
                                  "CAMPANHA CONCLUIDA — 20 toques por alvo");
            }
        }
    }
}

static void stress_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_state.stress_active) {
        s_state.completed_campaign_available = s_state.stress_measurement_started;
        s_state.completed_peak_render_ms = s_state.peak_render_ms_under_stress;
        s_state.completed_peak_flush_callback_ms = s_state.peak_flush_callback_ms_under_stress;
        s_state.completed_last_flushes = s_state.last_flushes_under_stress;
        s_state.completed_peak_flushes = s_state.peak_flushes_under_stress;
        s_state.stress_active = false;
        s_state.stress_measurement_started = false;
    } else {
        s_state.stress_active = true;
        s_state.stress_measurement_started = false;
        s_state.peak_flushes_under_stress = 0;
        s_state.peak_render_ms_under_stress = 0;
        s_state.peak_flush_callback_ms_under_stress = 0;
        s_state.last_flushes_under_stress = 0;
        s_state.completed_campaign_available = false;
        capture_soak_baseline();
    }
    lv_label_set_text(s_state.stress_button_label,
                      s_state.stress_active ? "CARGA DE RENDER: ATIVA" : "CARGA DE RENDER: PAUSADA");
    lv_obj_set_style_bg_color(s_state.stress_button,
                              s_state.stress_active ? lv_color_hex(0x7A3E10) : lv_color_hex(0x183554),
                              LV_PART_MAIN);
    lv_label_set_text(s_state.state_label,
                      s_state.stress_active ? "CARGA ATIVA — observe tearing, glitches e fluidez"
                                            : "CARGA PAUSADA — toque para retomar");
    if (!s_state.stress_active) {
        lv_label_set_text(s_state.stress_bar_label, "CARGA PAUSADA");
        update_telemetry(true);
    }
}

static void stress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_state.stress_active) {
        return;
    }

    /* Start after the button's own invalidation has completed. */
    if (!s_state.stress_measurement_started) {
        s_state.peak_flushes_under_stress = 0;
        s_state.stress_measurement_started = true;
    }

    s_state.stress_phase = (uint16_t)((s_state.stress_phase + 9U) % 401U);
    const int32_t x = 382 + (int32_t)s_state.stress_phase - 200;
    const uint32_t hue = (uint32_t)((s_state.stress_phase * 3U) & 0xffU);

    lv_obj_set_x(s_state.stress_bar, x);
    lv_obj_set_style_bg_color(s_state.stress_bar, lv_color_hsv_to_rgb(hue, 75, 85), LV_PART_MAIN);
    lv_label_set_text_fmt(s_state.stress_bar_label, "CARGA %03u", (unsigned int)s_state.stress_phase);
}

static lv_obj_t *create_target(lv_obj_t *parent, size_t index, lv_align_t align, int32_t x_offset,
                               int32_t y_offset)
{
    lv_obj_t *const target = lv_button_create(parent);
    lv_obj_set_size(target, DIAG_TARGET_SIZE, DIAG_TARGET_SIZE);
    lv_obj_align(target, align, x_offset, y_offset);
    lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(target, 3, LV_PART_MAIN);
    set_target_state(target, false);

    lv_obj_t *const label = lv_label_create(target);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    s_state.target_labels[index] = label;
    update_target_label(index);
    lv_obj_center(label);
    return target;
}

esp_err_t diagnostic_ui_create(lv_display_t *display, lv_indev_t *touch_indev)
{
    ESP_RETURN_ON_FALSE(display != NULL && touch_indev != NULL, ESP_ERR_INVALID_ARG,
                        "diag_ui", "Display or touch handle missing");

    s_state = (diagnostic_ui_state_t){0};

    lv_obj_t *const screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x09111F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *const title = lv_label_create(screen);
    lv_label_set_text(title, "NP2  |  DIAGNOSTICO DE TOUCH E RENDER");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *const instruction = lv_label_create(screen);
    lv_label_set_text(instruction, "Toque os alvos; ative carga somente depois de confirmar a orientacao");
    lv_obj_set_style_text_color(instruction, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(instruction, LV_ALIGN_TOP_MID, 0, 42);

    s_state.coordinate_label = lv_label_create(screen);
    lv_label_set_text(s_state.coordinate_label, "x=--  y=--  amostras=0");
    lv_obj_set_style_text_color(s_state.coordinate_label, lv_color_hex(0x68E0B8), LV_PART_MAIN);
    lv_obj_align(s_state.coordinate_label, LV_ALIGN_TOP_MID, 0, 68);

    s_state.memory_label = lv_label_create(screen);
    lv_label_set_text(s_state.memory_label, "SRAM/PSRAM: aguardando primeira amostra");
    lv_obj_set_style_text_color(s_state.memory_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.memory_label, LV_ALIGN_TOP_MID, 0, 92);

    s_state.memory_campaign_label = lv_label_create(screen);
    lv_label_set_text(s_state.memory_campaign_label, "soak: aguardando primeira amostra");
    lv_obj_set_style_text_color(s_state.memory_campaign_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.memory_campaign_label, LV_ALIGN_TOP_MID, 0, 114);

    s_state.render_label = lv_label_create(screen);
    lv_label_set_text(s_state.render_label, "render=-- | flush_cb=-- | flushes/render max=--");
    lv_obj_set_style_text_color(s_state.render_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.render_label, LV_ALIGN_TOP_MID, 0, 136);

    s_state.stress_button = lv_button_create(screen);
    lv_obj_set_size(s_state.stress_button, 240, 34);
    lv_obj_align(s_state.stress_button, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_radius(s_state.stress_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.stress_button, lv_color_hex(0x183554), LV_PART_MAIN);
    s_state.stress_button_label = lv_label_create(s_state.stress_button);
    lv_label_set_text(s_state.stress_button_label, "CARGA DE RENDER: PAUSADA");
    lv_obj_set_style_text_color(s_state.stress_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.stress_button_label);
    lv_obj_add_event_cb(s_state.stress_button, stress_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.stress_bar = lv_obj_create(screen);
    lv_obj_set_size(s_state.stress_bar, DIAG_STRESS_BAR_WIDTH, 22);
    lv_obj_set_pos(s_state.stress_bar, 182, 208);
    lv_obj_set_style_radius(s_state.stress_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.stress_bar, lv_color_hex(0x126A50), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_state.stress_bar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_state.stress_bar, LV_OBJ_FLAG_SCROLLABLE);
    s_state.stress_bar_label = lv_label_create(s_state.stress_bar);
    lv_label_set_text(s_state.stress_bar_label, "CARGA PAUSADA");
    lv_obj_set_style_text_color(s_state.stress_bar_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.stress_bar_label);

    s_state.flash_probe_button = lv_button_create(screen);
    lv_obj_set_size(s_state.flash_probe_button, 248, 30);
    lv_obj_align(s_state.flash_probe_button, LV_ALIGN_TOP_MID, 0, 376);
    lv_obj_set_style_radius(s_state.flash_probe_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.flash_probe_button, lv_color_hex(0x5A3A12), LV_PART_MAIN);
    s_state.flash_probe_button_label = lv_label_create(s_state.flash_probe_button);
    lv_label_set_text(s_state.flash_probe_button_label, "NVS EM CARGA / MANUT.");
    lv_obj_set_style_text_color(s_state.flash_probe_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.flash_probe_button_label);
    lv_obj_add_event_cb(s_state.flash_probe_button, flash_probe_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.flash_status_label = lv_label_create(screen);
    lv_label_set_text(s_state.flash_status_label, "NVS diagnostico: inicializando");
    lv_obj_set_style_text_color(s_state.flash_status_label, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_align(s_state.flash_status_label, LV_ALIGN_TOP_MID, 0, 412);

    s_state.state_label = lv_label_create(screen);
    lv_label_set_text(s_state.state_label, "AGUARDANDO TOQUE");
    lv_obj_set_style_text_color(s_state.state_label, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_align(s_state.state_label, LV_ALIGN_BOTTOM_MID, 0, -22);

    s_state.targets[0] = create_target(screen, 0, LV_ALIGN_TOP_LEFT, DIAG_TARGET_MARGIN, 130);
    s_state.targets[1] = create_target(screen, 1, LV_ALIGN_TOP_RIGHT, -DIAG_TARGET_MARGIN, 130);
    s_state.targets[2] = create_target(screen, 2, LV_ALIGN_BOTTOM_LEFT, DIAG_TARGET_MARGIN, -78);
    s_state.targets[3] = create_target(screen, 3, LV_ALIGN_BOTTOM_RIGHT, -DIAG_TARGET_MARGIN, -78);
    s_state.targets[4] = create_target(screen, 4, LV_ALIGN_CENTER, 0, 0);

    lv_indev_add_event_cb(touch_indev, touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_indev_add_event_cb(touch_indev, touch_event_cb, LV_EVENT_PRESSING, NULL);

    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_REFR_READY, NULL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_RENDER_READY, NULL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_FLUSH_START, NULL);
    lv_display_add_event_cb(display, display_event_cb, LV_EVENT_FLUSH_FINISH, NULL);
    lv_timer_create(telemetry_timer_cb, DIAG_METRIC_PERIOD_MS, NULL);
    lv_timer_create(stress_timer_cb, DIAG_STRESS_PERIOD_MS, NULL);

    return ESP_OK;
}
