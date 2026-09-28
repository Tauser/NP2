#include "product_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_state.h"
#include "device_control_service.h"
#include "diagnostic_ui.h"
#include "esp_check.h"
#include "esp_log.h"
#include "offline_value_format.h"
#include "np_screens.h"
#include "np_feedback.h"
#include "np_keyboard.h"
#include "notification_service.h"
#include "onboarding_service.h"
#include "weather_condition.h"
#include "wifi_setup_view.h"

#define UI_REFRESH_PERIOD_MS 250U
#define BOOT_MINIMUM_MS 1200U
#define BOOT_MAXIMUM_MS 6000U
#define SETTINGS_STAGE_INTERVAL_MS 80U
#define SETTINGS_STAGE_COUNT 1U

typedef struct {
    enum {
        PRODUCT_SCREEN_BOOT = 0,
        PRODUCT_SCREEN_HOME,
        PRODUCT_SCREEN_SETTINGS,
    } active_screen;
    lv_display_t *display;
    lv_indev_t *touch_indev;
    np_boot_view_t boot;
    np_home_view_t home;
    np_settings_view_t settings;
    np_feedback_t feedback;
    np_keyboard_t keyboard;
    lv_timer_t *refresh_timer;
    lv_timer_t *settings_stage_timer;
    lv_timer_t *settings_value_bubble_timer;
    uint32_t started_at_tick;
    uint32_t rendered_revision;
    uint32_t notified_persisted_generation;
    uint32_t control_save_completion_id;
    offline_data_snapshot_t rendered_home_data;
    const void *rendered_weather_icon_source;
    uint8_t settings_stage;
    bool navigation_pending;
    bool notification_feedback_initialized;
    bool syncing_notification_controls;
    bool home_data_rendered;
    bool settings_controls_initialized;
    bool settings_notification_callbacks_initialized;
    bool settings_timezone_callbacks_initialized;
    bool settings_wifi_callbacks_initialized;
    bool settings_rows_initialized;
    uint8_t projected_brightness;
    uint8_t projected_volume;
} product_ui_state_t;

static product_ui_state_t s_ui;
static const char *const TAG = "product_ui";

static void diagnostics_button_event_cb(lv_event_t *event);
static void settings_home_event_cb(lv_event_t *event);
static void settings_stage_timer_cb(lv_timer_t *timer);
static void settings_stage_async(void *user_data);
static void install_settings_control_callbacks(void);
static void settings_value_bubble_timer_cb(lv_timer_t *timer);
static void notification_switch_event_cb(lv_event_t *event);
static void notifications_test_event_cb(lv_event_t *event);
static esp_err_t timezone_select_cb(void *user_data, uint16_t timezone_index);
static void keyboard_modal_close_cb(void *user_data);
static void wifi_manage_event_cb(lv_event_t *event);
static void wifi_scan_event_cb(lv_event_t *event);
static void wifi_forget_event_cb(lv_event_t *event);
static void settings_modal_row_event_cb(lv_event_t *event);
static void update_settings(const app_ui_projection_t *projection);

typedef enum {
    SETTINGS_CONTROL_BRIGHTNESS = 0,
    SETTINGS_CONTROL_VOLUME,
} settings_control_t;

static np_data_state_t data_state(bool available, bool stale)
{
    if (!available) return NP_DATA_UNAVAILABLE;
    return stale ? NP_DATA_STALE : NP_DATA_LIVE;
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
    np_set_visible(s_ui.settings.brightness_bubble, false);
    np_set_visible(s_ui.settings.volume_bubble, false);
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
            settings_set_percent(s_ui.settings.brightness_slider,
                                 s_ui.settings.brightness_value,
                                 s_ui.projected_brightness);
            return;
        }
    } else {
        if (device_control_set_volume(percent) != ESP_OK) {
            ESP_LOGW(TAG, "Volume request unavailable");
            np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                                   "Volume nao alterado", NULL, 2500U);
            settings_set_percent(s_ui.settings.volume_slider,
                                 s_ui.settings.volume_value,
                                 s_ui.projected_volume);
            return;
        }
        /* The control worker applies this queued volume before the chime.
         * This is a local control preview, independent of notification
         * delivery preferences, and never runs audio from the LVGL task. */
        (void)device_control_play_notification_tone();
    }

    if (control == SETTINGS_CONTROL_BRIGHTNESS) {
        settings_set_percent(NULL, s_ui.settings.brightness_value, percent);
        settings_show_value_bubble(s_ui.settings.brightness_slider,
                                   s_ui.settings.brightness_bubble,
                                   s_ui.settings.brightness_bubble_value,
                                   percent);
    } else {
        settings_set_percent(NULL, s_ui.settings.volume_value, percent);
        settings_show_value_bubble(s_ui.settings.volume_slider,
                                   s_ui.settings.volume_bubble,
                                   s_ui.settings.volume_bubble_value,
                                   percent);
    }
}

