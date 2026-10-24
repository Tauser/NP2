#include "np_settings_system.h"

#include <stdio.h>

#define SYSTEM_ANIMATION_MS 160U

static void arc_exec(void *obj, int32_t value) { lv_arc_set_value(obj, value); }
static void bar_exec(void *obj, int32_t value) { lv_bar_set_value(obj, value, LV_ANIM_OFF); }

static void meter_set(lv_obj_t *obj, uint8_t target, bool arc, bool animate)
{
    const lv_anim_exec_xcb_t exec = arc ? arc_exec : bar_exec;
    const int32_t prior = arc ? lv_arc_get_value(obj) : lv_bar_get_value(obj);
    lv_anim_delete(obj, exec);
    if (prior == target) return;
    if (!animate) { exec(obj, target); return; }
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, exec);
    lv_anim_set_values(&anim, prior, target);
    lv_anim_set_duration(&anim, SYSTEM_ANIMATION_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

void np_settings_system_pause(np_settings_system_t *system)
{
    if (system == NULL || system->root == NULL) return;
    lv_anim_delete(system->cpu_arc, arc_exec);
    lv_anim_delete(system->ram_arc, arc_exec);
    lv_anim_delete(system->psram_arc, arc_exec);
    lv_anim_delete(system->storage_bar, bar_exec);
    lv_anim_delete(system->sd_bar, bar_exec);
    system->initialized = false;
}

static lv_obj_t *gauge(lv_obj_t *parent, int32_t x, int32_t y, int32_t size,
                       bool semicircle, lv_color_t color)
{
    lv_obj_t *obj = lv_arc_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_arc_color(obj, np_c_surface_raised(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(obj, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(obj, semicircle ? 10 : 12, LV_PART_MAIN);
    lv_obj_set_style_arc_width(obj, semicircle ? 10 : 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(obj, true, LV_PART_INDICATOR);
    lv_arc_set_rotation(obj, semicircle ? 0 : 135);
    lv_arc_set_bg_angles(obj, semicircle ? 180 : 0, semicircle ? 360 : 270);
    lv_arc_set_range(obj, 0, 100);
    lv_arc_set_value(obj, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *usage_bar(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, 20, 96);
    lv_obj_set_size(bar, 256, 10);
    lv_obj_set_style_bg_color(bar, np_c_surface_raised(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return bar;
}

static lv_obj_t *metric_panel(lv_obj_t *root, int32_t x, int32_t y, int32_t w,
                              int32_t h, const char *title, const char *icon)
{
    lv_obj_t *panel = np_panel(root, x, y, w, h);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, np_c_hairline(), LV_PART_MAIN);
    np_label(panel, icon, NP_FONT_ICON, np_c_accent(), 20, 20, 24, LV_TEXT_ALIGN_LEFT);
    np_label(panel, title, NP_FONT_MD, np_c_text(), 56, 20, w - 76, LV_TEXT_ALIGN_LEFT);
    return panel;
}

void np_settings_system_sync(np_settings_system_t *system, const char *firmware,
                              const char *temperature, bool restarting)
{
    if (system == NULL || system->firmware == NULL) return;
    np_set_text(system->firmware, firmware != NULL && firmware[0] != '\0' ? firmware : "Carregando...");
    np_set_text(system->temperature, temperature);
    if (restarting) lv_obj_add_state(system->restart_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(system->restart_button, LV_STATE_DISABLED);
}

static void percent_label(lv_obj_t *label, uint8_t percent, bool available)
{
    char text[12];
    if (available) (void)snprintf(text, sizeof(text), "%u%%", (unsigned)percent);
    else (void)snprintf(text, sizeof(text), "--");
    np_set_text(label, text);
}

static void storage_labels(lv_obj_t *value, lv_obj_t *free_label, uint64_t total,
                            uint64_t free_bytes, bool available, bool gib)
{
    char text[56];
    if (!available || total == 0U || free_bytes > total) {
        np_set_text(value, "Indisponível");
        np_set_text(free_label, "Sem medida de espaço");
        return;
    }
    const uint64_t unit = gib ? UINT64_C(1073741824) : UINT64_C(1048576);
    const uint64_t used_tenths = ((total - free_bytes) * 10U + unit / 2U) / unit;
    const uint64_t total_tenths = (total * 10U + unit / 2U) / unit;
    const uint64_t free_tenths = (free_bytes * 10U + unit / 2U) / unit;
    const char *const suffix = gib ? "GiB" : "MiB";
    (void)snprintf(text, sizeof(text), "%lu,%lu / %lu,%lu %s",
                   (unsigned long)(used_tenths / 10U), (unsigned long)(used_tenths % 10U),
                   (unsigned long)(total_tenths / 10U), (unsigned long)(total_tenths % 10U), suffix);
    np_set_text(value, text);
    (void)snprintf(text, sizeof(text), "%lu,%lu %s livres",
                   (unsigned long)(free_tenths / 10U), (unsigned long)(free_tenths % 10U), suffix);
    np_set_text(free_label, text);
}

void np_settings_system_sync_resources(np_settings_system_t *system,
                                        const system_resource_snapshot_t *r)
{
    if (system == NULL || system->root == NULL || r == NULL) return;
    const bool visible = !lv_obj_has_flag(system->root, LV_OBJ_FLAG_HIDDEN);
    const bool animate = visible && system->initialized && !r->maintenance;
    if (!visible || r->maintenance) np_settings_system_pause(system);
    np_set_text(system->status, r->maintenance ? "Manutenção" : r->available ? "Em operação" : "Coletando...");
    char text[64];
    if (r->available) {
        const uint32_t minutes = r->uptime_s / 60U;
        if (minutes >= 1440U) (void)snprintf(text, sizeof(text), "%lu d %02lu h",
            (unsigned long)(minutes / 1440U), (unsigned long)((minutes / 60U) % 24U));
        else (void)snprintf(text, sizeof(text), "%02lu h %02lu min",
            (unsigned long)(minutes / 60U), (unsigned long)(minutes % 60U));
        np_set_text(system->uptime, text);
    }
    percent_label(system->cpu_value, r->cpu_percent, r->cpu_available);
    if (r->cpu_available) (void)snprintf(text, sizeof(text), "C0 %u%% · C1 %u%%",
        (unsigned)r->cpu_core_percent[0], (unsigned)r->cpu_core_percent[1]);
    else (void)snprintf(text, sizeof(text), "CPU · aguardando amostra");
    np_set_text(system->core_value, text);
    meter_set(system->cpu_arc, r->cpu_available ? r->cpu_percent : 0U, true, animate);
    const bool ram = r->available && r->internal_total_bytes != 0U &&
                     r->internal_free_bytes <= r->internal_total_bytes;
    const bool psram = r->available && r->psram_total_bytes != 0U &&
                       r->psram_free_bytes <= r->psram_total_bytes;
    const uint8_t ram_pct = ram ? system_used_percent(r->internal_total_bytes, r->internal_free_bytes) : 0U;
    const uint8_t psram_pct = psram ? system_used_percent(r->psram_total_bytes, r->psram_free_bytes) : 0U;
    percent_label(system->ram_value, ram_pct, ram);
    percent_label(system->psram_value, psram_pct, psram);
    meter_set(system->ram_arc, ram_pct, true, animate);
    meter_set(system->psram_arc, psram_pct, true, animate);
    if (ram) {
        (void)snprintf(text, sizeof(text), "%lu KiB livres", (unsigned long)(r->internal_free_bytes / 1024U));
        np_set_text(system->ram_free, text);
        (void)snprintf(text, sizeof(text), "Maior bloco: %lu KiB", (unsigned long)(r->internal_largest_bytes / 1024U));
        np_set_text(system->ram_largest, text);
    } else {
        np_set_text(system->ram_free, "Indisponível");
        np_set_text(system->ram_largest, "Heap interno");
    }
    storage_labels(system->psram_used, system->psram_free, r->psram_total_bytes,
                   r->psram_free_bytes, psram, false);
    const bool storage = r->storage_available && r->storage_used_bytes <= r->storage_total_bytes;
    const uint32_t storage_free = storage ? r->storage_total_bytes - r->storage_used_bytes : 0U;
    storage_labels(system->storage_value, system->storage_free, r->storage_total_bytes,
                   storage_free, storage, false);
    meter_set(system->storage_bar, storage ? system_used_percent(r->storage_total_bytes, storage_free) : 0U,
        false, animate);
    storage_labels(system->sd_value, system->sd_free, r->sd_total_bytes, r->sd_free_bytes,
                   r->sd_available && r->sd_mounted, true);
    np_set_text(system->sd_status, !r->sd_mounted ? "Ausente" : r->sd_available ? "Montado" : "Sem leitura");
    np_set_text_color(system->sd_status, r->sd_available && r->sd_mounted ? np_c_positive() : np_c_warning());
    meter_set(system->sd_bar, r->sd_available && r->sd_mounted ?
        system_used_percent(r->sd_total_bytes, r->sd_free_bytes) : 0U, false, animate);
    if (visible && !r->maintenance && r->sequence != system->sampled_sequence) {
        /* A gap after leaving this scene is never stretched into 60 s. */
        if (system->sampled_uptime != 0U && r->uptime_s - system->sampled_uptime > 3U)
            lv_chart_set_all_values(system->cpu_chart, system->cpu_series, LV_CHART_POINT_NONE);
        lv_chart_set_next_value(system->cpu_chart, system->cpu_series,
                               r->cpu_available ? r->cpu_percent : LV_CHART_POINT_NONE);
        system->sampled_sequence = r->sequence;
        system->sampled_uptime = r->uptime_s;
    }
    system->initialized = visible && !r->maintenance;
}

void np_settings_system_scene_create(np_settings_system_view_t *view, lv_obj_t *parent)
{
    if (view == NULL || parent == NULL || view->root != NULL) return;
    view->root = np_scene(parent);
    view->system.root = view->root;
    np_set_visible(view->root, false);
    view->header = np_header(view->root);
    view->back_button = np_button(view->root, NP_HEADER_NAV_X, 12, NP_HEADER_NAV_W,
                                   NP_TOUCH_TARGET, "Voltar", false);
    np_label(view->root, "Sistema", NP_FONT_TITLE, np_c_text(), 24, 80, 160, LV_TEXT_ALIGN_LEFT);
    np_dot(view->root, 204, 94, 9, np_c_positive());
    view->system.status = np_label(view->root, "Coletando...", NP_FONT_SM, np_c_positive(),
                                    224, 89, 184, LV_TEXT_ALIGN_LEFT);

    np_settings_system_t *const s = &view->system;
    lv_obj_t *cpu = metric_panel(view->root, 24, 136, 352, 344, "Processamento", NP_ICON_CPU);
    s->cpu_arc = gauge(cpu, 85, 46, 182, false, np_c_accent());
    s->cpu_value = np_label(cpu, "--", NP_FONT_DISPLAY, np_c_text(), 78, 103, 196, LV_TEXT_ALIGN_CENTER);
    s->core_value = np_label(cpu, "CPU · aguardando amostra", NP_FONT_SM, np_c_text_2(),
                              40, 167, 272, LV_TEXT_ALIGN_CENTER);
    np_label(cpu, "CPU · últimos 60 s", NP_FONT_SM, np_c_text_3(), 20, 212, 312, LV_TEXT_ALIGN_RIGHT);
    s->cpu_chart = lv_chart_create(cpu);
    lv_obj_remove_style_all(s->cpu_chart);
    lv_obj_set_pos(s->cpu_chart, 20, 236);
    lv_obj_set_size(s->cpu_chart, 312, 30);
    lv_obj_set_style_line_width(s->cpu_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(s->cpu_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_remove_flag(s->cpu_chart, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(s->cpu_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(s->cpu_chart, 0, 0);
    lv_chart_set_axis_range(s->cpu_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(s->cpu_chart, SYSTEM_CPU_HISTORY_COUNT);
    s->cpu_series = lv_chart_add_series(s->cpu_chart, np_c_accent(), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(s->cpu_chart, s->cpu_series, LV_CHART_POINT_NONE);
    np_hline(cpu, 20, 274, 312);
    s->temperature = np_label(cpu, "--", NP_FONT_LG, np_c_text(), 20, 281, 146, LV_TEXT_ALIGN_LEFT);
    s->uptime = np_label(cpu, "--", NP_FONT_MD, np_c_text(), 184, 283, 154, LV_TEXT_ALIGN_LEFT);
    np_label(cpu, "Temperatura", NP_FONT_SM, np_c_text_3(), 20, 309, 146, LV_TEXT_ALIGN_LEFT);
    np_label(cpu, "Tempo ligado", NP_FONT_SM, np_c_text_3(), 184, 309, 154, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *ram = metric_panel(view->root, 392, 136, 296, 174, "RAM interna", NP_ICON_MEMORY);
    s->ram_arc = gauge(ram, 87, 44, 122, true, np_c_accent());
    s->ram_value = np_label(ram, "--", NP_FONT_LG, np_c_text(), 80, 81, 136, LV_TEXT_ALIGN_CENTER);
    s->ram_free = np_label(ram, "Carregando...", NP_FONT_MD, np_c_text(), 20, 112, 256, LV_TEXT_ALIGN_LEFT);
    s->ram_largest = np_label(ram, "Heap interno", NP_FONT_SM, np_c_text_3(), 20, 138, 256, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *psram = metric_panel(view->root, 704, 136, 296, 174, "PSRAM", NP_ICON_MEMORY);
    s->psram_arc = gauge(psram, 87, 44, 122, true, np_c_accent());
    s->psram_value = np_label(psram, "--", NP_FONT_LG, np_c_text(), 80, 81, 136, LV_TEXT_ALIGN_CENTER);
    s->psram_used = np_label(psram, "Carregando...", NP_FONT_MD, np_c_text(), 20, 112, 256, LV_TEXT_ALIGN_LEFT);
    s->psram_free = np_label(psram, "Heap externo", NP_FONT_SM, np_c_text_3(), 20, 138, 256, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *storage = metric_panel(view->root, 392, 326, 296, 154, "Armazenamento", NP_ICON_STORAGE);
    s->storage_value = np_label(storage, "Carregando...", NP_FONT_LG, np_c_text(), 20, 59, 256, LV_TEXT_ALIGN_LEFT);
    s->storage_bar = usage_bar(storage, np_c_accent());
    s->storage_free = np_label(storage, "LittleFS · cache e arquivos", NP_FONT_SM, np_c_text_2(), 20, 120, 256, LV_TEXT_ALIGN_LEFT);
    lv_obj_t *sd = metric_panel(view->root, 704, 326, 296, 154, "Cartão SD", NP_ICON_SD_CARD);
    s->sd_status = np_label(sd, "Ausente", NP_FONT_SM, np_c_text_3(), 180, 21, 96, LV_TEXT_ALIGN_RIGHT);
    s->sd_value = np_label(sd, "Carregando...", NP_FONT_LG, np_c_text(), 20, 59, 256, LV_TEXT_ALIGN_LEFT);
    s->sd_bar = usage_bar(sd, np_c_positive());
    s->sd_free = np_label(sd, "Sem medida de espaço", NP_FONT_SM, np_c_text_2(), 20, 120, 256, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *actions = np_panel(view->root, 24, 496, 976, 80);
    np_label(actions, "Firmware", NP_FONT_SM, np_c_text_3(), 20, 18, 100, LV_TEXT_ALIGN_LEFT);
    s->firmware = np_label(actions, "Carregando...", NP_FONT_MD, np_c_text(), 124, 16, 452, LV_TEXT_ALIGN_LEFT);
    np_label(actions, "ESP32-P4 + C6 · IDF 5.5.4", NP_FONT_SM, np_c_text_2(), 20, 48, 432, LV_TEXT_ALIGN_LEFT);
#ifdef NP2_SPOTIFY_PAIRING_BENCH
    view->update_button = np_button(actions, 600, 16, 176, NP_TOUCH_TARGET, "Parear Spotify", false);
#else
    view->update_button = np_button(actions, 600, 16, 176, NP_TOUCH_TARGET, NP_ICON_REFRESH " Atualizar", false);
    lv_obj_add_state(view->update_button, LV_STATE_DISABLED);
#endif
    s->restart_button = np_button(actions, 792, 16, 164, NP_TOUCH_TARGET, NP_ICON_POWER " Reiniciar", false);
}
