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

#include "app_state.h"
#include "diagnostic_ui.h"
#include "flash_coordinator.h"
#include "network_validation_service.h"
#include "provisioning_service.h"
#include "wifi_setup_view.h"

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

typedef enum {
    CACHE_TEST_FULL_FILESYSTEM,
    CACHE_TEST_CUT_BEFORE_RENAME,
    CACHE_TEST_CUT_AFTER_RENAME,
} cache_test_action_t;

typedef struct {
    lv_obj_t *coordinate_label;
    lv_obj_t *state_label;
    lv_obj_t *memory_label;
    lv_obj_t *memory_campaign_label;
    lv_obj_t *render_label;
    lv_obj_t *connectivity_label;
    lv_obj_t *network_arm_button;
    lv_obj_t *network_arm_button_label;
    lv_obj_t *wifi_setup_button;
    lv_obj_t *wifi_setup_button_label;
    lv_obj_t *provisioning_label;
    lv_obj_t *cache_full_button;
    lv_obj_t *cache_full_button_label;
    lv_obj_t *cache_cut_before_button;
    lv_obj_t *cache_cut_before_button_label;
    lv_obj_t *cache_cut_after_button;
    lv_obj_t *cache_cut_after_button_label;
    lv_obj_t *offline_refresh_button;
    lv_obj_t *offline_refresh_button_label;
    lv_obj_t *cache_test_status_label;
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
    bool compaction_requested;
    uint32_t cache_test_sequence;
    cache_test_action_t cache_test_action;
} diagnostic_ui_state_t;

static diagnostic_ui_state_t s_state;

static const cache_test_action_t s_cache_full_action = CACHE_TEST_FULL_FILESYSTEM;
static const cache_test_action_t s_cache_cut_before_action = CACHE_TEST_CUT_BEFORE_RENAME;
static const cache_test_action_t s_cache_cut_after_action = CACHE_TEST_CUT_AFTER_RENAME;

static const char *cache_test_action_name(cache_test_action_t action)
{
    switch (action) {
    case CACHE_TEST_FULL_FILESYSTEM:
        return "G4 filesystem cheio";
    case CACHE_TEST_CUT_BEFORE_RENAME:
        return "G4 corte antes do rename";
    case CACHE_TEST_CUT_AFTER_RENAME:
        return "G4 corte apos o rename";
    default:
        return "G4 ensaio";
    }
}

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
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    const app_storage_projection_t *const status = &projection.storage;

    if (!projection.ready || !status->ready) {
        lv_label_set_text_fmt(s_state.flash_status_label, "NVS diagnostico: indisponivel (%s)",
                              esp_err_to_name(status->init_result));
    } else if (!status->littlefs_ready && !status->busy && !status->pending) {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "LittleFS: segure o botao para formatar storage (%s)",
                              esp_err_to_name(status->littlefs_init_result));
    } else if (status->busy || status->pending) {
        lv_label_set_text(s_state.flash_status_label, "Persistencia diagnostica: solicitacao em andamento");
    } else if (status->last_littlefs_format) {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "LittleFS formatado: %s em %lums",
                              esp_err_to_name(status->last_result),
                              (unsigned long)status->last_duration_ms);
        s_state.compaction_requested = false;
        lv_label_set_text(s_state.flash_probe_button_label,
                          "TOQUE: LFS 64x4K | SEGURE: FORMATAR LFS");
    } else if (status->last_littlefs_writes > 0U) {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "Cache g%lu CRC=%s | LFS #%lu: %s %lu x 4K=%luK em %lums",
                              (unsigned long)status->cache_generation,
                              esp_err_to_name(status->cache_result),
                              (unsigned long)status->last_sequence,
                              esp_err_to_name(status->last_result),
                              (unsigned long)status->last_littlefs_writes,
                              (unsigned long)(status->last_littlefs_verified_bytes / 1024U),
                              (unsigned long)status->last_duration_ms);
        s_state.compaction_requested = false;
        lv_label_set_text(s_state.flash_probe_button_label,
                          "TOQUE: LFS 64x4K | SEGURE: FORMATAR LFS");
    } else if (status->completed_count > 0U) {
        if (status->last_batch_writes > 0U) {
            lv_label_set_text_fmt(s_state.flash_status_label,
                                  "NVS lote #%lu: %s %lu x 512B em %lums livre %lu>%lu",
                                  (unsigned long)status->last_sequence,
                                  esp_err_to_name(status->last_result),
                                  (unsigned long)status->last_batch_writes,
                                  (unsigned long)status->last_duration_ms,
                                  (unsigned long)status->last_free_entries_before,
                                  (unsigned long)status->last_free_entries_after);
            s_state.compaction_requested = false;
            lv_label_set_text(s_state.flash_probe_button_label,
                              "TOQUE: NVS | SEGURE: LOTE 64x512B");
        } else {
            lv_label_set_text_fmt(s_state.flash_status_label,
                                  "NVS diagnostico #%lu: %s em %lums",
                                  (unsigned long)status->last_sequence,
                                  esp_err_to_name(status->last_result),
                                  (unsigned long)status->last_duration_ms);
        }
    } else if (status->cache_valid) {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "Cache g%lu CRC=%s | Config g%lu CRC=%s",
                              (unsigned long)status->cache_generation,
                              esp_err_to_name(status->cache_result),
                              (unsigned long)status->config_generation,
                              esp_err_to_name(status->config_result));
    } else if (status->cache_result == ESP_ERR_NOT_FOUND) {
        lv_label_set_text(s_state.flash_status_label,
                          "Cache offline vazio: nenhuma geracao valida ainda");
    } else {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "Cache offline invalido (%s); storage nao foi formatado",
                              esp_err_to_name(status->cache_result));
    }
}