static void install_settings_control_callbacks(void)
{
    if (!s_ui.settings_controls_initialized) {
        app_ui_projection_t projection = {0};
        app_state_get_ui_projection(&projection);
        s_ui.projected_brightness = projection.device_controls.brightness_percent;
        s_ui.projected_volume = projection.device_controls.volume_percent;
        s_ui.settings_controls_initialized = true;

        settings_set_percent(s_ui.settings.brightness_slider,
                             s_ui.settings.brightness_value,
                             s_ui.projected_brightness);
        settings_set_percent(s_ui.settings.volume_slider,
                             s_ui.settings.volume_value,
                             s_ui.projected_volume);

        if (s_ui.settings_value_bubble_timer == NULL) {
            s_ui.settings_value_bubble_timer =
                lv_timer_create(settings_value_bubble_timer_cb, 1000U, NULL);
            if (s_ui.settings_value_bubble_timer != NULL) {
                lv_timer_pause(s_ui.settings_value_bubble_timer);
            }
        }

        if (s_ui.settings.brightness_slider != NULL) {
            lv_obj_add_event_cb(s_ui.settings.brightness_slider,
                                settings_control_event_cb,
                                LV_EVENT_RELEASED,
                                (void *)(uintptr_t)SETTINGS_CONTROL_BRIGHTNESS);
            lv_obj_add_event_cb(s_ui.settings.brightness_slider,
                                settings_control_event_cb,
                                LV_EVENT_PRESS_LOST,
                                (void *)(uintptr_t)SETTINGS_CONTROL_BRIGHTNESS);
        }
        if (s_ui.settings.volume_slider != NULL) {
            lv_obj_add_event_cb(s_ui.settings.volume_slider,
                                settings_control_event_cb,
                                LV_EVENT_RELEASED,
                                (void *)(uintptr_t)SETTINGS_CONTROL_VOLUME);
            lv_obj_add_event_cb(s_ui.settings.volume_slider,
                                settings_control_event_cb,
                                LV_EVENT_PRESS_LOST,
                                (void *)(uintptr_t)SETTINGS_CONTROL_VOLUME);
        }
    }

    if (!s_ui.settings_notification_callbacks_initialized &&
        s_ui.settings.notifications.general_switch != NULL) {
        lv_obj_add_event_cb(s_ui.settings.notifications.general_switch, notification_switch_event_cb,
                            LV_EVENT_VALUE_CHANGED, (void *)0U);
        lv_obj_add_event_cb(s_ui.settings.notifications.sound_switch, notification_switch_event_cb,
                            LV_EVENT_VALUE_CHANGED, (void *)1U);
        lv_obj_add_event_cb(s_ui.settings.notifications.system_switch, notification_switch_event_cb,
                            LV_EVENT_VALUE_CHANGED, (void *)2U);
        if (s_ui.settings.notifications.test_button != NULL) {
            lv_obj_add_event_cb(s_ui.settings.notifications.test_button,
                                notifications_test_event_cb,
                                LV_EVENT_CLICKED, NULL);
        }
        s_ui.settings_notification_callbacks_initialized = true;
    }

    if (!s_ui.settings_timezone_callbacks_initialized &&
        s_ui.settings.timezone.search != NULL) {
        np_settings_timezone_set_select_callback(&s_ui.settings.timezone,
                                                 timezone_select_cb, NULL);
        np_settings_timezone_set_close_callback(&s_ui.settings.timezone,
                                                keyboard_modal_close_cb,
                                                &s_ui.keyboard);
        np_keyboard_bind(&s_ui.keyboard, s_ui.settings.timezone.search,
                         NP_KEYBOARD_MODE_TEXT);
        s_ui.settings_timezone_callbacks_initialized = true;
    }

    if (!s_ui.settings_wifi_callbacks_initialized &&
        s_ui.settings.wifi.manage_button != NULL) {
        lv_obj_add_event_cb(s_ui.settings.wifi.manage_button, wifi_manage_event_cb,
                            LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(s_ui.settings.wifi.scan_button, wifi_scan_event_cb,
                            LV_EVENT_CLICKED, NULL);
        s_ui.settings_wifi_callbacks_initialized = true;
    }

    if (!s_ui.settings_rows_initialized && s_ui.settings.wifi_row != NULL) {
        lv_obj_add_event_cb(s_ui.settings.wifi_row, settings_modal_row_event_cb,
                            LV_EVENT_CLICKED, (void *)0U);
        lv_obj_add_event_cb(s_ui.settings.timezone_row, settings_modal_row_event_cb,
                            LV_EVENT_CLICKED, (void *)1U);
        lv_obj_add_event_cb(s_ui.settings.notifications_row, settings_modal_row_event_cb,
                            LV_EVENT_CLICKED, (void *)2U);
        lv_obj_add_event_cb(s_ui.settings.system_row, settings_modal_row_event_cb,
                            LV_EVENT_CLICKED, (void *)3U);
        s_ui.settings_rows_initialized = true;
    }
}

static void discard_settings_modals(uintptr_t keep)
{
    np_keyboard_hide(&s_ui.keyboard);
    if (keep != 0U && s_ui.settings.wifi.modal.scrim != NULL) {
        lv_obj_delete(s_ui.settings.wifi.modal.scrim);
        s_ui.settings.wifi = (np_settings_wifi_t){0};
        s_ui.settings_wifi_callbacks_initialized = false;
    }
    if (keep != 1U && s_ui.settings.timezone.modal.scrim != NULL) {
        lv_obj_delete(s_ui.settings.timezone.modal.scrim);
        s_ui.settings.timezone = (np_settings_timezone_t){0};
        s_ui.settings_timezone_callbacks_initialized = false;
    }
    if (keep != 2U && s_ui.settings.notifications.modal.scrim != NULL) {
        lv_obj_delete(s_ui.settings.notifications.modal.scrim);
        s_ui.settings.notifications = (np_settings_notifications_t){0};
        s_ui.settings_notification_callbacks_initialized = false;
    }
    if (keep != 3U && s_ui.settings.system.modal.scrim != NULL) {
        lv_obj_delete(s_ui.settings.system.modal.scrim);
        s_ui.settings.system = (np_settings_system_t){0};
    }
}

static void settings_modal_row_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const uintptr_t type = (uintptr_t)lv_event_get_user_data(event);
    discard_settings_modals(type);
    if (type == 0U) {
        if (s_ui.settings.wifi.modal.scrim == NULL) {
            np_settings_wifi_create(&s_ui.settings.wifi, s_ui.settings.root);
            lv_obj_add_event_cb(s_ui.settings.wifi.manage_button, wifi_manage_event_cb,
                                LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.settings.wifi.scan_button, wifi_scan_event_cb,
                                LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.settings.wifi.connect_button, wifi_manage_event_cb,
                                LV_EVENT_CLICKED, NULL);
            lv_obj_add_event_cb(s_ui.settings.wifi.forget_button, wifi_forget_event_cb,
                                LV_EVENT_CLICKED, NULL);
            s_ui.settings_wifi_callbacks_initialized = true;
        }
        app_ui_projection_t projection = {0}; app_state_get_ui_projection(&projection);
        np_settings_wifi_sync(&s_ui.settings.wifi, projection.network.online,
                              projection.network.scan_results_count, projection.network.scan_results);
        np_settings_wifi_show(&s_ui.settings.wifi);
    } else if (type == 1U) {
        if (s_ui.settings.timezone.modal.scrim == NULL) {
            np_settings_timezone_create(&s_ui.settings.timezone, s_ui.settings.root);
            np_settings_timezone_set_select_callback(&s_ui.settings.timezone, timezone_select_cb, NULL);
            np_settings_timezone_set_close_callback(&s_ui.settings.timezone, keyboard_modal_close_cb, &s_ui.keyboard);
            np_keyboard_bind(&s_ui.keyboard, s_ui.settings.timezone.search, NP_KEYBOARD_MODE_TEXT);
            s_ui.settings_timezone_callbacks_initialized = true;
        }
        app_ui_projection_t projection = {0}; app_state_get_ui_projection(&projection);
        np_settings_timezone_show(&s_ui.settings.timezone, projection.onboarding.timezone_index);
    } else if (type == 2U) {
        if (s_ui.settings.notifications.modal.scrim == NULL) {
            np_settings_notifications_create(&s_ui.settings.notifications, s_ui.settings.root);
            lv_obj_add_event_cb(s_ui.settings.notifications.general_switch, notification_switch_event_cb, LV_EVENT_VALUE_CHANGED, (void *)0U);
            lv_obj_add_event_cb(s_ui.settings.notifications.sound_switch, notification_switch_event_cb, LV_EVENT_VALUE_CHANGED, (void *)1U);
            lv_obj_add_event_cb(s_ui.settings.notifications.system_switch, notification_switch_event_cb, LV_EVENT_VALUE_CHANGED, (void *)2U);
            lv_obj_add_event_cb(s_ui.settings.notifications.test_button, notifications_test_event_cb, LV_EVENT_CLICKED, NULL);
            s_ui.settings_notification_callbacks_initialized = true;
        }
        np_settings_notifications_show(&s_ui.settings.notifications);
    } else {
        if (s_ui.settings.system.modal.scrim == NULL) np_settings_system_create(&s_ui.settings.system, s_ui.settings.root);
        np_settings_system_show(&s_ui.settings.system);
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

static void keyboard_modal_close_cb(void *user_data)
{
    np_keyboard_hide(user_data);
}

static esp_err_t timezone_select_cb(void *user_data, uint16_t timezone_index)
{
    (void)user_data;
    const esp_err_t result = onboarding_service_request_timezone_update(timezone_index);
    if (result != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Fuso nao alterado",
                               "Aguarde a atualizacao atual terminar", 2600U);
        return result;
    }

    np_keyboard_hide(&s_ui.keyboard);
    np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                           "Fuso horario atualizado",
                           "Aplicando e salvando preferencia", 2200U);
    return ESP_OK;
}

