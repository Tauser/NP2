#include "product_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_state.h"
#include "device_control_service.h"
#include "flash_coordinator.h"
#include "esp_check.h"
#include "esp_log.h"
#include "offline_value_format.h"
#include "np_screens.h"
#include "np_profile.h"
#include "np_preferences.h"
#include "np_navigation_manager.h"
#include "np_feedback.h"
#include "np_confirm.h"
#include "np_keyboard.h"
#include "notification_service.h"
#include "onboarding_service.h"
#include "weather_condition.h"
#include "provisioning_service.h"
#include "settings/np_settings_display_sound.h"
#include "settings/np_settings_notifications.h"
#include "settings/np_settings_system.h"
#include "settings/np_settings_timezone.h"
#include "settings/np_settings_wifi.h"
#include "settings/np_wifi_password.h"

#define UI_REFRESH_PERIOD_MS 250U
#define BOOT_MINIMUM_MS 1200U
#define BOOT_MAXIMUM_MS 6000U
#define BOOT_SAVED_NETWORK_MAXIMUM_MS 15000U
#define BOOT_TRANSITION_MS 180U

typedef struct {
    enum {
        PRODUCT_SCREEN_BOOT = 0,
        PRODUCT_SCREEN_HOME,
        PRODUCT_SCREEN_PROFILE,
        PRODUCT_SCREEN_PREFERENCES,
        PRODUCT_SCREEN_DISPLAY_SOUND,
        PRODUCT_SCREEN_SYSTEM,
        PRODUCT_SCREEN_NOTIFICATIONS,
        PRODUCT_SCREEN_WIFI,
        PRODUCT_SCREEN_TIMEZONE,
    } active_screen;
    lv_display_t *display;
    lv_indev_t *touch_indev;
    lv_obj_t *shell;
    lv_obj_t *content_host;
    np_header_t shell_header;
    np_navigation_manager_t navigation;
    np_boot_view_t boot;
    np_home_view_t home;
    np_profile_view_t profile;
    np_preferences_view_t preferences;
    np_settings_display_sound_view_t display_sound;
    np_settings_system_view_t system_scene;
    np_settings_notifications_view_t notifications_scene;
    np_settings_wifi_view_t wifi_scene;
    np_settings_timezone_view_t timezone_scene;
    uint32_t timezone_scene_object_count;
    np_feedback_t feedback;
    np_keyboard_t keyboard;
    np_wifi_password_t wifi_password;
    np_settings_wifi_add_t wifi_add;
    bool wifi_add_pending;
    np_confirm_t wifi_confirmation;
    np_confirm_t system_confirmation;
    bool system_confirmation_pending;
    uint32_t restart_completion_id;
    char pending_wifi_ssid[33];
    bool pending_wifi_secure;
    bool wifi_password_pending;
    bool wifi_confirmation_pending;
    lv_timer_t *refresh_timer;
    lv_timer_t *home_transition_timer;
    lv_timer_t *navigation_build_timer;
    lv_timer_t *settings_value_bubble_timer;
    uint32_t started_at_tick;
    uint32_t saved_network_wait_started_tick;
    uint32_t rendered_revision;
    uint32_t notified_persisted_generation;
    uint32_t control_save_completion_id;
    offline_data_snapshot_t rendered_home_data;
    const void *rendered_weather_icon_source;
    uint8_t boot_rendered_stage;
    bool boot_saved_network_seen;
    bool home_transition_pending;
    bool navigation_pending;
    bool profile_editor_pending;
    bool notification_feedback_initialized;
    bool syncing_notification_controls;
    bool syncing_night_control;
    bool home_data_rendered;
    bool display_sound_callbacks_initialized;
    bool notification_callbacks_initialized;
    uint8_t projected_brightness;
    uint8_t projected_volume;
    uint32_t system_scene_object_count;
    uintptr_t navigation_destination;
    bool navigation_scene_released;
} product_ui_state_t;

static product_ui_state_t s_ui;
static const char *const TAG = "product_ui";

static void install_display_sound_callbacks(void);
static void settings_value_bubble_timer_cb(lv_timer_t *timer);
static void night_switch_event_cb(lv_event_t *event);
static void notification_switch_event_cb(lv_event_t *event);
static void notifications_test_event_cb(lv_event_t *event);
static esp_err_t timezone_select_cb(void *user_data, uint16_t timezone_index);
static void wifi_manage_event_cb(lv_event_t *event);
static void wifi_add_open_async(void *user_data);
static void wifi_password_open_async(void *user_data);
static void wifi_forget_open_async(void *user_data);
static void wifi_scan_event_cb(lv_event_t *event);
static void wifi_forget_event_cb(lv_event_t *event);
static void discard_wifi_password(void);
static void discard_wifi_confirmation(void);
static void discard_system_confirmation(void);
static void system_restart_event_cb(lv_event_t *event);
static np_wifi_status_t wifi_status_for(const app_network_projection_t *network);
static void scene_navigation_event_cb(lv_event_t *event);
static void scene_navigation_async(void *user_data);
static void scene_navigation_build_timer_cb(lv_timer_t *timer);
static void release_current_scene(void);
static void settings_home_async(void *user_data);
static void profile_identity_event_cb(lv_event_t *event);
static void profile_editor_open_async(void *user_data);
static void profile_save_event_cb(lv_event_t *event);
static void profile_cancel_event_cb(lv_event_t *event);
static void home_build_timer_cb(lv_timer_t *timer);
static void home_dispose_boot_timer_cb(lv_timer_t *timer);
static void navigation_shell_create(void);
static void navigation_leave(void *context, uintptr_t page);
static void navigation_clean(void *context, uintptr_t page);
static bool navigation_build(void *context, uintptr_t page);
static void navigation_enter(void *context, uintptr_t page);

typedef enum {
    SETTINGS_CONTROL_BRIGHTNESS = 0,
    SETTINGS_CONTROL_VOLUME,
} settings_control_t;

static np_display_sound_controls_t *active_display_controls(void)
{
    return &s_ui.display_sound.controls;
}

static np_data_state_t data_state(bool available, bool stale)
{
    if (!available) return NP_DATA_UNAVAILABLE;
    return stale ? NP_DATA_STALE : NP_DATA_LIVE;
}

static np_wifi_status_t wifi_status_for(const app_network_projection_t *network)
{
    if (network == NULL) return NP_WIFI_STATUS_OFFLINE;
    if (network->online) return NP_WIFI_STATUS_ONLINE;
    if (network->connected_ssid[0] != '\0') {
        return NP_WIFI_STATUS_ASSOCIATED_PENDING_IP;
    }
    switch (network->state) {
    case APP_NETWORK_STATE_ASSOCIATING:
    case APP_NETWORK_STATE_WAITING_FOR_IP:
        return NP_WIFI_STATUS_CONNECTING;
    case APP_NETWORK_STATE_FAILED:
        return NP_WIFI_STATUS_FAILED;
    default:
        return NP_WIFI_STATUS_OFFLINE;
    }
}

static void settings_set_percent(lv_obj_t *slider, lv_obj_t *value_label,
                                 uint8_t percent)
{
    if (slider != NULL) {
        lv_slider_set_value(slider, percent, LV_ANIM_OFF);
    }
    if (value_label != NULL) {
        char text[8] = {0};
        (void)snprintf(text, sizeof(text), "%u%%", (unsigned int)percent);
        np_set_text(value_label, text);
    }
}

static void settings_show_value_bubble(lv_obj_t *slider, lv_obj_t *bubble,
                                       lv_obj_t *bubble_value, uint8_t percent)
{
    if (slider == NULL || bubble == NULL || bubble_value == NULL) return;
    char text[8] = {0};
    (void)snprintf(text, sizeof(text), "%u%%", (unsigned int)percent);
    np_set_text(bubble_value, text);
    int32_t x = lv_obj_get_x(slider) +
                ((lv_obj_get_width(slider) - 20) * percent) / 100 - 18;
    if (x < 84) x = 84;
    if (x > 442) x = 442;
    lv_obj_set_pos(bubble, x, lv_obj_get_y(slider) - 34);
    np_set_visible(bubble, true);
    if (s_ui.settings_value_bubble_timer != NULL) {
        lv_timer_reset(s_ui.settings_value_bubble_timer);
        lv_timer_resume(s_ui.settings_value_bubble_timer);
    }
}

static void settings_value_bubble_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    np_set_visible(active_display_controls()->brightness_bubble, false);
    np_set_visible(active_display_controls()->volume_bubble, false);
    lv_timer_pause(s_ui.settings_value_bubble_timer);
}

static void settings_control_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_RELEASED &&
        lv_event_get_code(event) != LV_EVENT_PRESS_LOST) return;

    lv_obj_t *const slider = lv_event_get_target(event);
    const uint8_t percent = (uint8_t)lv_slider_get_value(slider);
    const settings_control_t control =
        (settings_control_t)(uintptr_t)lv_event_get_user_data(event);

    if (control == SETTINGS_CONTROL_BRIGHTNESS) {
        if (device_control_set_brightness(percent) != ESP_OK) {
            ESP_LOGW(TAG, "Brightness request unavailable");
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                                   "Brilho nao alterado", NULL, 2500U);
            settings_set_percent(active_display_controls()->brightness_slider,
                                 active_display_controls()->brightness_value,
                                 s_ui.projected_brightness);
            return;
        }
    } else {
        if (device_control_set_volume(percent) != ESP_OK) {
            ESP_LOGW(TAG, "Volume request unavailable");
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                                   "Volume nao alterado", NULL, 2500U);
            settings_set_percent(active_display_controls()->volume_slider,
                                 active_display_controls()->volume_value,
                                 s_ui.projected_volume);
            return;
        }
        /* The control worker applies this queued volume before the chime.
         * This is a local control preview, independent of notification
         * delivery preferences, and never runs audio from the LVGL task. */
        (void)device_control_play_notification_tone();
    }

    if (control == SETTINGS_CONTROL_BRIGHTNESS) {
        settings_set_percent(NULL, active_display_controls()->brightness_value, percent);
        settings_show_value_bubble(active_display_controls()->brightness_slider,
                                   active_display_controls()->brightness_bubble,
                                   active_display_controls()->brightness_bubble_value,
                                   percent);
    } else {
        settings_set_percent(NULL, active_display_controls()->volume_value, percent);
        settings_show_value_bubble(active_display_controls()->volume_slider,
                                   active_display_controls()->volume_bubble,
                                   active_display_controls()->volume_bubble_value,
                                   percent);
    }
}

static void install_notification_callbacks(np_settings_notifications_t *notifications)
{
    if (s_ui.notification_callbacks_initialized || notifications->general_switch == NULL) return;
    lv_obj_add_event_cb(notifications->general_switch, notification_switch_event_cb,
                        LV_EVENT_VALUE_CHANGED, (void *)0U);
    lv_obj_add_event_cb(notifications->sound_switch, notification_switch_event_cb,
                        LV_EVENT_VALUE_CHANGED, (void *)1U);
    lv_obj_add_event_cb(notifications->system_switch, notification_switch_event_cb,
                        LV_EVENT_VALUE_CHANGED, (void *)2U);
    if (notifications->test_button != NULL) {
        lv_obj_add_event_cb(notifications->test_button, notifications_test_event_cb,
                            LV_EVENT_CLICKED, NULL);
    }
    s_ui.notification_callbacks_initialized = true;
}