static void update_cache_test_status_label(void)
{
    if (s_state.cache_test_sequence == 0U) {
        return;
    }

    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if ((s_state.cache_test_action == CACHE_TEST_CUT_BEFORE_RENAME ||
         s_state.cache_test_action == CACHE_TEST_CUT_AFTER_RENAME) &&
        status.power_cut_window_active) {
        const uint32_t seconds_left = (status.power_cut_remaining_ms + 999U) / 1000U;
        lv_label_set_text_fmt(s_state.state_label, "G4: DESLIGUE AGORA! %s (%lus)",
                              status.power_cut_after_rename ? "apos rename" : "antes rename",
                              (unsigned long)seconds_left);
        return;
    }
    if (s_state.cache_test_action == CACHE_TEST_FULL_FILESYSTEM && status.full_probe_active) {
        if (status.full_probe_target_bytes == 0U) {
            lv_label_set_text(s_state.state_label, "G4: preparando storage");
        } else if (status.full_probe_syncing) {
            lv_label_set_text(s_state.state_label,
                              "G4: testando rejeicao e limpando; aguarde");
        } else {
            lv_label_set_text_fmt(s_state.state_label, "G4: preenchendo %lu/%lu KiB",
                                  (unsigned long)(status.full_probe_written_bytes / 1024U),
                                  (unsigned long)(status.full_probe_target_bytes / 1024U));
        }
        return;
    }
    if (status.busy || status.pending || status.last_sequence != s_state.cache_test_sequence) {
        return;
    }

    if ((s_state.cache_test_action == CACHE_TEST_CUT_BEFORE_RENAME ||
         s_state.cache_test_action == CACHE_TEST_CUT_AFTER_RENAME) &&
        status.last_result == ESP_ERR_TIMEOUT) {
        lv_label_set_text(s_state.state_label,
                          "G4 corte: NAO VALIDADO (sem corte em 10s)");
    } else {
        lv_label_set_text_fmt(s_state.state_label, "%s %s: %s em %lums",
                              cache_test_action_name(s_state.cache_test_action),
                              status.last_result == ESP_OK ? "aprovado" : "FALHOU",
                              esp_err_to_name(status.last_result),
                              (unsigned long)status.last_duration_ms);
    }
    s_state.cache_test_sequence = 0U;
}