static void wifi_manage_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    np_settings_wifi_hide(&s_ui.settings.wifi);
    if (wifi_setup_view_open(s_ui.settings.root) != ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Configuracao indisponivel",
                               "Tente novamente em alguns instantes", 2600U);
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

static void wifi_forget_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (connectivity_diagnostic_request_forget() == ESP_OK) {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_INFO,
                               "Rede esquecida", NULL, 1800U);
    } else {
        np_feedback_show_toast(&s_ui.feedback, NP_FEEDBACK_ERROR,
                               "Nao foi possivel esquecer", NULL, 2200U);
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

    np_header_set_drawer_active(header, settings_active);
    np_header_set_connections(
        header,
        projection->network.online,
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
    np_home_set_btc_spark(&s_ui.home, NULL, 0U, np_c_text_3());
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
    if (projection->offline_data.weather.available ||
        projection->offline_data.market.available) {
        stage = 5U;
    }

    return stage;
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

    np_segbar_set(&s_ui.boot.progress, stage, np_c_accent());
    np_set_text(s_ui.boot.status, status[stage]);

    if (stage < 3U) {
        np_set_text(s_ui.boot.detail,
                    "A inicialização continua mesmo sem internet.");
    } else if (!projection->network.online) {
        np_set_text(s_ui.boot.detail,
                    "O painel abrirá com os últimos dados salvos.");
    } else {
        np_set_text(s_ui.boot.detail,
                    "Conexão disponível; concluindo a inicialização.");
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

static void update_settings(const app_ui_projection_t *projection)
{
    if (projection == NULL) return;

    update_header(&s_ui.settings.header, projection, true);

    np_settings_timezone_sync(&s_ui.settings.timezone,
                              projection->onboarding.timezone_index);
    np_set_text(s_ui.settings.timezone_value,
                np_settings_timezone_selected_label(&s_ui.settings.timezone));
    np_settings_wifi_sync(&s_ui.settings.wifi, projection->network.online,
                          projection->network.scan_results_count,
                          projection->network.scan_results);
    np_set_text(s_ui.settings.wifi_value,
                projection->network.online ? "Conectado" : "Sem conexao");

    if (s_ui.settings_controls_initialized && projection->device_controls.ready) {
        if (projection->device_controls.brightness_percent != s_ui.projected_brightness) {
            s_ui.projected_brightness = projection->device_controls.brightness_percent;
            settings_set_percent(s_ui.settings.brightness_slider,
                                 s_ui.settings.brightness_value,
                                 s_ui.projected_brightness);
        }
        if (projection->device_controls.volume_percent != s_ui.projected_volume) {
            s_ui.projected_volume = projection->device_controls.volume_percent;
            settings_set_percent(s_ui.settings.volume_slider,
                                 s_ui.settings.volume_value,
                                 s_ui.projected_volume);
        }
    }

    if (!projection->notifications.ready) return;

    s_ui.syncing_notification_controls = true;
    np_settings_notifications_sync(&s_ui.settings.notifications,
                                   projection->notifications.general_enabled,
                                   projection->notifications.sound_enabled,
                                   projection->notifications.system_alerts_enabled);
    s_ui.syncing_notification_controls = false;
    np_set_text(s_ui.settings.notifications_value,
                projection->notifications.persistence_pending
                    ? "Atualizando..."
                    : projection->notifications.general_enabled ? "Ativadas" : "Silenciadas");
}

static void install_home_navigation_callbacks(void);

static void show_home(const app_ui_projection_t *projection)
{
    if (s_ui.active_screen != PRODUCT_SCREEN_BOOT) return;

    lv_obj_t *const boot_root = s_ui.boot.root;
    s_ui.home = np_home_build(lv_screen_active());
    s_ui.home_data_rendered = false;
    np_feedback_bring_to_front(&s_ui.feedback);
    install_home_navigation_callbacks();
    update_home(projection);
    s_ui.active_screen = PRODUCT_SCREEN_HOME;
    s_ui.rendered_revision = projection->revision;

    /* Mantemos somente a cena ativa; a tela de boot nao fica alocada. */
    lv_obj_delete(boot_root);
    s_ui.boot = (np_boot_view_t){0};

    ESP_LOGI(TAG, "Boot transition complete; Home V2 visible");
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);

    if (projection.device_controls.save_completion_id !=
        s_ui.control_save_completion_id) {
        s_ui.control_save_completion_id = projection.device_controls.save_completion_id;
        const uint8_t mask = projection.device_controls.save_completion_mask;
        const char *const title = (mask & (DEVICE_CONTROL_BRIGHTNESS_MASK |
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
            s_ui.active_screen == PRODUCT_SCREEN_SETTINGS) {
            settings_set_percent(s_ui.settings.brightness_slider,
                                 s_ui.settings.brightness_value,
                                 projection.device_controls.brightness_percent);
            settings_set_percent(s_ui.settings.volume_slider,
                                 s_ui.settings.volume_value,
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
               s_ui.active_screen == PRODUCT_SCREEN_SETTINGS) {
        update_settings(&projection);
        s_ui.rendered_revision = projection.revision;
    }

    const uint32_t elapsed = lv_tick_elaps(s_ui.started_at_tick);

    if (elapsed >= BOOT_MINIMUM_MS &&
        projection.ready &&
        (projection.time_trusted || elapsed >= BOOT_MAXIMUM_MS)) {
        show_home(&projection);
    } else if (elapsed >= BOOT_MAXIMUM_MS) {
        show_home(&projection);
    }
}

static void open_settings_async(void *user_data)
{
    (void)user_data;
    s_ui.navigation_pending = false;
    if (s_ui.active_screen != PRODUCT_SCREEN_HOME) return;

    lv_obj_t *const home_root = s_ui.home.root;
    /* This runs after the click callback. Release the animated Home before
     * allocating and rendering the Settings tree on the same LVGL owner. */
    lv_obj_delete(home_root);
    s_ui.home = (np_home_view_t){0};
    s_ui.settings = np_settings_begin(lv_screen_active());
    s_ui.settings_controls_initialized = false;
    s_ui.settings_notification_callbacks_initialized = false;
    s_ui.settings_timezone_callbacks_initialized = false;
    s_ui.settings_wifi_callbacks_initialized = false;
    s_ui.settings_rows_initialized = false;
    np_feedback_bring_to_front(&s_ui.feedback);
    np_set_visible(s_ui.settings.root, true);
    np_settings_reset_stages(&s_ui.settings);
    if (s_ui.settings.home_button != NULL) {
        lv_obj_add_event_cb(s_ui.settings.home_button,
                            settings_home_event_cb, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_add_event_cb(s_ui.settings.header.drawer_home_button,
                        settings_home_event_cb, LV_EVENT_CLICKED, NULL);
    s_ui.active_screen = PRODUCT_SCREEN_SETTINGS;

    s_ui.settings_stage = 0U;
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_settings(&projection);
    s_ui.rendered_revision = projection.revision;
    s_ui.settings_stage_timer = lv_timer_create(settings_stage_timer_cb,
                                                 SETTINGS_STAGE_INTERVAL_MS,
                                                 NULL);
    if (s_ui.settings_stage_timer == NULL) {
        /* Cada card continua em um ciclo proprio, mesmo sem timer. */
        if (lv_async_call(settings_stage_async, NULL) != LV_RESULT_OK) {
            ESP_LOGE(TAG, "Settings cards unavailable: no LVGL work slot");
        }
    }
}

static void settings_stage_timer_cb(lv_timer_t *timer)
{
    if (s_ui.active_screen != PRODUCT_SCREEN_SETTINGS) {
        lv_timer_delete(timer);
        s_ui.settings_stage_timer = NULL;
        return;
    }

    if (!np_settings_build_next_card(&s_ui.settings)) {
        lv_timer_delete(timer);
        s_ui.settings_stage_timer = NULL;
        return;
    }

    install_settings_control_callbacks();
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_settings(&projection);

    if (s_ui.settings_stage == 0U) {
        np_set_visible(s_ui.settings.left_card, true);
    }

    s_ui.settings_stage++;
    if (s_ui.settings_stage >= SETTINGS_STAGE_COUNT) {
        lv_timer_delete(timer);
        s_ui.settings_stage_timer = NULL;
    }
}

static void settings_stage_async(void *user_data)
{
    (void)user_data;
    if (s_ui.active_screen != PRODUCT_SCREEN_SETTINGS ||
        !np_settings_build_next_card(&s_ui.settings)) {
        return;
    }

    install_settings_control_callbacks();
    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_settings(&projection);

    if (s_ui.settings_stage == 0U) {
        np_set_visible(s_ui.settings.left_card, true);
    }

    s_ui.settings_stage++;
    if (s_ui.settings_stage < SETTINGS_STAGE_COUNT &&
        lv_async_call(settings_stage_async, NULL) != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Settings card scheduling failed");
    }
}

static void settings_menu_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_ui.navigation_pending) return;

    s_ui.navigation_pending = true;
    if (lv_async_call(open_settings_async, NULL) != LV_RESULT_OK) {
        s_ui.navigation_pending = false;
    }
}

static void settings_home_async(void *user_data)
{
    (void)user_data;
    s_ui.navigation_pending = false;
    if (s_ui.active_screen != PRODUCT_SCREEN_SETTINGS) return;

    lv_obj_t *const settings_root = s_ui.settings.root;
    if (s_ui.settings_stage_timer != NULL) {
        lv_timer_delete(s_ui.settings_stage_timer);
        s_ui.settings_stage_timer = NULL;
    }
    if (s_ui.settings_value_bubble_timer != NULL) {
        lv_timer_delete(s_ui.settings_value_bubble_timer);
        s_ui.settings_value_bubble_timer = NULL;
    }
    np_keyboard_hide(&s_ui.keyboard);
    /* Free Settings before constructing Home to keep one scene's draw tree
     * active at a time on the LVGL task. */
    lv_obj_delete(settings_root);
    s_ui.settings = (np_settings_view_t){0};
    s_ui.settings_controls_initialized = false;
    s_ui.settings_notification_callbacks_initialized = false;
    s_ui.settings_timezone_callbacks_initialized = false;
    s_ui.settings_wifi_callbacks_initialized = false;
    s_ui.settings_rows_initialized = false;
    s_ui.settings_stage = 0U;
    s_ui.home = np_home_build(lv_screen_active());
    s_ui.home_data_rendered = false;
    np_feedback_bring_to_front(&s_ui.feedback);
    install_home_navigation_callbacks();

    app_ui_projection_t projection = {0};
    app_state_get_ui_projection(&projection);
    update_home(&projection);
    s_ui.rendered_revision = projection.revision;
    s_ui.active_screen = PRODUCT_SCREEN_HOME;
}

static void settings_home_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || s_ui.navigation_pending) return;

    s_ui.navigation_pending = true;
    if (lv_async_call(settings_home_async, NULL) != LV_RESULT_OK) {
        s_ui.navigation_pending = false;
    }
}

static void install_home_navigation_callbacks(void)
{
    lv_obj_add_event_cb(s_ui.home.header.drawer_settings_button,
                        settings_menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ui.home.header.settings_button,
                        diagnostics_button_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);
}

static void diagnostics_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_ui.refresh_timer != NULL) {
        lv_timer_delete(s_ui.refresh_timer);
    }
    np_keyboard_destroy(&s_ui.keyboard);
    np_feedback_destroy(&s_ui.feedback);

    lv_display_t *display = s_ui.display;
    lv_indev_t *touch_indev = s_ui.touch_indev;

    s_ui = (product_ui_state_t){0};

    lv_obj_clean(lv_screen_active());
    (void)diagnostic_ui_create(display, touch_indev);
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