static void install_display_sound_callbacks(void)
{
    if (s_ui.display_sound_callbacks_initialized) return;
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    s_ui.projected_brightness = projection.device_controls.brightness_percent;
    s_ui.projected_volume = projection.device_controls.volume_percent;
    s_ui.display_sound_callbacks_initialized = true;
    lv_obj_add_event_cb(active_display_controls()->night_switch, night_switch_event_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
    settings_set_percent(active_display_controls()->brightness_slider,
                         active_display_controls()->brightness_value,
                         s_ui.projected_brightness);
    settings_set_percent(active_display_controls()->volume_slider,
                         active_display_controls()->volume_value,
                         s_ui.projected_volume);
    if (s_ui.settings_value_bubble_timer == NULL) {
        s_ui.settings_value_bubble_timer = lv_timer_create(settings_value_bubble_timer_cb, 1000U, NULL);
        if (s_ui.settings_value_bubble_timer != NULL) lv_timer_pause(s_ui.settings_value_bubble_timer);
    }
    if (active_display_controls()->brightness_slider != NULL) {
        lv_obj_add_event_cb(active_display_controls()->brightness_slider, settings_control_event_cb,
                            LV_EVENT_RELEASED, (void *)(uintptr_t)SETTINGS_CONTROL_BRIGHTNESS);
        lv_obj_add_event_cb(active_display_controls()->brightness_slider, settings_control_event_cb,
                            LV_EVENT_PRESS_LOST, (void *)(uintptr_t)SETTINGS_CONTROL_BRIGHTNESS);
    }
    if (active_display_controls()->volume_slider != NULL) {
        lv_obj_add_event_cb(active_display_controls()->volume_slider, settings_control_event_cb,
                            LV_EVENT_RELEASED, (void *)(uintptr_t)SETTINGS_CONTROL_VOLUME);
        lv_obj_add_event_cb(active_display_controls()->volume_slider, settings_control_event_cb,
                            LV_EVENT_PRESS_LOST, (void *)(uintptr_t)SETTINGS_CONTROL_VOLUME);
    }
}

static void discard_system_confirmation(void)
{
    s_ui.system_confirmation_pending = false;
    if (s_ui.system_confirmation.modal.scrim != NULL) {
        lv_obj_delete(s_ui.system_confirmation.modal.scrim);
        s_ui.system_confirmation = (np_confirm_t){0};
    }
}

static bool system_restart_confirmed(void *user_data)
{
    (void)user_data;
    np_feedback_bring_to_front(&s_ui.feedback);
    const esp_err_t result = flash_coordinator_request_restart();
    if (result != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               "Reinicio indisponivel",
                               "Aguarde o salvamento ou a manutencao terminar", 2600U);
        return false;
    }
    (void)app_state_request_refresh();
    return true;
}

static void system_restart_open_async(void *user_data)
{
    (void)user_data;
    s_ui.system_confirmation_pending = false;
    lv_obj_t *parent = s_ui.active_screen == PRODUCT_SCREEN_SYSTEM
                           ? s_ui.system_scene.root : NULL;
    if (parent == NULL) return;
    discard_system_confirmation();
    np_confirm_create(&s_ui.system_confirmation, parent,
                       "Reiniciar painel?", "O painel sera reiniciado.\nSuas configuracoes serao mantidas.",
                       "Reiniciar", system_restart_confirmed, NULL);
}

static void system_restart_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_ui.system_confirmation_pending) return;
    s_ui.system_confirmation_pending = true;
    if (lv_async_call(system_restart_open_async, NULL) != LV_RESULT_OK) {
        s_ui.system_confirmation_pending = false;
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Confirmacao indisponivel", NULL, 2200U);
    }
}

static void notification_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
    if (s_ui.syncing_notification_controls) return;
    lv_obj_t *sw = lv_event_get_target(event);
    const bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    const uintptr_t type = (uintptr_t)lv_event_get_user_data(event);
    esp_err_t result = type == 0U ? notification_service_set_general_enabled(enabled) :
                       type == 1U ? notification_service_set_sound_enabled(enabled) :
                                    notification_service_set_system_alerts_enabled(enabled);
    if (result == ESP_OK) {
        const char *const title = type == 0U
                                      ? (enabled ? "Notificacoes ativadas"
                                                 : "Notificacoes silenciadas")
                                      : type == 1U
                                            ? (enabled ? "Som das notificacoes ativado"
                                                       : "Som das notificacoes desativado")
                                            : (enabled ? "Alertas do sistema ativados"
                                                       : "Alertas do sistema desativados");
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               title, "Salvando preferencias", 1600U);
    } else {
        if (enabled) {
            lv_obj_remove_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Preferencia nao alterada",
                               "Tente novamente em alguns instantes", 3500U);
    }
}

static void notifications_test_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (notification_service_request_alert_sound() == ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               "Som de notificacao",
                               "Reproduzindo no volume atual", 1800U);
    } else {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Som indisponivel",
                               "Ative notificacoes, som e o volume", 2800U);
    }
}

static esp_err_t timezone_select_cb(void *user_data, uint16_t timezone_index)
{
    (void)user_data;
    const esp_err_t result = onboarding_service_request_timezone_update(timezone_index);
    if (result != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Fuso nao alterado",
                               "Preferencia indisponivel no momento", 2600U);
        return result;
    }

    (void)app_state_request_refresh();
    np_keyboard_hide(&s_ui.keyboard);
    np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                           "Fuso horario atualizado",
                           "Aplicando agora e salvando preferencia", 2200U);
    return ESP_OK;
}

static np_settings_wifi_t *active_wifi(void)
{
    return &s_ui.wifi_scene.wifi;
}

static lv_obj_t *wifi_dialog_parent(void)
{
    return s_ui.active_screen == PRODUCT_SCREEN_WIFI ? s_ui.wifi_scene.root : NULL;
}

static void discard_wifi_add(void)
{
    if (s_ui.wifi_add.modal.scrim != NULL) {
        np_modal_hide(&s_ui.wifi_add.modal);
        lv_obj_delete(s_ui.wifi_add.modal.scrim);
    }
    s_ui.wifi_add = (np_settings_wifi_add_t){0};
}

static void discard_wifi_password(void)
{
    np_keyboard_hide(&s_ui.keyboard);
    provisioning_service_touch_cancel();
    if (s_ui.wifi_password.modal.scrim != NULL) {
        lv_obj_delete(s_ui.wifi_password.modal.scrim);
    }
    s_ui.wifi_password = (np_wifi_password_t){0};
}

static void discard_wifi_confirmation(void)
{
    if (s_ui.wifi_confirmation.modal.scrim != NULL) {
        lv_obj_delete(s_ui.wifi_confirmation.modal.scrim);
    }
    s_ui.wifi_confirmation = (np_confirm_t){0};
}

static void sync_wifi_password(void)
{
    provisioning_service_status_t status = {0};
    provisioning_service_get_status(&status);
    np_wifi_password_sync(&s_ui.wifi_password, status.touch_password_length,
                           status.touch_password_visible);
}

static bool wifi_password_action(np_wifi_password_action_t action, char character)
{
    esp_err_t result = ESP_OK;
    switch (action) {
    case NP_WIFI_PASSWORD_APPEND:
        result = provisioning_service_touch_append_password(character);
        break;
    case NP_WIFI_PASSWORD_BACKSPACE:
        result = provisioning_service_touch_backspace_password();
        break;
    case NP_WIFI_PASSWORD_REVEAL:
        result = provisioning_service_touch_set_password_visible(character != 0);
        break;
    case NP_WIFI_PASSWORD_CANCEL:
        provisioning_service_touch_cancel();
        break;
    case NP_WIFI_PASSWORD_SUBMIT:
        result = provisioning_service_touch_submit();
        if (result == ESP_OK) {
            np_feedback_bring_to_front(&s_ui.feedback);
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                                   "Conexao solicitada", "Aguardando a rede", 2400U);
        } else {
            provisioning_service_status_t status = {0};
            provisioning_service_get_status(&status);
            /* A rejected mailbox transfer has already wiped the session. */
            if (!status.touch_active) {
                (void)provisioning_service_touch_begin_for_network(
                    lv_label_get_text(s_ui.wifi_password.modal.subtitle),
                    s_ui.wifi_password.secure);
            }
        }
        break;
    }
    sync_wifi_password();
    if (result != ESP_OK) {
        np_feedback_bring_to_front(&s_ui.feedback);
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               action == NP_WIFI_PASSWORD_SUBMIT ? "Conexao nao solicitada" : "Entrada recusada",
                               "Confira a senha ou tente novamente", 2400U);
    }
    return result == ESP_OK;
}

static void wifi_password_draw(lv_event_t *event)
{
    if (!np_modal_is_visible(&s_ui.wifi_password.modal)) return;
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target(event), &area);
    lv_layer_t *const layer = lv_event_get_layer(event);
    provisioning_service_status_t status = {0};
    provisioning_service_get_status(&status);
    /* Draw individual glyphs only. No password string, textarea or label;
     * each descriptor lives only for the current rendering pass. */
    const int32_t advance = 12;
    /* Reserve the visibility action at the field's trailing edge. */
    const uint8_t max_visible = (uint8_t)((lv_area_get_width(&area) - 80) / advance);
    const uint8_t first = status.touch_password_length > max_visible ?
                          status.touch_password_length - max_visible : 0U;
    const int32_t baseline = area.y1 +
                             (lv_area_get_height(&area) - NP_FONT_SM->line_height) / 2;
    for (uint8_t i = first; i < status.touch_password_length && i < 63U; ++i) {
        lv_draw_letter_dsc_t glyph;
        lv_draw_letter_dsc_init(&glyph);
        glyph.font = NP_FONT_SM;
        glyph.color = np_c_text();
        glyph.unicode = provisioning_service_touch_display_character(i);
        if (glyph.unicode == 0U) break;
        const lv_point_t point = {.x = area.x1 + 16 + (i - first) * advance,
                                  .y = baseline};
        lv_draw_letter(layer, &glyph, &point);
        glyph.unicode = 0U;
    }
}

static void wifi_password_open_async(void *user_data)
{
    (void)user_data;
    s_ui.wifi_password_pending = false;
    lv_obj_t *const parent = wifi_dialog_parent();
    if (parent == NULL || s_ui.navigation_pending) return;
    discard_wifi_add();
    discard_wifi_password();
    discard_wifi_confirmation();
    const esp_err_t result = provisioning_service_touch_begin_for_network(
        s_ui.pending_wifi_ssid, s_ui.pending_wifi_secure);
    if (result == ESP_OK) {
        np_keyboard_hide(&s_ui.keyboard);
        np_wifi_password_create(&s_ui.wifi_password, parent,
                                 &s_ui.keyboard,
                                 s_ui.pending_wifi_ssid, s_ui.pending_wifi_secure,
                                 wifi_password_action, wifi_password_draw);
    } else {
        np_feedback_bring_to_front(&s_ui.feedback);
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Configuracao indisponivel", NULL, 2400U);
    }
    s_ui.pending_wifi_ssid[0] = '\0';
}