static const char *connectivity_state_name(app_network_state_t state)
{
    switch (state) {
    case APP_NETWORK_STATE_IDLE:
        return "ociosa";
    case APP_NETWORK_STATE_STARTING:
        return "iniciando Hosted";
    case APP_NETWORK_STATE_LINK_UP:
        return "enlace SDIO pronto";
    case APP_NETWORK_STATE_WIFI_READY:
        return "Wi-Fi pronto";
    case APP_NETWORK_STATE_SCANNING:
        return "varrendo APs";
    case APP_NETWORK_STATE_SCAN_COMPLETE:
        return "sem credencial";
    case APP_NETWORK_STATE_ASSOCIATING:
        return "associando";
    case APP_NETWORK_STATE_WAITING_FOR_IP:
        return "aguardando DHCP";
    case APP_NETWORK_STATE_ONLINE:
        return "IP adquirido";
    case APP_NETWORK_STATE_BACKOFF:
        return "recuperando";
    case APP_NETWORK_STATE_LINK_DOWN:
        return "C6/SDIO offline";
    case APP_NETWORK_STATE_RECOVERING_LINK:
        return "recuperando C6";
    case APP_NETWORK_STATE_FAILED:
        return "erro";
    default:
        return "desconhecido";
    }
}

static void update_connectivity_label(void)
{
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    const app_network_projection_t *const status = &projection.network;

    lv_label_set_text_fmt(s_state.connectivity_label,
                          "Rede: %s | APs=%u | tentativas=%lu | C6falhas=%lu | IP=%s | ultimo=%s",
                          connectivity_state_name(status->state),
                          (unsigned int)status->access_points_found,
                          (unsigned long)status->reconnect_attempts,
                          (unsigned long)status->transport_failures,
                          status->online ? "sim" : "nao",
                          esp_err_to_name(status->last_result));
    lv_obj_set_style_text_color(s_state.connectivity_label,
                                status->state == APP_NETWORK_STATE_FAILED
                                    ? lv_color_hex(0xF4C95D)
                                    : lv_color_hex(0x9DB4D1),
                                LV_PART_MAIN);
}

static void update_provisioning_label(void)
{
    provisioning_service_status_t status = {0};
    provisioning_service_get_status(&status);

    if (status.armed) {
        network_validation_status_t validation = {0};
        network_validation_service_get_status(&validation);
        lv_label_set_text_fmt(s_state.network_arm_button_label,
                              "USB REDE ARMADO: %lus", (unsigned long)status.remaining_seconds);
        if (validation.busy) {
            lv_label_set_text(s_state.provisioning_label,
                              "USB: CHECK DNS/NTP/HTTPS em andamento; uma conexao TLS");
        } else {
            lv_label_set_text(s_state.provisioning_label,
                              "USB: OPEN, CHECK, RECOVER_C6 ou CACHE_CORRUPT; sem senha");
        }
        lv_obj_set_style_bg_color(s_state.network_arm_button, lv_color_hex(0x7A3E10), LV_PART_MAIN);
    } else {
        network_validation_status_t validation = {0};
        network_validation_service_get_status(&validation);
        lv_label_set_text(s_state.network_arm_button_label, "ARMAR REDE USB (60s)");
        if (validation.completed_checks > 0U) {
            lv_label_set_text_fmt(s_state.provisioning_label,
                                  "DNS=%s NTP=%s HTTPS=%s em %lums",
                                  esp_err_to_name(validation.dns_result),
                                  esp_err_to_name(validation.ntp_result),
                                  esp_err_to_name(validation.https_result),
                                  (unsigned long)validation.last_duration_ms);
        } else {
            lv_label_set_text_fmt(s_state.provisioning_label,
                                  "USB: fechado | ultimo=%s", esp_err_to_name(status.last_result));
        }
        lv_obj_set_style_bg_color(s_state.network_arm_button, lv_color_hex(0x183554), LV_PART_MAIN);
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

    update_connectivity_label();
    update_provisioning_label();
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
    update_cache_test_status_label();
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

static void flash_probe_button_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_LONG_PRESSED) {
        if (!s_state.stress_active) {
            lv_label_set_text(s_state.flash_status_label,
                              "Formatacao LFS bloqueada: ative a carga de render antes");
        } else if (!s_state.compaction_requested) {
            const esp_err_t request_err = flash_coordinator_request_littlefs_format();
            if (request_err == ESP_OK) {
                s_state.compaction_requested = true;
                lv_label_set_text(s_state.flash_probe_button_label, "LFS: FORMATANDO");
                lv_label_set_text(s_state.flash_status_label,
                                  "LittleFS: formatacao explicita de storage durante carga");
            } else {
                lv_label_set_text_fmt(s_state.flash_status_label,
                                      "Formatacao LFS recusada: %s", esp_err_to_name(request_err));
            }
        }
        return;
    }
    if (code != LV_EVENT_CLICKED) {
        return;
    }

    if (s_state.compaction_requested) {
        return;
    }

    if (!s_state.stress_active) {
        lv_label_set_text(s_state.flash_status_label,
                          "LittleFS bloqueado: ative a carga de render antes");
        return;
    }

    const esp_err_t request_err = flash_coordinator_request_littlefs_probe();
    if (request_err == ESP_OK) {
        s_state.compaction_requested = true;
        lv_label_set_text(s_state.flash_probe_button_label, "LFS: SOLICITADO");
        lv_label_set_text(s_state.flash_status_label,
                          "LittleFS: 64 ciclos de write/fsync/rename/verificacao");
    } else {
        lv_label_set_text_fmt(s_state.flash_status_label,
                              "LittleFS recusado: %s", esp_err_to_name(request_err));
    }
}

