#pragma once

#include "np_components.h"
#include "system_resource_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *firmware;
    lv_obj_t *temperature;
    lv_obj_t *restart_button;
    lv_obj_t *status;
    lv_obj_t *uptime;
    lv_obj_t *cpu_value;
    lv_obj_t *core_value;
    lv_obj_t *cpu_arc;
    lv_obj_t *ram_arc;
    lv_obj_t *psram_arc;
    lv_obj_t *ram_value;
    lv_obj_t *ram_free;
    lv_obj_t *ram_largest;
    lv_obj_t *psram_value;
    lv_obj_t *psram_used;
    lv_obj_t *psram_free;
    lv_obj_t *storage_value;
    lv_obj_t *storage_free;
    lv_obj_t *storage_bar;
    lv_obj_t *sd_value;
    lv_obj_t *sd_free;
    lv_obj_t *sd_status;
    lv_obj_t *sd_bar;
    lv_obj_t *cpu_chart;
    lv_chart_series_t *cpu_series;
    uint32_t sampled_sequence;
    uint32_t sampled_uptime;
    bool initialized;
} np_settings_system_t;

void np_settings_system_sync(np_settings_system_t *system, const char *firmware,
                              const char *temperature, bool restarting);
void np_settings_system_sync_resources(np_settings_system_t *system,
                                        const system_resource_snapshot_t *resources);
void np_settings_system_pause(np_settings_system_t *system);

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    lv_obj_t *back_button;
    lv_obj_t *update_button;
    np_settings_system_t system;
} np_settings_system_view_t;

/* Lazy, idempotent construction. Re-enter by showing this same root. */
void np_settings_system_scene_create(np_settings_system_view_t *view, lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