static void wifi_add_submit_event_cb(lv_event_t *event)
{
    (void)event;
    const char *ssid = lv_textarea_get_text(s_ui.wifi_add.ssid);
    const size_t length = strnlen(ssid, sizeof(s_ui.pending_wifi_ssid));
    if (length == 0U || length >= sizeof(s_ui.pending_wifi_ssid)) {
        np_feedback_bring_to_front(&s_ui.feedback);
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
            "Informe um nome de rede", "O SSID deve ter até 32 bytes", 2400U);
        return;
    }
    if (s_ui.wifi_password_pending || s_ui.navigation_pending) return;
    memcpy(s_ui.pending_wifi_ssid, ssid, length + 1U);
    s_ui.pending_wifi_secure = lv_obj_has_state(s_ui.wifi_add.secure_switch, LV_STATE_CHECKED);
    s_ui.wifi_password_pending = true;
    if (lv_async_call(wifi_password_open_async, NULL) == LV_RESULT_OK) {
        np_modal_hide(&s_ui.wifi_add.modal);
    } else {
        s_ui.wifi_password_pending = false;
        s_ui.pending_wifi_ssid[0] = '\0';
    }
}

static void wifi_add_open_async(void *user_data)
{
    (void)user_data;
    s_ui.wifi_add_pending = false;
    if (s_ui.active_screen != PRODUCT_SCREEN_WIFI || s_ui.navigation_pending) return;
    discard_wifi_add();
    discard_wifi_password();
    discard_wifi_confirmation();
    np_settings_wifi_add_create(&s_ui.wifi_add, s_ui.wifi_scene.root, &s_ui.keyboard);
    lv_obj_add_event_cb(s_ui.wifi_add.continue_button, wifi_add_submit_event_cb, LV_EVENT_CLICKED, NULL);
}

static void wifi_add_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_ui.wifi_add_pending || s_ui.navigation_pending) return;
    s_ui.wifi_add_pending = true;
    if (lv_async_call(wifi_add_open_async, NULL) != LV_RESULT_OK) s_ui.wifi_add_pending = false;
}

static void wifi_manage_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (lv_event_get_target(event) == active_wifi()->manage_button) {
        wifi_scan_event_cb(event);
        return;
    }
    if (s_ui.wifi_password_pending ||
        !np_settings_wifi_copy_selected_ssid(active_wifi(),
                                             s_ui.pending_wifi_ssid,
                                             sizeof(s_ui.pending_wifi_ssid))) return;
    s_ui.pending_wifi_secure = active_wifi()->network_secure[active_wifi()->selected_index];
    s_ui.wifi_password_pending = true;
    if (lv_async_call(wifi_password_open_async, NULL) != LV_RESULT_OK) {
        s_ui.wifi_password_pending = false;
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Configuracao indisponivel", NULL, 2600U);
    }
}

static void wifi_scan_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (connectivity_diagnostic_request_scan() == ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               "Buscando redes", NULL, 1800U);
    } else {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Busca indisponivel", NULL, 2200U);
    }
}

static bool wifi_forget_confirmed(void *user_data)
{
    (void)user_data;
    np_feedback_bring_to_front(&s_ui.feedback);
    if (connectivity_diagnostic_request_forget() == ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               "Remocao solicitada", "Aguardando o servico", 2200U);
        return true;
    } else {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Nao foi possivel esquecer", NULL, 2200U);
        return false;
    }
}

static void wifi_forget_open_async(void *user_data)
{
    (void)user_data;
    s_ui.wifi_confirmation_pending = false;
    lv_obj_t *const parent = wifi_dialog_parent();
    if (parent == NULL || s_ui.navigation_pending) return;
    discard_wifi_password();
    discard_wifi_confirmation();
    np_confirm_create(&s_ui.wifi_confirmation, parent,
                        "Esquecer rede?",
                        "A conexao sera encerrada e a credencial salva removida.\n"
                        "Para reconectar, sera preciso configurar a rede novamente.",
                        "Esquecer rede", wifi_forget_confirmed, NULL);
}

static void wifi_forget_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_ui.wifi_confirmation_pending) return;
    s_ui.wifi_confirmation_pending = true;
    if (lv_async_call(wifi_forget_open_async, NULL) != LV_RESULT_OK) {
        s_ui.wifi_confirmation_pending = false;
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Confirmacao indisponivel", NULL, 2200U);
    }
}

static bool projection_local_time(const app_ui_projection_t *projection, struct tm *local)
{
    if (projection == NULL || local == NULL ||
        !projection->time_trusted || projection->current_unix_s == 0U) {
        return false;
    }

    const time_t now = (time_t)projection->current_unix_s;
    return localtime_r(&now, local) != NULL;
}

static void format_grouped_u32(uint32_t value, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0U) return;

    if (value >= 1000000U) {
        (void)snprintf(out, out_size, "%lu.%03lu.%03lu",
                       (unsigned long)(value / 1000000U),
                       (unsigned long)((value / 1000U) % 1000U),
                       (unsigned long)(value % 1000U));
    } else if (value >= 1000U) {
        (void)snprintf(out, out_size, "%lu.%03lu",
                       (unsigned long)(value / 1000U),
                       (unsigned long)(value % 1000U));
    } else {
        (void)snprintf(out, out_size, "%lu", (unsigned long)value);
    }
}

static void format_btc_whole(uint32_t cents, char *out, size_t out_size)
{
    format_grouped_u32(cents / 100U, out, out_size);
}

static void format_basis_points(int16_t basis_points, char *out, size_t out_size)
{
    char value[16] = {0};

    if (!offline_format_market_change(basis_points, value, sizeof(value))) {
        (void)snprintf(out, out_size, "--");
        return;
    }

    char *separator = strchr(value, '.');
    if (separator != NULL) *separator = ',';

    (void)snprintf(out, out_size, "%s%s%%",
                   basis_points > 0 ? "+" : "", value);
}

#if OFFLINE_DATA_SCHEMA_VERSION >= 4

static void format_temperature_deci(int16_t deci_c, char *out, size_t out_size)
{
    char value[16] = {0};
    if (!offline_format_temperature(deci_c, value, sizeof(value))) {
        (void)snprintf(out, out_size, "--");
        return;
    }

    char *separator = strchr(value, '.');
    if (separator != NULL) *separator = ',';
    (void)snprintf(out, out_size, "%s°", value);
}

static void format_usd_metric(uint32_t cents, char *out, size_t out_size)
{
    char whole[20] = {0};
    format_btc_whole(cents, whole, sizeof(whole));
    (void)snprintf(out, out_size, "%s", whole);
}

static void format_volume_short(uint64_t cents, char *out, size_t out_size)
{
    const uint64_t dollars = cents / 100U;

    if (dollars >= UINT64_C(1000000000)) {
        const uint64_t whole = dollars / UINT64_C(1000000000);
        const uint64_t hundredths =
            (dollars % UINT64_C(1000000000)) / UINT64_C(10000000);
        (void)snprintf(out, out_size, "%llu,%02llu bi",
                       (unsigned long long)whole,
                       (unsigned long long)hundredths);
    } else if (dollars >= UINT64_C(1000000)) {
        const uint64_t whole = dollars / UINT64_C(1000000);
        const uint64_t hundredths =
            (dollars % UINT64_C(1000000)) / UINT64_C(10000);
        (void)snprintf(out, out_size, "%llu,%02llu mi",
                       (unsigned long long)whole,
                       (unsigned long long)hundredths);
    } else {
        (void)snprintf(out, out_size, "%llu",
                       (unsigned long long)dollars);
    }
}

#endif

/* ------------------------------------------------------------------ */
/* Header                                                              */
/* ------------------------------------------------------------------ */

static void update_header(np_header_t *header,
                          const app_ui_projection_t *projection,
                          bool settings_active)
{
    if (header == NULL || projection == NULL) return;

    const char *const name = projection->user_profile.profile.name;
    const bool has_name = projection->user_profile.ready &&
                          projection->user_profile.configured && name[0] != '\0';
    if (has_name) {
        char first_name[USER_PROFILE_NAME_BYTES] = {0};
        size_t length = 0U;
        while (length + 1U < sizeof(first_name) && name[length] != '\0' &&
               name[length] != ' ') {
            first_name[length] = name[length];
            ++length;
        }
        const char *salutation = "Olá";
        struct tm local = {0};
        if (projection_local_time(projection, &local)) {
            salutation = local.tm_hour < 12 ? "Bom dia" :
                         local.tm_hour < 18 ? "Boa tarde" : "Boa noite";
        }
        char greeting[USER_PROFILE_NAME_BYTES + 16U] = {0};
        (void)snprintf(greeting, sizeof(greeting), "%s, %s", salutation, first_name);
        np_set_text(header->brand_nova, greeting);
        if (lv_obj_get_width(header->brand_nova) != 420) {
            lv_label_set_long_mode(header->brand_nova, LV_LABEL_LONG_DOT);
            lv_obj_set_width(header->brand_nova, 420);
        }
        np_set_visible(header->brand_panel, false);
        np_set_text(header->user_greeting, "");
        np_set_visible(header->user_greeting, false);
    } else {
        np_set_text(header->brand_nova, "Nova");
        if (lv_obj_get_width(header->brand_nova) != 64) {
            lv_label_set_long_mode(header->brand_nova, LV_LABEL_LONG_DOT);
            lv_obj_set_width(header->brand_nova, 64);
        }
        np_set_visible(header->brand_panel, true);
        np_set_text(header->user_greeting, "");
        np_set_visible(header->user_greeting, false);
    }

    int8_t connected_rssi = 0;
    bool connected_rssi_measured = false;
    for (uint8_t i = 0; i < projection->network.scan_results_count; ++i) {
        const connectivity_scan_result_t *const network = &projection->network.scan_results[i];
        if (projection->network.connected_ssid[0] != '\0' &&
            strcmp(network->ssid, projection->network.connected_ssid) == 0) {
            connected_rssi = network->rssi;
            connected_rssi_measured = true;
            break;
        }
    }

    np_header_set_drawer_active(header, settings_active);
    np_header_set_connections(
        header,
        projection->network.online,
        connected_rssi,
        connected_rssi_measured,
        false,
        projection->network.state == APP_NETWORK_STATE_FAILED ||
            projection->storage.last_result != ESP_OK);
    np_header_set_notifications_enabled(header,
                                        projection->notifications.general_enabled);

    struct tm local = {0};
    if (!projection_local_time(projection, &local)) {
        np_clock_set(&header->clock, "--:--");
        return;
    }

    char clock[6] = {0};
    (void)snprintf(clock, sizeof(clock), "%02d:%02d",
                   local.tm_hour, local.tm_min);
    np_clock_set(&header->clock, clock);
}

static void update_clock(const app_ui_projection_t *projection)
{
    struct tm local = {0};
    update_header(&s_ui.home.header, projection, false);

    if (!projection_local_time(projection, &local)) {
        np_set_text(s_ui.home.weather_date, "Data indisponível");
        return;
    }

    static const char *const weekdays_long[] = {
        "Domingo", "Segunda-feira", "Terça-feira", "Quarta-feira",
        "Quinta-feira", "Sexta-feira", "Sábado",
    };
    static const char *const months[] = {
        "janeiro", "fevereiro", "março", "abril", "maio", "junho",
        "julho", "agosto", "setembro", "outubro", "novembro", "dezembro",
    };

    char weather_date[64] = {0};
    (void)snprintf(weather_date, sizeof(weather_date), "%s, %d de %s",
                   weekdays_long[local.tm_wday], local.tm_mday,
                   months[local.tm_mon]);

    np_set_text(s_ui.home.weather_date, weather_date);
}

/* ------------------------------------------------------------------ */
/* Clima                                                               */
/* ------------------------------------------------------------------ */