static void cache_test_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const cache_test_action_t action = *(const cache_test_action_t *)lv_event_get_user_data(event);
    const char *const name = cache_test_action_name(action);
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    esp_err_t request_err = ESP_ERR_INVALID_ARG;
    switch (action) {
    case CACHE_TEST_FULL_FILESYSTEM:
        request_err = flash_coordinator_request_cache_full_probe();
        break;
    case CACHE_TEST_CUT_BEFORE_RENAME:
        request_err = flash_coordinator_request_cache_cut_before_rename();
        break;
    case CACHE_TEST_CUT_AFTER_RENAME:
        request_err = flash_coordinator_request_cache_cut_after_rename();
        break;
    default:
        break;
    }

    if (request_err == ESP_OK) {
        s_state.cache_test_sequence = status.last_sequence + 1U;
        s_state.cache_test_action = action;
        lv_label_set_text_fmt(s_state.state_label, "%s: NA FILA%s", name,
                              s_state.stress_active ? "; pause CARGA para iniciar" : "");
    } else {
        lv_label_set_text_fmt(s_state.state_label, "%s recusado: %s", name,
                              esp_err_to_name(request_err));
    }
}

static void offline_refresh_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const esp_err_t result = network_validation_service_request_offline_data_refresh();
    if (result == ESP_OK) {
        lv_label_set_text(s_state.state_label,
                          "G4 dados Brasilia: NA FILA; acompanhe REDE LOCAL");
    } else {
        lv_label_set_text_fmt(s_state.state_label,
                              "G4 dados Brasilia recusado: %s", esp_err_to_name(result));
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
    } else {
        /* Capture and display the baseline once without refreshing labels in the hot loop. */
        update_telemetry(true);
    }
}

static void network_arm_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const esp_err_t result = provisioning_service_arm_open_network();
    if (result != ESP_OK) {
        lv_label_set_text_fmt(s_state.provisioning_label,
                              "USB de rede indisponivel: %s", esp_err_to_name(result));
        return;
    }
    update_provisioning_label();
}