static void update_weather(const app_ui_projection_t *projection)
{
    const offline_weather_data_t *weather = &projection->offline_data.weather;

    np_home_set_weather_icon_source(
        &s_ui.home, projection->weather_assets.icon_source);

    np_status_dot_set(&s_ui.home.weather_status,
                      data_state(weather->available, weather->stale));

    if (!weather->available) {
        np_set_text(s_ui.home.weather_temperature, "--°");
        np_set_text(s_ui.home.weather_place, "Brasília, DF");
        np_set_text(s_ui.home.weather_summary, "Sem dados de clima");
        np_set_text(s_ui.home.weather_wind.value, "--");
        np_set_text(s_ui.home.weather_humidity.value, "--");
        np_set_text(s_ui.home.weather_feels_like.value, "--");
        np_set_text(s_ui.home.weather_uv.value, "--");
        return;
    }

    char display_temperature[20] = {0};
    char humidity[12] = {0};

    /* A Home prioriza leitura a distância, como no protótipo: temperatura
     * inteira e grande. O dado original continua preservado no modelo. */
    const int16_t rounded_c = weather->temperature_deci_c >= 0
                                  ? (int16_t)((weather->temperature_deci_c + 5) / 10)
                                  : (int16_t)((weather->temperature_deci_c - 5) / 10);
    (void)snprintf(display_temperature, sizeof(display_temperature),
                   "%d°", (int)rounded_c);

    (void)snprintf(humidity, sizeof(humidity), "%u%%",
                   (unsigned int)weather->relative_humidity_percent);

    np_set_text(s_ui.home.weather_temperature, display_temperature);
    np_set_text(s_ui.home.weather_place, "Brasília, DF");
    np_set_text(s_ui.home.weather_summary,
                weather_condition_summary(
                    weather_condition_from_code(weather->weather_code)));
    np_set_text(s_ui.home.weather_humidity.value, humidity);

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    char feels_like[20] = {0};
    char wind[20] = {0};
    char uv[16] = {0};

    if (weather->wind_speed_available) {
        const unsigned int whole =
            (unsigned int)(weather->wind_speed_deci_kmh / 10U);
        const unsigned int decimal =
            (unsigned int)(weather->wind_speed_deci_kmh % 10U);
        (void)snprintf(wind, sizeof(wind), "%u,%u km/h", whole, decimal);
        np_set_text(s_ui.home.weather_wind.value, wind);
    } else {
        np_set_text(s_ui.home.weather_wind.value, "--");
    }

    if (weather->apparent_temperature_available) {
        format_temperature_deci(weather->apparent_temperature_deci_c,
                                feels_like, sizeof(feels_like));
        np_set_text(s_ui.home.weather_feels_like.value, feels_like);
    } else {
        np_set_text(s_ui.home.weather_feels_like.value, "--");
    }

    if (weather->uv_index_available) {
        const unsigned int whole =
            (unsigned int)(weather->uv_index_deci / 10U);
        const unsigned int decimal =
            (unsigned int)(weather->uv_index_deci % 10U);
        (void)snprintf(uv, sizeof(uv), "%u,%u", whole, decimal);
        np_set_text(s_ui.home.weather_uv.value, uv);
    } else {
        np_set_text(s_ui.home.weather_uv.value, "--");
    }
#else
    np_set_text(s_ui.home.weather_wind.value, "--");
    np_set_text(s_ui.home.weather_feels_like.value, "--");
    np_set_text(s_ui.home.weather_uv.value, "--");
#endif
}

/* ------------------------------------------------------------------ */
/* Bitcoin                                                             */
/* ------------------------------------------------------------------ */

static void clear_btc_details(void)
{
    np_set_text(s_ui.home.btc_high_24h, "--");
    np_set_text(s_ui.home.btc_low_24h, "--");
    np_set_text(s_ui.home.btc_volume_24h, "--");
    np_home_set_btc_spark(&s_ui.home, NULL, 0U, np_c_btc());
}

static void update_market(const app_ui_projection_t *projection)
{
    const offline_market_data_t *market = &projection->offline_data.market;

    np_status_dot_set(&s_ui.home.btc_status,
                      data_state(market->available, market->stale));

    if (!market->available) {
        np_set_text(s_ui.home.btc_price_prefix, "US$");
        np_set_text(s_ui.home.btc_price, "--");
        np_set_text(s_ui.home.btc_change_icon, NP_ICON_RISE);
        np_set_text(s_ui.home.btc_change, "--");
        np_set_text_color(s_ui.home.btc_change_icon, np_c_text_3());
        np_set_text_color(s_ui.home.btc_change, np_c_text_3());
        clear_btc_details();
        return;
    }

    char price[24] = {0};
    char change[20] = {0};

    format_btc_whole(market->bitcoin_usd_cents, price, sizeof(price));
    format_basis_points(market->change_24h_basis_points,
                        change, sizeof(change));

    const lv_color_t change_color =
        market->change_24h_basis_points >= 0
            ? np_c_positive()
            : np_c_negative();

    np_set_text(s_ui.home.btc_price_prefix, "US$");
    np_set_text(s_ui.home.btc_price, price);
    np_set_text(s_ui.home.btc_change_icon,
                market->change_24h_basis_points >= 0
                    ? NP_ICON_RISE
                    : NP_ICON_FALL);
    np_set_text(s_ui.home.btc_change, change);
    np_set_text_color(s_ui.home.btc_change_icon, change_color);
    np_set_text_color(s_ui.home.btc_change, change_color);

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    char high[24] = {0};
    char low[24] = {0};
    char volume[24] = {0};

    if (market->high_24h_available) {
        format_usd_metric(market->high_24h_usd_cents,
                          high, sizeof(high));
        np_set_text(s_ui.home.btc_high_24h, high);
    } else {
        np_set_text(s_ui.home.btc_high_24h, "--");
    }

    if (market->low_24h_available) {
        format_usd_metric(market->low_24h_usd_cents,
                          low, sizeof(low));
        np_set_text(s_ui.home.btc_low_24h, low);
    } else {
        np_set_text(s_ui.home.btc_low_24h, "--");
    }

    if (market->volume_24h_available) {
        format_volume_short(market->volume_24h_usd_cents,
                            volume, sizeof(volume));
        np_set_text(s_ui.home.btc_volume_24h, volume);
    } else {
        np_set_text(s_ui.home.btc_volume_24h, "--");
    }

    if (market->history_count >= 2U) {
        int32_t samples[24] = {0};
        uint8_t count = market->history_count;
        if (count > 24U) count = 24U;

        for (uint8_t i = 0U; i < count; ++i) {
            samples[i] = (int32_t)(market->history_usd_cents[i] / 100U);
        }

        /* O gráfico inteiro acompanha a direção de 24 h: verde na alta e
         * vermelho na queda. Linha e degradê recebem exatamente a mesma cor. */
        np_home_set_btc_spark(&s_ui.home,
                              samples,
                              count,
                              change_color);
    } else {
        np_home_set_btc_spark(&s_ui.home,
                              NULL, 0U, change_color);
    }
#else
    clear_btc_details();
#endif
}

/* ------------------------------------------------------------------ */
/* USD/BRL                                                             */
/* ------------------------------------------------------------------ */

static void update_exchange(const app_ui_projection_t *projection)
{
    const offline_exchange_data_t *const exchange =
        &projection->offline_data.exchange;

    np_status_dot_set(&s_ui.home.usd.status,
                      data_state(exchange->available, exchange->stale));

    if (!exchange->available) {
        np_set_text(s_ui.home.usd.value, "--");
        np_set_text(s_ui.home.usd.change_icon, "");
        np_set_text(s_ui.home.usd.change, "--");
        np_set_text_color(s_ui.home.usd.change, np_c_text_3());
        np_set_visible(s_ui.home.usd.change_icon, true);
        np_set_visible(s_ui.home.usd.change, true);
        return;
    }

    /* O contrato preserva 4 casas; a Home mostra 2 para leitura a distância. */
    const uint32_t cents =
        (exchange->usd_brl_ten_thousandths + 50U) / 100U;

    char value[24] = {0};
    (void)snprintf(value, sizeof(value), "R$ %lu,%02lu",
                   (unsigned long)(cents / 100U),
                   (unsigned long)(cents % 100U));

    np_set_text(s_ui.home.usd.value, value);
    np_set_text_color(s_ui.home.usd.value, np_c_text());

#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    if (exchange->change_available) {
        char change[20] = {0};
        format_basis_points(exchange->change_basis_points,
                            change, sizeof(change));

        const bool positive = exchange->change_basis_points >= 0;
        const lv_color_t color = positive ? np_c_positive() : np_c_negative();

        np_set_text(s_ui.home.usd.change_icon,
                    positive ? NP_ICON_RISE : NP_ICON_FALL);
        np_set_text(s_ui.home.usd.change, change);
        np_set_text_color(s_ui.home.usd.change_icon, color);
        np_set_text_color(s_ui.home.usd.change, color);
    } else {
        np_set_text(s_ui.home.usd.change_icon, "");
        np_set_text(s_ui.home.usd.change, "--");
        np_set_text_color(s_ui.home.usd.change, np_c_text_3());
    }
#else
    /* O schema v3 ainda não traz variação do USD/BRL. Mantemos o espaço\n     * visual correto sem inventar um percentual. */
    np_set_text(s_ui.home.usd.change_icon, "");
    np_set_text(s_ui.home.usd.change, "--");
    np_set_text_color(s_ui.home.usd.change, np_c_text_3());
#endif

    np_set_visible(s_ui.home.usd.change_icon, true);
    np_set_visible(s_ui.home.usd.change, true);
}

/* ------------------------------------------------------------------ */
/* Ibovespa                                                            */
/* ------------------------------------------------------------------ */

static void update_ibovespa(const app_ui_projection_t *projection)
{
#if OFFLINE_DATA_SCHEMA_VERSION >= 4
    const offline_ibovespa_data_t *const ibov =
        &projection->offline_data.ibovespa;

    np_status_dot_set(&s_ui.home.ibov.status,
                      data_state(ibov->available, ibov->stale));

    if (!ibov->available) {
        np_set_text(s_ui.home.ibov.value, "--");
        np_set_text(s_ui.home.ibov.change_icon, "");
        np_set_text(s_ui.home.ibov.change, "--");
        np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
        return;
    }

    char value[24] = {0};
    format_grouped_u32(ibov->value_centi_points / 100U,
                       value, sizeof(value));
    np_set_text(s_ui.home.ibov.value, value);

    if (ibov->change_available) {
        char change[20] = {0};
        format_basis_points(ibov->change_basis_points,
                            change, sizeof(change));

        const bool positive = ibov->change_basis_points >= 0;
        const lv_color_t color = positive ? np_c_positive() : np_c_negative();

        np_set_text(s_ui.home.ibov.change_icon,
                    positive ? NP_ICON_RISE : NP_ICON_FALL);
        np_set_text(s_ui.home.ibov.change, change);
        np_set_text_color(s_ui.home.ibov.change_icon, color);
        np_set_text_color(s_ui.home.ibov.change, color);
    } else {
        np_set_text(s_ui.home.ibov.change_icon, "");
        np_set_text(s_ui.home.ibov.change, "--");
        np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
    }
#else
    (void)projection;
    np_status_dot_set(&s_ui.home.ibov.status, NP_DATA_UNAVAILABLE);
    np_set_text(s_ui.home.ibov.value, "--");
    np_set_text(s_ui.home.ibov.change_icon, "");
    np_set_text(s_ui.home.ibov.change, "--");
    np_set_text_color(s_ui.home.ibov.change, np_c_text_3());
#endif

    np_set_visible(s_ui.home.ibov.change_icon, true);
    np_set_visible(s_ui.home.ibov.change, true);
}