static void wifi_setup_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    const esp_err_t result = wifi_setup_view_open(lv_screen_active());
    if (result != ESP_OK) {
        lv_label_set_text_fmt(s_state.provisioning_label,
                              "Configuracao Wi-Fi indisponivel: %s", esp_err_to_name(result));
        return;
    }
    lv_label_set_text(s_state.state_label, "CONFIGURACAO WPA2 ABERTA — senha somente em RAM");
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
    lv_label_set_text(instruction,
                      "Toque os alvos; com carga ativa, teste LittleFS ou segure para formatar storage");
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

    s_state.connectivity_label = lv_label_create(screen);
    lv_label_set_text(s_state.connectivity_label, "Rede: aguardando worker");
    lv_obj_set_style_text_color(s_state.connectivity_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.connectivity_label, LV_ALIGN_TOP_MID, 0, 158);

    s_state.stress_button = lv_button_create(screen);
    lv_obj_set_size(s_state.stress_button, 240, 34);
    lv_obj_align(s_state.stress_button, LV_ALIGN_TOP_MID, 0, 184);
    lv_obj_set_style_radius(s_state.stress_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.stress_button, lv_color_hex(0x183554), LV_PART_MAIN);
    s_state.stress_button_label = lv_label_create(s_state.stress_button);
    lv_label_set_text(s_state.stress_button_label, "CARGA DE RENDER: PAUSADA");
    lv_obj_set_style_text_color(s_state.stress_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.stress_button_label);
    lv_obj_add_event_cb(s_state.stress_button, stress_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.stress_bar = lv_obj_create(screen);
    lv_obj_set_size(s_state.stress_bar, DIAG_STRESS_BAR_WIDTH, 22);
    lv_obj_set_pos(s_state.stress_bar, 182, 232);
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
    lv_label_set_text(s_state.flash_probe_button_label,
                      "TOQUE: LFS 64x4K | SEGURE: FORMATAR LFS");
    lv_obj_set_style_text_color(s_state.flash_probe_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.flash_probe_button_label);
    lv_obj_add_event_cb(s_state.flash_probe_button, flash_probe_button_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_state.flash_probe_button, flash_probe_button_event_cb, LV_EVENT_LONG_PRESSED,
                        NULL);

    s_state.flash_status_label = lv_label_create(screen);
    lv_label_set_text(s_state.flash_status_label, "NVS diagnostico: inicializando");
    lv_obj_set_style_text_color(s_state.flash_status_label, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_align(s_state.flash_status_label, LV_ALIGN_TOP_MID, 0, 412);

    s_state.network_arm_button = lv_button_create(screen);
    lv_obj_set_size(s_state.network_arm_button, 240, 30);
    lv_obj_align(s_state.network_arm_button, LV_ALIGN_TOP_MID, 0, 448);
    lv_obj_set_style_radius(s_state.network_arm_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.network_arm_button, lv_color_hex(0x183554), LV_PART_MAIN);
    s_state.network_arm_button_label = lv_label_create(s_state.network_arm_button);
    lv_label_set_text(s_state.network_arm_button_label, "ARMAR REDE USB (60s)");
    lv_obj_set_style_text_color(s_state.network_arm_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.network_arm_button_label);
    lv_obj_add_event_cb(s_state.network_arm_button, network_arm_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.wifi_setup_button = lv_button_create(screen);
    lv_obj_set_size(s_state.wifi_setup_button, 240, 30);
    lv_obj_align(s_state.wifi_setup_button, LV_ALIGN_TOP_MID, 0, 482);
    lv_obj_set_style_radius(s_state.wifi_setup_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.wifi_setup_button, lv_color_hex(0x126A50), LV_PART_MAIN);
    s_state.wifi_setup_button_label = lv_label_create(s_state.wifi_setup_button);
    lv_label_set_text(s_state.wifi_setup_button_label, "CONFIGURAR WI-FI WPA2");
    lv_obj_set_style_text_color(s_state.wifi_setup_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.wifi_setup_button_label);
    lv_obj_add_event_cb(s_state.wifi_setup_button, wifi_setup_button_event_cb, LV_EVENT_CLICKED, NULL);

    s_state.cache_full_button = lv_button_create(screen);
    lv_obj_set_size(s_state.cache_full_button, 280, 30);
    lv_obj_align(s_state.cache_full_button, LV_ALIGN_TOP_LEFT, 44, 516);
    lv_obj_set_style_radius(s_state.cache_full_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.cache_full_button, lv_color_hex(0x6B4214), LV_PART_MAIN);
    s_state.cache_full_button_label = lv_label_create(s_state.cache_full_button);
    lv_label_set_text(s_state.cache_full_button_label, "G4: FS CHEIO");
    lv_obj_set_style_text_color(s_state.cache_full_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.cache_full_button_label);
    lv_obj_add_event_cb(s_state.cache_full_button, cache_test_button_event_cb, LV_EVENT_CLICKED,
                        (void *)&s_cache_full_action);

    s_state.cache_cut_before_button = lv_button_create(screen);
    lv_obj_set_size(s_state.cache_cut_before_button, 280, 30);
    lv_obj_align(s_state.cache_cut_before_button, LV_ALIGN_TOP_MID, 0, 516);
    lv_obj_set_style_radius(s_state.cache_cut_before_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.cache_cut_before_button, lv_color_hex(0x7A321D), LV_PART_MAIN);
    s_state.cache_cut_before_button_label = lv_label_create(s_state.cache_cut_before_button);
    lv_label_set_text(s_state.cache_cut_before_button_label, "G4: CORTE PRE-RENAME");
    lv_obj_set_style_text_color(s_state.cache_cut_before_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.cache_cut_before_button_label);
    lv_obj_add_event_cb(s_state.cache_cut_before_button, cache_test_button_event_cb, LV_EVENT_CLICKED,
                        (void *)&s_cache_cut_before_action);

    s_state.cache_cut_after_button = lv_button_create(screen);
    lv_obj_set_size(s_state.cache_cut_after_button, 280, 30);
    lv_obj_align(s_state.cache_cut_after_button, LV_ALIGN_TOP_RIGHT, -44, 516);
    lv_obj_set_style_radius(s_state.cache_cut_after_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.cache_cut_after_button, lv_color_hex(0x7A321D), LV_PART_MAIN);
    s_state.cache_cut_after_button_label = lv_label_create(s_state.cache_cut_after_button);
    lv_label_set_text(s_state.cache_cut_after_button_label, "G4: CORTE POS-RENAME");
    lv_obj_set_style_text_color(s_state.cache_cut_after_button_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(s_state.cache_cut_after_button_label);
    lv_obj_add_event_cb(s_state.cache_cut_after_button, cache_test_button_event_cb, LV_EVENT_CLICKED,
                        (void *)&s_cache_cut_after_action);

    s_state.offline_refresh_button = lv_button_create(screen);
    lv_obj_set_size(s_state.offline_refresh_button, 280, 30);
    lv_obj_align(s_state.offline_refresh_button, LV_ALIGN_TOP_MID, 0, 550);
    lv_obj_set_style_radius(s_state.offline_refresh_button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_state.offline_refresh_button, lv_color_hex(0x126A50), LV_PART_MAIN);
    s_state.offline_refresh_button_label = lv_label_create(s_state.offline_refresh_button);
    lv_label_set_text(s_state.offline_refresh_button_label, "G4: ATUALIZAR DADOS BRASILIA");
    lv_obj_set_style_text_color(s_state.offline_refresh_button_label, lv_color_hex(0xF4F7FB),
                                LV_PART_MAIN);
    lv_obj_center(s_state.offline_refresh_button_label);
    lv_obj_add_event_cb(s_state.offline_refresh_button, offline_refresh_button_event_cb,
                        LV_EVENT_CLICKED, NULL);

    s_state.cache_test_status_label = lv_label_create(screen);
    lv_obj_add_flag(s_state.cache_test_status_label, LV_OBJ_FLAG_HIDDEN);

    s_state.provisioning_label = lv_label_create(screen);
    lv_label_set_text(s_state.provisioning_label, "USB: fechado");
    lv_obj_set_style_text_color(s_state.provisioning_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_align(s_state.provisioning_label, LV_ALIGN_TOP_MID, 0, 646);

    s_state.state_label = lv_label_create(screen);
    lv_label_set_text(s_state.state_label, "AGUARDANDO TOQUE");
    lv_obj_set_style_text_color(s_state.state_label, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_set_width(s_state.state_label, 920);
    lv_label_set_long_mode(s_state.state_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(s_state.state_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(s_state.state_label, LV_ALIGN_BOTTOM_MID, 0, -8);

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