/* ------------------------------------------------------------------ */
/* Boot/Home orchestration                                             */
/* ------------------------------------------------------------------ */

static uint8_t boot_progress(const app_ui_projection_t *projection)
{
    uint8_t stage = 1U;

    if (projection->storage.ready) stage = 2U;
    if (projection->network.state >= APP_NETWORK_STATE_LINK_UP) stage = 3U;
    if (projection->time_trusted) stage = 4U;
    if (projection->time_trusted && (projection->offline_data.weather.available ||
        projection->offline_data.market.available)) {
        stage = 5U;
    }

    return stage;
}

static void boot_opacity_animation(void *target, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)target, (lv_opa_t)value, 0);
}

static void boot_fade_in(lv_obj_t *object)
{
    if (object == NULL) return;

    lv_anim_del(object, boot_opacity_animation);
    lv_obj_set_style_opa(object, LV_OPA_TRANSP, 0);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&animation, BOOT_TRANSITION_MS);
    lv_anim_set_exec_cb(&animation, boot_opacity_animation);
    lv_anim_start(&animation);
}

static void update_boot(const app_ui_projection_t *projection)
{
    const uint8_t stage = boot_progress(projection);

    static const char *const status[] = {
        "Preparando o display",
        "Verificando o armazenamento",
        "Procurando redes Wi-Fi",
        "Atualizando data e hora",
        "Carregando os dados do painel",
        "Tudo pronto",
    };

    if (stage != s_ui.boot_rendered_stage) {
        if (stage > s_ui.boot_rendered_stage) {
            for (uint8_t index = s_ui.boot_rendered_stage;
                 index < stage && index < s_ui.boot.progress.count; ++index) {
                np_set_bg_color(s_ui.boot.progress.seg[index], np_c_accent());
                boot_fade_in(s_ui.boot.progress.seg[index]);
            }
        } else {
            np_segbar_set(&s_ui.boot.progress, stage, np_c_accent());
        }
        np_set_text(s_ui.boot.status, status[stage]);
        boot_fade_in(s_ui.boot.status);
        s_ui.boot_rendered_stage = stage;
    }

    const char *detail = NULL;
    if (stage < 2U) {
        detail = "Preparando os serviços do painel.";
    } else if (stage < 3U && projection->network.credentials_active) {
        detail = "Conectando à rede Wi-Fi salva.";
    } else if (stage < 3U) {
        detail = "Procurando redes Wi-Fi disponíveis.";
    } else if (stage < 4U && projection->network.online) {
        detail = "Conexão pronta; sincronizando data e hora.";
    } else if (stage < 4U) {
        detail = "A inicialização continua mesmo sem internet.";
    } else if (stage < 5U) {
        detail = "Hora atualizada; preparando os dados do painel.";
    } else {
        detail = "Tudo pronto.";
    }
    if (strcmp(lv_label_get_text(s_ui.boot.detail), detail) != 0) {
        np_set_text(s_ui.boot.detail,
                    detail);
        boot_fade_in(s_ui.boot.detail);
    }
}

static void update_home(const app_ui_projection_t *projection)
{
    update_clock(projection);

    /* The app projection changes every second with the clock and data age.
     * Redraw the large cards only when their displayed data or icon changes. */
    if (!s_ui.home_data_rendered ||
        memcmp(&s_ui.rendered_home_data, &projection->offline_data,
               sizeof(projection->offline_data)) != 0 ||
        s_ui.rendered_weather_icon_source != projection->weather_assets.icon_source) {
        update_weather(projection);
        update_market(projection);
        update_exchange(projection);
        update_ibovespa(projection);
        s_ui.rendered_home_data = projection->offline_data;
        s_ui.rendered_weather_icon_source = projection->weather_assets.icon_source;
        s_ui.home_data_rendered = true;
    }

    if (projection->notifications.ready) {
        if (!s_ui.notification_feedback_initialized) {
            s_ui.notified_persisted_generation =
                projection->notifications.persisted_generation;
            s_ui.notification_feedback_initialized = true;
        } else if (!projection->notifications.persistence_pending &&
                   projection->notifications.persisted_generation != 0U &&
                   projection->notifications.persisted_generation !=
                       s_ui.notified_persisted_generation) {
            s_ui.notified_persisted_generation =
                projection->notifications.persisted_generation;
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_SUCCESS,
                                   "Preferencias salvas", NULL, 1800U);
        }
    }
}

static void update_display_sound_controls(const app_ui_projection_t *projection)
{
    if (s_ui.display_sound_callbacks_initialized && projection->device_controls.ready) {
        s_ui.syncing_night_control = true;
        if (projection->device_controls.night_mode_enabled) {
            lv_obj_add_state(active_display_controls()->night_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(active_display_controls()->night_switch, LV_STATE_CHECKED);
        }
        s_ui.syncing_night_control = false;
        np_set_text(active_display_controls()->night_detail,
                    projection->device_controls.effective_brightness_percent !=
                        (projection->device_controls.night_mode_active &&
                         projection->device_controls.brightness_percent > 15U
                             ? 15U : projection->device_controls.brightness_percent)
                        ? "Nao foi possivel aplicar o brilho"
                        : !projection->device_controls.night_mode_enabled
                        ? "22:00 - 06:00 · brilho ate 15%"
                        : !projection->time_trusted ? "Aguardando horario confiavel"
                        : projection->device_controls.night_mode_active
                            ? "Ativo · brilho limitado a 15%"
                            : "22:00 - 06:00 · programado");
        if (projection->device_controls.brightness_percent != s_ui.projected_brightness) {
            s_ui.projected_brightness = projection->device_controls.brightness_percent;
            settings_set_percent(active_display_controls()->brightness_slider,
                                 active_display_controls()->brightness_value,
                                 s_ui.projected_brightness);
        }
        if (projection->device_controls.volume_percent != s_ui.projected_volume) {
            s_ui.projected_volume = projection->device_controls.volume_percent;
            settings_set_percent(active_display_controls()->volume_slider,
                                 active_display_controls()->volume_value,
                                 s_ui.projected_volume);
        }
    }

}

static void update_system_information(np_settings_system_t *system,
                                       const app_ui_projection_t *projection)
{
    char chip_temperature[24] = "Nao disponivel";
    if (projection->system.temperature_available) {
        format_temperature_deci(projection->system.chip_temperature_deci_c,
                                  chip_temperature, sizeof(chip_temperature));
    }
    np_settings_system_sync(system, projection->system.firmware_version,
                             chip_temperature, projection->system.restart_pending);
}

static void update_notification_controls(np_settings_notifications_t *notifications,
                                          const app_ui_projection_t *projection)
{
    if (!projection->notifications.ready) return;
    s_ui.syncing_notification_controls = true;
    np_settings_notifications_sync(notifications,
                                   projection->notifications.general_enabled,
                                   projection->notifications.sound_enabled,
                                   projection->notifications.system_alerts_enabled);
    s_ui.syncing_notification_controls = false;
}

static void update_notifications_scene(const app_ui_projection_t *projection)
{
    np_settings_notifications_t *notifications = &s_ui.notifications_scene.notifications;
    update_header(&s_ui.notifications_scene.header, projection, true);
    update_notification_controls(notifications, projection);
    lv_obj_t *controls[] = {notifications->general_switch, notifications->sound_switch,
                            notifications->system_switch, notifications->test_button};
    for (unsigned i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        if (projection->notifications.ready) lv_obj_remove_state(controls[i], LV_STATE_DISABLED);
        else lv_obj_add_state(controls[i], LV_STATE_DISABLED);
    }
    np_set_text(s_ui.notifications_scene.persistence_status,
        !projection->notifications.ready ? "Carregando preferências..."
        : projection->notifications.persistence_pending ? "Salvando preferências..."
        : projection->notifications.last_result != ESP_OK ? "Não foi possível ler ou salvar as preferências"
        : projection->notifications.persisted_generation != 0U ? "Preferências salvas automaticamente"
        : "Salvamento automático ao alterar");
}

static void update_timezone_scene(const app_ui_projection_t *projection)
{
    update_header(&s_ui.timezone_scene.header, projection, true);
    np_settings_timezone_scene_sync(&s_ui.timezone_scene,
        projection->onboarding.timezone_index,
        projection->onboarding.timezone_persistence_pending,
        projection->onboarding.last_result, projection->time_trusted);
}

static void update_wifi_scene(const app_ui_projection_t *projection)
{
    update_header(&s_ui.wifi_scene.header, projection, true);
    np_settings_wifi_scene_sync(&s_ui.wifi_scene, wifi_status_for(&projection->network),
        projection->network.connected_ssid, projection->network.scan_results_count,
        projection->network.scan_results, projection->network.ip_address,
        projection->network.gateway, projection->network.state == APP_NETWORK_STATE_SCANNING);
    if (projection->network.state == APP_NETWORK_STATE_SCAN_COMPLETE &&
        projection->network.last_result != ESP_OK) {
        np_set_text(s_ui.wifi_scene.page_label, "Busca falhou · tente novamente");
    }
}

static void night_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED || s_ui.syncing_night_control) return;
    const bool enabled = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    if (device_control_set_night_mode(enabled) != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Modo noturno indisponivel", NULL, 2200U);
    }
    (void)app_state_request_refresh();
}

static void install_home_navigation_callbacks(void);

static void navigation_shell_create(void)
{
    if (s_ui.shell != NULL) return;
    s_ui.shell = np_scene(lv_screen_active());
    s_ui.content_host = np_scene(s_ui.shell);
    s_ui.shell_header = np_header(s_ui.shell);
    lv_obj_add_event_cb(s_ui.shell_header.drawer_home_button,
                        scene_navigation_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PRODUCT_SCREEN_HOME);
    lv_obj_add_event_cb(s_ui.shell_header.drawer_settings_button,
                        scene_navigation_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
    lv_obj_add_event_cb(s_ui.shell_header.settings_button,
                        scene_navigation_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PRODUCT_SCREEN_PROFILE);
    np_set_visible(s_ui.shell, false);
}

static void home_dispose_boot_timer_cb(lv_timer_t *timer)
{
    if (s_ui.boot.root != NULL) lv_obj_delete(s_ui.boot.root);
    s_ui.boot = (np_boot_view_t){0};
    s_ui.home_transition_timer = NULL;
    s_ui.home_transition_pending = false;
    lv_timer_delete(timer);
    ESP_LOGI(TAG, "Boot transition complete; Home V2 visible");
}

static void home_build_timer_cb(lv_timer_t *timer)
{
    s_ui.home_transition_timer = NULL;
    lv_timer_delete(timer);
    if (s_ui.active_screen != PRODUCT_SCREEN_BOOT || s_ui.boot.root == NULL) {
        s_ui.home_transition_pending = false;
        return;
    }

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    navigation_shell_create();
    s_ui.home = np_home_build_with_header(s_ui.content_host, &s_ui.shell_header);
    s_ui.home_data_rendered = false;
    np_feedback_bring_to_front(&s_ui.feedback);
    update_home(&projection);
    s_ui.active_screen = PRODUCT_SCREEN_HOME;
    s_ui.navigation.current_page = PRODUCT_SCREEN_HOME;
    s_ui.navigation.destination = PRODUCT_SCREEN_HOME;
    ++s_ui.navigation.page_generation;
    s_ui.rendered_revision = projection.revision;
    np_set_visible(s_ui.shell, true);
    lv_obj_move_foreground(s_ui.shell);
    np_feedback_bring_to_front(&s_ui.feedback);

    /* Deleting the old tree is intentionally deferred to a later LVGL pass.
     * Building Home and freeing the boot scene together can starve IDLE0. */
    s_ui.home_transition_timer =
        lv_timer_create(home_dispose_boot_timer_cb, 32U, NULL);
    if (s_ui.home_transition_timer == NULL) {
        if (s_ui.boot.root != NULL) lv_obj_delete(s_ui.boot.root);
        s_ui.boot = (np_boot_view_t){0};
        s_ui.home_transition_pending = false;
        ESP_LOGW(TAG, "Boot scene disposal could not be deferred");
    }
}

static void show_home(const app_ui_projection_t *projection)
{
    (void)projection;
    if (s_ui.active_screen != PRODUCT_SCREEN_BOOT || s_ui.home_transition_pending) return;

    s_ui.home_transition_pending = true;
    s_ui.home_transition_timer = lv_timer_create(home_build_timer_cb, 32U, NULL);
    if (s_ui.home_transition_timer == NULL) {
        s_ui.home_transition_pending = false;
        ESP_LOGE(TAG, "Home transition timer allocation failed");
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);

    if (projection.system.restart_completion_id != s_ui.restart_completion_id) {
        s_ui.restart_completion_id = projection.system.restart_completion_id;
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Reinicio adiado", "Uma operacao ainda esta em andamento", 2600U);
    }
    if (projection.device_controls.save_completion_id !=
        s_ui.control_save_completion_id) {
        s_ui.control_save_completion_id = projection.device_controls.save_completion_id;
        const uint8_t mask = projection.device_controls.save_completion_mask;
        const char *const title = (mask & DEVICE_CONTROL_NIGHT_MASK) != 0U
                                      ? "Preferencias da tela salvas"
                                      : (mask & (DEVICE_CONTROL_BRIGHTNESS_MASK |
                                           DEVICE_CONTROL_VOLUME_MASK)) ==
                                          (DEVICE_CONTROL_BRIGHTNESS_MASK |
                                           DEVICE_CONTROL_VOLUME_MASK)
                                      ? "Brilho e volume salvos"
                                      : (mask & DEVICE_CONTROL_BRIGHTNESS_MASK) != 0U
                                            ? "Brilho da tela salvo"
                                            : "Volume salvo";
        np_feedback_show_toast(&s_ui.feedback,
                               projection.device_controls.save_result == ESP_OK
                                   ? NP_FEEDBACK_SUCCESS : NP_FEEDBACK_ERROR,
                               projection.device_controls.save_result == ESP_OK
                                   ? title : "Nao foi possivel salvar",
                               projection.device_controls.save_result == ESP_OK
                                   ? NULL : "Verifique o dispositivo", 2500U);
        if (projection.device_controls.save_result != ESP_OK &&
            s_ui.active_screen == PRODUCT_SCREEN_DISPLAY_SOUND) {
            settings_set_percent(active_display_controls()->brightness_slider,
                                 active_display_controls()->brightness_value,
                                 projection.device_controls.brightness_percent);
            settings_set_percent(active_display_controls()->volume_slider,
                                 active_display_controls()->volume_value,
                                 projection.device_controls.volume_percent);
        }
    }

    if (projection.revision != s_ui.rendered_revision &&
        s_ui.active_screen == PRODUCT_SCREEN_BOOT) {
        update_boot(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_HOME) {
        update_home(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_DISPLAY_SOUND) {
        update_header(&s_ui.display_sound.header, &projection, true);
        update_display_sound_controls(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_SYSTEM) {
        update_header(&s_ui.system_scene.header, &projection, true);
        update_system_information(&s_ui.system_scene.system, &projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_TIMEZONE) {
        update_timezone_scene(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_WIFI) {
        update_wifi_scene(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               s_ui.active_screen == PRODUCT_SCREEN_NOTIFICATIONS) {
        update_notifications_scene(&projection);
        s_ui.rendered_revision = projection.revision;
    } else if (projection.revision != s_ui.rendered_revision &&
               (s_ui.active_screen == PRODUCT_SCREEN_PROFILE ||
                s_ui.active_screen == PRODUCT_SCREEN_PREFERENCES)) {
        update_header(s_ui.active_screen == PRODUCT_SCREEN_PROFILE
                          ? &s_ui.profile.header : &s_ui.preferences.header,
                      &projection, true);
        if (s_ui.active_screen == PRODUCT_SCREEN_PROFILE)
            np_profile_sync(&s_ui.profile, projection.user_profile.configured,
                &projection.user_profile.profile,
                projection.user_profile.persistence_pending,
                projection.user_profile.last_result);
        s_ui.rendered_revision = projection.revision;
    }

    if (s_ui.active_screen == PRODUCT_SCREEN_BOOT) {
        const uint32_t elapsed = lv_tick_elaps(s_ui.started_at_tick);
        if (projection.network.credentials_active && !s_ui.boot_saved_network_seen) {
            s_ui.boot_saved_network_seen = true;
            s_ui.saved_network_wait_started_tick = lv_tick_get();
        }

        const bool saved_network_wait_expired = s_ui.boot_saved_network_seen &&
            lv_tick_elaps(s_ui.saved_network_wait_started_tick) >=
                BOOT_SAVED_NETWORK_MAXIMUM_MS;
        const bool saved_network_failed = s_ui.boot_saved_network_seen &&
            projection.network.state == APP_NETWORK_STATE_FAILED;
        const bool ready_to_leave = projection.time_trusted ||
            (!s_ui.boot_saved_network_seen && elapsed >= BOOT_MAXIMUM_MS) ||
            saved_network_wait_expired || saved_network_failed;

        if (elapsed >= BOOT_MINIMUM_MS && projection.ready && ready_to_leave) {
            show_home(&projection);
        }
    }
}

static void release_current_scene(void)
{
    /* All scene destruction runs after the touch callback, on the LVGL
     * owner. Cancel deferred work before freeing its target tree. */
    (void)lv_async_call_cancel(wifi_add_open_async, NULL);
    s_ui.wifi_add_pending = false;
    (void)lv_async_call_cancel(wifi_password_open_async, NULL);
    (void)lv_async_call_cancel(wifi_forget_open_async, NULL);
    (void)lv_async_call_cancel(system_restart_open_async, NULL);
    (void)lv_async_call_cancel(profile_editor_open_async, (void *)1);
    (void)lv_async_call_cancel(profile_editor_open_async, (void *)2);
    s_ui.profile_editor_pending = false;
    s_ui.wifi_password_pending = false;
    s_ui.wifi_confirmation_pending = false;
    if (s_ui.settings_value_bubble_timer != NULL) {
        lv_timer_delete(s_ui.settings_value_bubble_timer);
        s_ui.settings_value_bubble_timer = NULL;
    }
    np_keyboard_hide(&s_ui.keyboard);
    np_profile_close_editor(&s_ui.profile);
    discard_wifi_add();
    discard_wifi_password();
    discard_wifi_confirmation();
    discard_system_confirmation();
    /* All product scenes are lazy caches. Rebuilding them on each visit
     * fragments the LVGL heap under repeated navigation. Hidden roots do not
     * draw; only their small, fixed object trees remain allocated. */
    np_set_visible(s_ui.home.root, false);
    np_set_visible(s_ui.profile.root, false);
    np_set_visible(s_ui.preferences.root, false);
    np_set_visible(s_ui.display_sound.root, false);
    np_set_visible(s_ui.notifications_scene.root, false);
    np_set_visible(s_ui.wifi_scene.root, false);
    np_set_visible(s_ui.system_scene.header.drawer_scrim, false);
    np_set_visible(s_ui.system_scene.root, false);
    np_set_visible(s_ui.timezone_scene.header.drawer_scrim, false);
    np_set_visible(s_ui.timezone_scene.root, false);
    if (s_ui.timezone_scene.timezone.search != NULL)
        lv_obj_remove_state(s_ui.timezone_scene.timezone.search, LV_STATE_FOCUSED);
    s_ui.pending_wifi_ssid[0] = '\0';
}

static void navigation_leave(void *context, uintptr_t page)
{
    (void)context;
    release_current_scene();
    if (page == PRODUCT_SCREEN_PROFILE &&
        s_ui.navigation.destination == PRODUCT_SCREEN_PREFERENCES &&
        s_ui.profile.root != NULL) {
        /* Keep the outgoing scene painted until Preferences is ready. The
         * legacy Profile tree remains cached through the separated pass. */
        np_set_visible(s_ui.profile.root, true);
    }
    if (page == PRODUCT_SCREEN_PREFERENCES && s_ui.preferences.profile_button != NULL &&
        lv_obj_get_parent(s_ui.preferences.profile_button) == s_ui.shell) {
        np_set_visible(s_ui.preferences.profile_button, false);
    }
}

static bool navigation_evict_legacy(uintptr_t page)
{
    lv_obj_t *root = NULL;
    switch (page) {
    case PRODUCT_SCREEN_PROFILE: root = s_ui.profile.root; break;
    case PRODUCT_SCREEN_DISPLAY_SOUND: root = s_ui.display_sound.root; break;
    case PRODUCT_SCREEN_SYSTEM: root = s_ui.system_scene.root; break;
    case PRODUCT_SCREEN_NOTIFICATIONS: root = s_ui.notifications_scene.root; break;
    case PRODUCT_SCREEN_WIFI: root = s_ui.wifi_scene.root; break;
    case PRODUCT_SCREEN_TIMEZONE: root = s_ui.timezone_scene.root; break;
    default: return false;
    }
    if (root == NULL) return false;
    lv_obj_delete(root);
    switch (page) {
    case PRODUCT_SCREEN_PROFILE: s_ui.profile = (np_profile_view_t){0}; break;
    case PRODUCT_SCREEN_DISPLAY_SOUND:
        s_ui.display_sound = (np_settings_display_sound_view_t){0}; break;
    case PRODUCT_SCREEN_SYSTEM:
        s_ui.system_scene = (np_settings_system_view_t){0};
        s_ui.system_scene_object_count = 0; break;
    case PRODUCT_SCREEN_NOTIFICATIONS:
        s_ui.notifications_scene = (np_settings_notifications_view_t){0}; break;
    case PRODUCT_SCREEN_WIFI: s_ui.wifi_scene = (np_settings_wifi_view_t){0}; break;
    case PRODUCT_SCREEN_TIMEZONE:
        s_ui.timezone_scene = (np_settings_timezone_view_t){0};
        s_ui.timezone_scene_object_count = 0; break;
    default: break;
    }
    return true;
}

static bool navigation_legacy_cached(uintptr_t page)
{
    switch (page) {
    case PRODUCT_SCREEN_PROFILE: return s_ui.profile.root != NULL;
    case PRODUCT_SCREEN_DISPLAY_SOUND: return s_ui.display_sound.root != NULL;
    case PRODUCT_SCREEN_SYSTEM: return s_ui.system_scene.root != NULL;
    case PRODUCT_SCREEN_NOTIFICATIONS: return s_ui.notifications_scene.root != NULL;
    case PRODUCT_SCREEN_WIFI: return s_ui.wifi_scene.root != NULL;
    case PRODUCT_SCREEN_TIMEZONE: return s_ui.timezone_scene.root != NULL;
    default: return false;
    }
}

static void navigation_reclaim_for_build(uintptr_t destination)
{
    /* The 64 KiB LVGL pool cannot retain every legacy scene. Free hidden
     * caches after LEAVE and before BUILD, keeping the destination cache. */
    const uint32_t reserve = navigation_legacy_cached(destination) ? 16U * 1024U
        : (destination == PRODUCT_SCREEN_WIFI || destination == PRODUCT_SCREEN_TIMEZONE)
            ? 40U * 1024U
        : (destination == PRODUCT_SCREEN_HOME || destination == PRODUCT_SCREEN_PREFERENCES)
            ? 24U * 1024U : 30U * 1024U;
    const uintptr_t candidates[] = {
        PRODUCT_SCREEN_SYSTEM, PRODUCT_SCREEN_WIFI, PRODUCT_SCREEN_TIMEZONE,
        PRODUCT_SCREEN_DISPLAY_SOUND, PRODUCT_SCREEN_NOTIFICATIONS,
        PRODUCT_SCREEN_PROFILE,
    };
    lv_mem_monitor_t heap = {0};
    lv_mem_monitor(&heap);
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) &&
         heap.free_size < reserve; ++i) {
        if (candidates[i] == destination || !navigation_evict_legacy(candidates[i]))
            continue;
        lv_mem_monitor(&heap);
        ESP_LOGI(TAG, "Evicted cached page=%u; LVGL free=%u reserve=%u",
                 (unsigned)candidates[i], (unsigned)heap.free_size, (unsigned)reserve);
    }
    if (heap.free_size < reserve)
        ESP_LOGW(TAG, "LVGL reserve low before page=%u: free=%u reserve=%u",
                 (unsigned)destination, (unsigned)heap.free_size, (unsigned)reserve);
}

static void navigation_clean(void *context, uintptr_t page)
{
    (void)context;
    if (page == PRODUCT_SCREEN_HOME || page == PRODUCT_SCREEN_PREFERENCES) {
        /* Only pilot page objects belong to content_host. The shell, feedback,
         * keyboard and legacy lazy caches live outside it. */
        if (page == PRODUCT_SCREEN_PREFERENCES &&
            s_ui.preferences.profile_button != NULL &&
            lv_obj_get_parent(s_ui.preferences.profile_button) == s_ui.shell) {
            lv_obj_delete(s_ui.preferences.profile_button);
        }
        lv_obj_clean(s_ui.content_host);
        if (page == PRODUCT_SCREEN_HOME) {
            s_ui.home = (np_home_view_t){0};
            s_ui.home_data_rendered = false;
            s_ui.rendered_weather_icon_source = NULL;
        } else {
            s_ui.preferences = (np_preferences_view_t){0};
        }
    }
    if (s_ui.navigation.destination != PRODUCT_SCREEN_HOME &&
        s_ui.navigation.destination != PRODUCT_SCREEN_PREFERENCES) {
        np_set_visible(s_ui.shell, false);
    }
    navigation_reclaim_for_build(s_ui.navigation.destination);
}

static bool navigation_build(void *context, uintptr_t page)
{
    (void)context;
    if (page == PRODUCT_SCREEN_HOME || page == PRODUCT_SCREEN_PREFERENCES) {
        navigation_shell_create();
        app_ui_projection_t projection = {0};
        app_state_get_ui_projection(&projection);
        if (page == PRODUCT_SCREEN_HOME) {
            s_ui.home = np_home_build_with_header(s_ui.content_host, &s_ui.shell_header);
            if (s_ui.home.root == NULL) return false;
            s_ui.home_data_rendered = false;
            update_home(&projection);
            s_ui.active_screen = PRODUCT_SCREEN_HOME;
        } else {
            s_ui.preferences = np_preferences_build_with_header(s_ui.content_host,
                                                                  &s_ui.shell_header,
                                                                  s_ui.shell);
            if (s_ui.preferences.root == NULL) return false;
            lv_obj_add_event_cb(s_ui.preferences.profile_button,
                                scene_navigation_event_cb, LV_EVENT_CLICKED,
                                (void *)(uintptr_t)PRODUCT_SCREEN_PROFILE);
            for (uint8_t i = 0; i < NP_PREFERENCES_ITEM_COUNT; ++i) {
                lv_obj_add_event_cb(s_ui.preferences.rows[i], scene_navigation_event_cb,
                                    LV_EVENT_CLICKED, (void *)(uintptr_t)(i == 0U
                                        ? PRODUCT_SCREEN_DISPLAY_SOUND : i == 4U
                                        ? PRODUCT_SCREEN_SYSTEM : i == 3U
                                        ? PRODUCT_SCREEN_NOTIFICATIONS : i == 1U
                                        ? PRODUCT_SCREEN_WIFI : PRODUCT_SCREEN_TIMEZONE));
            }
            update_header(&s_ui.shell_header, &projection, true);
            s_ui.active_screen = PRODUCT_SCREEN_PREFERENCES;
        }
        s_ui.rendered_revision = projection.revision;
        np_set_visible(page == PRODUCT_SCREEN_HOME ? s_ui.home.root : s_ui.preferences.root,
                       true);
        return true;
    }
    /* The non-pilot screens retain the previous lazy cache implementation. */
    s_ui.navigation_scene_released = true;
    scene_navigation_async((void *)page);
    return (uintptr_t)s_ui.active_screen == page;
}

static void navigation_enter(void *context, uintptr_t page)
{
    (void)context;
    if (page == PRODUCT_SCREEN_HOME || page == PRODUCT_SCREEN_PREFERENCES) {
        if (s_ui.profile.root != NULL) np_set_visible(s_ui.profile.root, false);
        np_set_visible(s_ui.shell, true);
        lv_obj_move_foreground(s_ui.shell);
    }
    np_feedback_bring_to_front(&s_ui.feedback);
    s_ui.navigation_pending = false;
}

static void settings_home_async(void *user_data)
{
    (void)user_data;
    s_ui.navigation_pending = false;
    if (s_ui.active_screen == PRODUCT_SCREEN_BOOT ||
        s_ui.active_screen == PRODUCT_SCREEN_HOME) return;
    if (!s_ui.navigation_scene_released) release_current_scene();
    if (s_ui.home.root == NULL) {
        s_ui.home = np_home_build(lv_screen_active());
        s_ui.home_data_rendered = false;
        install_home_navigation_callbacks();
    }
    np_feedback_bring_to_front(&s_ui.feedback);

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_home(&projection);
    s_ui.rendered_revision = projection.revision;
    s_ui.active_screen = PRODUCT_SCREEN_HOME;
    np_set_visible(s_ui.home.root, true);
    lv_obj_move_foreground(s_ui.home.root);
    np_feedback_bring_to_front(&s_ui.feedback);
}

static void profile_identity_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_ui.profile_editor_pending) return;
    s_ui.profile_editor_pending = true;
    const bool focus_name = lv_event_get_target(event) != s_ui.profile.avatar_row;
    if (lv_async_call(profile_editor_open_async, (void *)(uintptr_t)(focus_name ? 1U : 2U))
        != LV_RESULT_OK) s_ui.profile_editor_pending = false;
}

static void profile_editor_open_async(void *user_data)
{
    s_ui.profile_editor_pending = false;
    if (s_ui.active_screen != PRODUCT_SCREEN_PROFILE || s_ui.profile.root == NULL) return;
    const bool first_open = s_ui.profile.editor.scrim == NULL;
    np_profile_open_editor(&s_ui.profile, &s_ui.keyboard, (uintptr_t)user_data == 1U);
    if (first_open && s_ui.profile.editor.scrim != NULL) {
        lv_obj_add_event_cb(s_ui.profile.save_button, profile_save_event_cb,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(s_ui.profile.cancel_button, profile_cancel_event_cb,
                            LV_EVENT_CLICKED, NULL);
    }
}

static void profile_cancel_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED)
        np_profile_close_editor(&s_ui.profile);
}

static void profile_save_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    user_profile_t profile = {0};
    if (!np_profile_editor_value(&s_ui.profile, &profile)) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
            "Nome inválido", "Informe um nome com até 48 bytes", 2800U);
        return;
    }
    const esp_err_t result = app_state_request_user_profile_update(&profile);
    if (result != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
            "Perfil indisponível", "Tente salvar novamente", 2800U);
        return;
    }
    np_profile_close_editor(&s_ui.profile);
}

static uint32_t scene_object_count(lv_obj_t *root)
{
    if (root == NULL) return 0U;
    uint32_t count = 1U;
    const uint32_t children = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < children; ++i) {
        count += scene_object_count(lv_obj_get_child(root, i));
    }
    return count;
}

static void scene_navigation_async(void *user_data)
{
    const uintptr_t destination = (uintptr_t)user_data;
    if (s_ui.active_screen == PRODUCT_SCREEN_BOOT ||
        destination == (uintptr_t)s_ui.active_screen) return;

    if (!s_ui.navigation_scene_released) {
        if (destination != PRODUCT_SCREEN_HOME &&
            destination != PRODUCT_SCREEN_PROFILE &&
            destination != PRODUCT_SCREEN_PREFERENCES &&
            destination != PRODUCT_SCREEN_DISPLAY_SOUND &&
            destination != PRODUCT_SCREEN_SYSTEM &&
            destination != PRODUCT_SCREEN_NOTIFICATIONS &&
            destination != PRODUCT_SCREEN_WIFI &&
            destination != PRODUCT_SCREEN_TIMEZONE) {
            s_ui.navigation_pending = false;
            return;
        }
        s_ui.navigation_destination = destination;
        s_ui.navigation_build_timer =
            lv_timer_create(scene_navigation_build_timer_cb, 32U, NULL);
        if (s_ui.navigation_build_timer == NULL) {
            /* Never combine destruction and construction as an allocation
             * fallback: that can starve IDLE0 while LVGL compacts its heap.
             * Keeping the current scene is safer than a risky transition. */
            s_ui.navigation_pending = false;
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                                   "Navegação indisponível", NULL, 2200U);
        }
        return;
    }

    s_ui.navigation_pending = false;
    if (destination == PRODUCT_SCREEN_HOME) {
        settings_home_async(NULL);
        s_ui.navigation_scene_released = false;
        return;
    }
    if (destination != PRODUCT_SCREEN_PROFILE &&
        destination != PRODUCT_SCREEN_PREFERENCES &&
        destination != PRODUCT_SCREEN_DISPLAY_SOUND &&
        destination != PRODUCT_SCREEN_SYSTEM &&
        destination != PRODUCT_SCREEN_NOTIFICATIONS &&
        destination != PRODUCT_SCREEN_WIFI &&
        destination != PRODUCT_SCREEN_TIMEZONE) return;

    np_header_t *header;
    lv_obj_t *root;
    bool install_header_callbacks = true;
    if (destination == PRODUCT_SCREEN_PROFILE) {
        install_header_callbacks = s_ui.profile.root == NULL;
        if (install_header_callbacks) {
            s_ui.profile = np_profile_build(lv_screen_active());
            lv_obj_add_event_cb(s_ui.profile.home_button, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_HOME);
            lv_obj_add_event_cb(s_ui.profile.initial_screen_row, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_HOME);
            lv_obj_add_event_cb(s_ui.profile.preferences_row, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
            lv_obj_add_event_cb(s_ui.profile.edit_button, profile_identity_event_cb,
                                LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.profile.name_row, profile_identity_event_cb,
                                LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.profile.avatar_row, profile_identity_event_cb,
                                LV_EVENT_CLICKED, NULL);
        }
        header = &s_ui.profile.header;
        root = s_ui.profile.root;
        app_ui_projection_t current = {0};
        app_state_get_ui_projection(&current);
        np_profile_sync(&s_ui.profile, current.user_profile.configured,
            &current.user_profile.profile, current.user_profile.persistence_pending,
            current.user_profile.last_result);
        s_ui.active_screen = PRODUCT_SCREEN_PROFILE;
    } else if (destination == PRODUCT_SCREEN_DISPLAY_SOUND) {
        install_header_callbacks = s_ui.display_sound.root == NULL;
        if (install_header_callbacks) {
            s_ui.display_sound = np_settings_display_sound_build(lv_screen_active());
            install_display_sound_callbacks();
            lv_obj_add_event_cb(s_ui.display_sound.back_button, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
        }
        s_ui.active_screen = PRODUCT_SCREEN_DISPLAY_SOUND;
        header = &s_ui.display_sound.header;
        root = s_ui.display_sound.root;
    } else if (destination == PRODUCT_SCREEN_TIMEZONE) {
        install_header_callbacks = s_ui.timezone_scene.root == NULL;
        const uint32_t started = lv_tick_get();
        np_settings_timezone_scene_create(&s_ui.timezone_scene, lv_screen_active(),
            &s_ui.keyboard, timezone_select_cb, NULL);
        header = &s_ui.timezone_scene.header;
        root = s_ui.timezone_scene.root;
        s_ui.active_screen = PRODUCT_SCREEN_TIMEZONE;
        app_ui_projection_t current = {0};
        app_state_get_ui_projection(&current);
        np_settings_timezone_scene_enter(&s_ui.timezone_scene, current.onboarding.timezone_index);
        if (install_header_callbacks) {
            lv_obj_add_event_cb(s_ui.timezone_scene.back_button, scene_navigation_event_cb,
                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
            s_ui.timezone_scene_object_count = scene_object_count(root);
            ESP_LOGI(TAG, "Timezone scene built: %lu objects, %lu ms",
                (unsigned long)s_ui.timezone_scene_object_count, (unsigned long)lv_tick_elaps(started));
        } else if (scene_object_count(root) != s_ui.timezone_scene_object_count) {
            ESP_LOGW(TAG, "Timezone scene object count changed");
        }
    } else if (destination == PRODUCT_SCREEN_WIFI) {
        install_header_callbacks = s_ui.wifi_scene.root == NULL;
        const uint32_t started = lv_tick_get();
        if (install_header_callbacks) {
            s_ui.wifi_scene = np_settings_wifi_scene_build(lv_screen_active());
            np_settings_wifi_scene_bind(&s_ui.wifi_scene);
            lv_obj_add_event_cb(s_ui.wifi_scene.back_button, scene_navigation_event_cb,
                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
            lv_obj_add_event_cb(s_ui.wifi_scene.add_button, wifi_add_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.wifi_scene.wifi.scan_button, wifi_scan_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.wifi_scene.wifi.connect_button, wifi_manage_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.wifi_scene.wifi.forget_button, wifi_forget_event_cb, LV_EVENT_CLICKED, NULL);
            ESP_LOGI(TAG, "Wi-Fi scene built: %lu objects, %lu ms",
                (unsigned long)scene_object_count(s_ui.wifi_scene.root), (unsigned long)lv_tick_elaps(started));
        }
        header = &s_ui.wifi_scene.header;
        root = s_ui.wifi_scene.root;
        s_ui.active_screen = PRODUCT_SCREEN_WIFI;
    } else if (destination == PRODUCT_SCREEN_NOTIFICATIONS) {
        install_header_callbacks = s_ui.notifications_scene.root == NULL;
        if (install_header_callbacks) {
            s_ui.notifications_scene = np_settings_notifications_scene_build(lv_screen_active());
            install_notification_callbacks(&s_ui.notifications_scene.notifications);
            lv_obj_add_event_cb(s_ui.notifications_scene.back_button, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
        }
        header = &s_ui.notifications_scene.header;
        root = s_ui.notifications_scene.root;
        s_ui.active_screen = PRODUCT_SCREEN_NOTIFICATIONS;
    } else if (destination == PRODUCT_SCREEN_SYSTEM) {
        install_header_callbacks = s_ui.system_scene.root == NULL;
        const uint32_t started = lv_tick_get();
        np_settings_system_scene_create(&s_ui.system_scene, lv_screen_active());
        header = &s_ui.system_scene.header;
        root = s_ui.system_scene.root;
        s_ui.active_screen = PRODUCT_SCREEN_SYSTEM;
        if (install_header_callbacks) {
            lv_obj_add_event_cb(s_ui.system_scene.back_button, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
            lv_obj_add_event_cb(s_ui.system_scene.system.restart_button, system_restart_event_cb,
                                LV_EVENT_CLICKED, NULL);
            s_ui.system_scene_object_count = scene_object_count(root);
            ESP_LOGI(TAG, "System scene built: %lu objects, %lu ms",
                     (unsigned long)s_ui.system_scene_object_count,
                     (unsigned long)lv_tick_elaps(started));
        } else {
            const uint32_t count = scene_object_count(root);
            if (count != s_ui.system_scene_object_count) {
                ESP_LOGW(TAG, "System scene object count changed: %lu -> %lu",
                         (unsigned long)s_ui.system_scene_object_count,
                         (unsigned long)count);
            }
        }
    } else {
        install_header_callbacks = s_ui.preferences.root == NULL;
        if (install_header_callbacks) {
            s_ui.preferences = np_preferences_build(lv_screen_active());
            lv_obj_add_event_cb(s_ui.preferences.profile_button, scene_navigation_event_cb,
                                LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PROFILE);
            for (uint8_t i = 0; i < NP_PREFERENCES_ITEM_COUNT; ++i) {
                lv_obj_add_event_cb(s_ui.preferences.rows[i], scene_navigation_event_cb,
                                    LV_EVENT_CLICKED, (void *)(uintptr_t)(i == 0U
                                        ? PRODUCT_SCREEN_DISPLAY_SOUND : i == 4U
                                        ? PRODUCT_SCREEN_SYSTEM : i == 3U
                                        ? PRODUCT_SCREEN_NOTIFICATIONS : i == 1U
                                        ? PRODUCT_SCREEN_WIFI : PRODUCT_SCREEN_TIMEZONE));
            }
        }
        header = &s_ui.preferences.header;
        root = s_ui.preferences.root;
        s_ui.active_screen = PRODUCT_SCREEN_PREFERENCES;
    }
    if (install_header_callbacks) {
        lv_obj_add_event_cb(header->drawer_home_button, scene_navigation_event_cb,
                            LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_HOME);
        lv_obj_add_event_cb(header->drawer_settings_button, scene_navigation_event_cb,
                            LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
        lv_obj_add_event_cb(header->settings_button, scene_navigation_event_cb,
                            LV_EVENT_CLICKED, (void *)(uintptr_t)PRODUCT_SCREEN_PROFILE);
    }
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_header(header, &projection, true);
    if (destination == PRODUCT_SCREEN_DISPLAY_SOUND) update_display_sound_controls(&projection);
    if (destination == PRODUCT_SCREEN_SYSTEM) update_system_information(&s_ui.system_scene.system, &projection);
    if (destination == PRODUCT_SCREEN_NOTIFICATIONS) update_notifications_scene(&projection);
    if (destination == PRODUCT_SCREEN_WIFI) update_wifi_scene(&projection);
    if (destination == PRODUCT_SCREEN_TIMEZONE) update_timezone_scene(&projection);
    s_ui.rendered_revision = projection.revision;
    np_set_visible(root, true);
    lv_obj_move_foreground(root);
    np_feedback_bring_to_front(&s_ui.feedback);
    s_ui.navigation_scene_released = false;
}

static void scene_navigation_build_timer_cb(lv_timer_t *timer)
{
    const uintptr_t destination = s_ui.navigation_destination;
    s_ui.navigation_build_timer = NULL;
    lv_timer_delete(timer);
    if (!s_ui.navigation_scene_released) {
        /* Keep the current scene displayed through the debounce. Free its
         * object tree in this pass and let LVGL return before the next pass
         * creates another tree. This avoids allocator/draw pressure from
         * deleting and building a full scene in one LVGL cycle. */
        release_current_scene();
        s_ui.navigation_scene_released = true;
        s_ui.navigation_build_timer =
            lv_timer_create(scene_navigation_build_timer_cb, 32U, NULL);
        if (s_ui.navigation_build_timer != NULL) return;

        /* This is an exceptional memory exhaustion path. A deferred async
         * still preserves task ordering; it must never recurse here. */
        if (lv_async_call(scene_navigation_async, (void *)destination) == LV_RESULT_OK)
            return;
        s_ui.navigation_scene_released = false;
        s_ui.navigation_pending = false;
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Navegação indisponível", NULL, 2200U);
        return;
    }

    scene_navigation_async((void *)destination);
}

static void scene_navigation_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_ui.navigation_pending || s_ui.active_screen == PRODUCT_SCREEN_BOOT) return;
    const uintptr_t destination = (uintptr_t)lv_event_get_user_data(event);
    if (destination == (uintptr_t)s_ui.active_screen) return;
    if (np_navigation_request(&s_ui.navigation, destination)) {
        s_ui.navigation_pending = true;
    }
}

static void install_home_navigation_callbacks(void)
{
    lv_obj_add_event_cb(s_ui.home.header.drawer_settings_button,
                        scene_navigation_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PRODUCT_SCREEN_PREFERENCES);
    lv_obj_add_event_cb(s_ui.home.header.settings_button,
                        scene_navigation_event_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)PRODUCT_SCREEN_PROFILE);
}

esp_err_t product_ui_create(lv_display_t *display, lv_indev_t *touch_indev)
{
    ESP_RETURN_ON_FALSE(display != NULL && touch_indev != NULL,
                        ESP_ERR_INVALID_ARG,
                        "product_ui",
                        "Display or touch handle missing");

    s_ui = (product_ui_state_t){
        .display = display,
        .touch_indev = touch_indev,
        .started_at_tick = lv_tick_get(),
        .rendered_revision = UINT32_MAX,
    };

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);

    const np_navigation_ops_t navigation_ops = {
        .leave = navigation_leave,
        .clean = navigation_clean,
        .build = navigation_build,
        .enter = navigation_enter,
    };
    np_navigation_init(&s_ui.navigation, screen, PRODUCT_SCREEN_BOOT,
                       &navigation_ops, NULL);

    s_ui.boot = np_boot_build(screen);
    s_ui.feedback = np_feedback_create(screen);
    s_ui.keyboard = np_keyboard_create(screen);
    np_feedback_bring_to_front(&s_ui.feedback);
    s_ui.refresh_timer =
        lv_timer_create(refresh_timer_cb, UI_REFRESH_PERIOD_MS, NULL);

    ESP_RETURN_ON_FALSE(s_ui.refresh_timer != NULL,
                        ESP_ERR_NO_MEM,
                        "product_ui",
                        "UI refresh timer allocation failed");

    refresh_timer_cb(s_ui.refresh_timer);
    return ESP_OK;
}

void product_ui_cycle_begin(void *context)
{
    (void)context;
    np_navigation_cycle_begin(&s_ui.navigation);
}

void product_ui_cycle_end(void *context)
{
    (void)context;
    np_navigation_cycle_end(&s_ui.navigation);
}
