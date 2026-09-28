/*
 * Physical-maintenance provisioning bridge for the G3 open-network test.
 *
 * It accepts OPEN <ssid>, FORGET and bounded diagnostics while armed from the
 * touch UI. The
 * service intentionally has no password command, echo, or log of input. The
 * touch WPA path can retain a credential only through the protected vault,
 * after a stable connection; it never makes the password part of the serial
 * protocol, UI model or log.
 */
#include "provisioning_service.h"

#include <stdio.h>
#include <string.h>

#include "connectivity_diagnostic.h"
#include "flash_coordinator.h"
#include "network_validation_service.h"
#include "update_development.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NP2_PROVISIONING_TASK_STACK_BYTES (4U * 1024U)
#define NP2_PROVISIONING_TASK_PRIORITY 2U
#define NP2_PROVISIONING_ARM_WINDOW_MS 60000U
#define NP2_PROVISIONING_INPUT_SETTLE_MS 500U
#define NP2_PROVISIONING_POLL_MS 100U
#define NP2_PROVISIONING_LINE_BYTES 64U
#define NP2_PROVISIONING_SSID_BYTES 33U
#define NP2_PROVISIONING_PASSWORD_BYTES 64U

static const char *const TAG = "np2_provision";

static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static provisioning_service_status_t s_status = {
    .last_result = ESP_OK,
};
static int64_t s_armed_until_us;
static int64_t s_accept_commands_after_us;
static bool s_started;
/* Secret-bearing buffers are owned here, never by an LVGL text widget. */
static char s_touch_ssid[NP2_PROVISIONING_SSID_BYTES];
static char s_touch_password[NP2_PROVISIONING_PASSWORD_BYTES];
static bool s_touch_secure = true;

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) {
        *bytes++ = 0;
    }
}

static bool is_printable_ssid_char(uint8_t value)
{
    return value >= 0x20U && value <= 0x7eU;
}

static bool is_command_whitespace(uint8_t value)
{
    return value == ' ' || value == '\t';
}

static void disarm_with_result(esp_err_t result)
{
    taskENTER_CRITICAL(&s_status_lock);
    s_armed_until_us = 0;
    s_accept_commands_after_us = 0;
    s_status.armed = false;
    s_status.remaining_seconds = 0;
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
}

static bool is_armed(void)
{
    const int64_t now_us = esp_timer_get_time();
    bool armed = false;
    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.armed && now_us < s_armed_until_us) {
        const int64_t remaining_us = s_armed_until_us - now_us;
        s_status.remaining_seconds = (uint32_t)((remaining_us + 999999LL) / 1000000LL);
        armed = true;
    } else if (s_status.armed) {
        s_armed_until_us = 0;
        s_status.armed = false;
        s_status.remaining_seconds = 0;
        s_status.last_result = ESP_ERR_TIMEOUT;
    }
    taskEXIT_CRITICAL(&s_status_lock);
    return armed;
}

static bool is_accepting_commands(void)
{
    const int64_t now_us = esp_timer_get_time();
    bool accepting = false;
    taskENTER_CRITICAL(&s_status_lock);
    accepting = s_status.armed && now_us >= s_accept_commands_after_us;
    taskEXIT_CRITICAL(&s_status_lock);
    return accepting;
}

static void discard_pending_usb_input(void)
{
    uint8_t discarded[NP2_PROVISIONING_LINE_BYTES];
    while (usb_serial_jtag_read_bytes(discarded, sizeof(discarded), 0) > 0) {
    }
    secure_zero(discarded, sizeof(discarded));
}

static void write_reply(const char *message)
{
    (void)usb_serial_jtag_write_bytes(message, strlen(message), pdMS_TO_TICKS(20));
    (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(20));
}

static void write_result_reply(esp_err_t result)
{
    if (result == ESP_OK) {
        write_reply("OK\n");
        return;
    }

    char reply[48] = {0};
    const int written = snprintf(reply, sizeof(reply), "ERR %s\n", esp_err_to_name(result));
    if (written > 0) {
        write_reply(reply);
    }
    secure_zero(reply, sizeof(reply));
}

static void process_line(uint8_t *line, size_t length)
{
    esp_err_t result = ESP_ERR_INVALID_ARG;
    char ssid[NP2_PROVISIONING_SSID_BYTES] = {0};
    const uint8_t *command = line;
    size_t command_length = length;

    if (!is_armed()) {
        secure_zero(line, length);
        return;
    }

    while (command_length > 0U && is_command_whitespace(*command)) {
        ++command;
        --command_length;
    }
    while (command_length > 0U && is_command_whitespace(command[command_length - 1U])) {
        --command_length;
    }

    if (command_length > 5U && memcmp(command, "OPEN ", 5U) == 0) {
        const size_t ssid_length = command_length - 5U;
        if (ssid_length <= 32U) {
            bool valid = true;
            for (size_t i = 0; i < ssid_length; ++i) {
                if (!is_printable_ssid_char(command[5U + i])) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                memcpy(ssid, command + 5U, ssid_length);
                result = connectivity_diagnostic_request_join(ssid, "");
            }
        }
    } else if (command_length == 10U && memcmp(command, "OTA_STATUS", 10U) == 0) {
        update_development_log_status();
        result = ESP_OK;
    } else if (command_length == 13U && memcmp(command, "OTA_PREFLIGHT", 13U) == 0) {
        result = update_development_request(false);
    } else if (command_length == 9U && memcmp(command, "OTA_APPLY", 9U) == 0) {
        result = update_development_request(true);
    } else if (command_length == 6U && memcmp(command, "FORGET", 6U) == 0) {
        result = connectivity_diagnostic_request_forget();
        ESP_LOGI(TAG, "maintenance command=FORGET result=%s", esp_err_to_name(result));
    } else if (command_length == 10U && memcmp(command, "RECOVER_C6", 10U) == 0) {
        result = connectivity_diagnostic_request_hosted_recovery();
        ESP_LOGI(TAG, "maintenance command=RECOVER_C6 result=%s", esp_err_to_name(result));
    } else if (command_length == 19U && memcmp(command, "RECOVER_C6_COOLDOWN", 19U) == 0) {
        result = connectivity_diagnostic_request_hosted_recovery_cooldown_campaign();
        ESP_LOGI(TAG, "maintenance command=RECOVER_C6_COOLDOWN result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 24U && memcmp(command, "RECOVER_C6_COOLDOWN_FULL", 24U) == 0) {
        result = connectivity_diagnostic_request_hosted_recovery_full_cooldown_campaign();
        ESP_LOGI(TAG, "maintenance command=RECOVER_C6_COOLDOWN_FULL result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 18U && memcmp(command, "CHECK_DHCP_TIMEOUT", 18U) == 0) {
        result = connectivity_diagnostic_request_dhcp_silence_injection();
        ESP_LOGI(TAG, "maintenance command=CHECK_DHCP_TIMEOUT result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 20U && memcmp(command, "CACHE_CORRUPT_NEWEST", 20U) == 0) {
        result = flash_coordinator_request_cache_corrupt_newest();
        ESP_LOGI(TAG, "maintenance command=CACHE_CORRUPT_NEWEST result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 16U && memcmp(command, "CACHE_FULL_PROBE", 16U) == 0) {
        result = flash_coordinator_request_cache_full_probe();
        ESP_LOGI(TAG, "maintenance command=CACHE_FULL_PROBE result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 23U && memcmp(command, "CACHE_CUT_BEFORE_RENAME", 23U) == 0) {
        result = flash_coordinator_request_cache_cut_before_rename();
        ESP_LOGI(TAG, "maintenance command=CACHE_CUT_BEFORE_RENAME result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 22U && memcmp(command, "CACHE_CUT_AFTER_RENAME", 22U) == 0) {
        result = flash_coordinator_request_cache_cut_after_rename();
        ESP_LOGI(TAG, "maintenance command=CACHE_CUT_AFTER_RENAME result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 12U && memcmp(command, "CONFIG_WRITE", 12U) == 0) {
        result = flash_coordinator_request_config_journal_write();
        ESP_LOGI(TAG, "maintenance command=CONFIG_WRITE result=%s", esp_err_to_name(result));
    } else if (command_length == 21U && memcmp(command, "CONFIG_CORRUPT_NEWEST", 21U) == 0) {
        result = flash_coordinator_request_config_corrupt_newest();
        ESP_LOGI(TAG, "maintenance command=CONFIG_CORRUPT_NEWEST result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 20U && memcmp(command, "REFRESH_OFFLINE_DATA", 20U) == 0) {
        result = network_validation_service_request_offline_data_refresh();
        ESP_LOGI(TAG, "maintenance command=REFRESH_OFFLINE_DATA result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 5U && memcmp(command, "CHECK", 5U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_NORMAL);
        ESP_LOGI(TAG, "maintenance command=CHECK result=%s", esp_err_to_name(result));
    } else if (command_length == 11U && memcmp(command, "CHECK_NXDNS", 11U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_DNS_NXDOMAIN);
        ESP_LOGI(TAG, "maintenance command=CHECK_NXDNS result=%s", esp_err_to_name(result));
    } else if (command_length == 17U && memcmp(command, "CHECK_DNS_TIMEOUT", 17U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_DNS_TIMEOUT);
        ESP_LOGI(TAG, "maintenance command=CHECK_DNS_TIMEOUT result=%s", esp_err_to_name(result));
    } else if (command_length == 16U && memcmp(command, "CHECK_TLS_REJECT", 16U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_TLS_REJECT);
        ESP_LOGI(TAG, "maintenance command=CHECK_TLS_REJECT result=%s", esp_err_to_name(result));
    } else if (command_length == 19U && memcmp(command, "CHECK_HTTPS_TIMEOUT", 19U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_HTTPS_TIMEOUT);
        ESP_LOGI(TAG, "maintenance command=CHECK_HTTPS_TIMEOUT result=%s",
                 esp_err_to_name(result));
    } else if (command_length == 20U && memcmp(command, "CHECK_HTTPS_OVERSIZE", 20U) == 0) {
        result = network_validation_service_request_check(NETWORK_VALIDATION_MODE_HTTPS_OVERSIZE);
        ESP_LOGI(TAG, "maintenance command=CHECK_HTTPS_OVERSIZE result=%s",
                 esp_err_to_name(result));
    } else {
        ESP_LOGW(TAG, "maintenance command rejected: length=%u", (unsigned int)command_length);
    }

    secure_zero(ssid, sizeof(ssid));
    secure_zero(line, length);
    disarm_with_result(result);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "maintenance request rejected: %s", esp_err_to_name(result));
    }
    write_result_reply(result);
}

static void provisioning_task(void *arg)
{
    (void)arg;
    uint8_t line[NP2_PROVISIONING_LINE_BYTES] = {0};
    size_t length = 0;

    for (;;) {
        if (!is_armed()) {
            secure_zero(line, sizeof(line));
            length = 0;
            vTaskDelay(pdMS_TO_TICKS(NP2_PROVISIONING_POLL_MS));
            continue;
        }

        if (!is_accepting_commands()) {
            discard_pending_usb_input();
            secure_zero(line, sizeof(line));
            length = 0;
            vTaskDelay(pdMS_TO_TICKS(NP2_PROVISIONING_POLL_MS));
            continue;
        }

        uint8_t byte = 0;
        const int read = usb_serial_jtag_read_bytes(&byte, 1U, pdMS_TO_TICKS(NP2_PROVISIONING_POLL_MS));
        if (read != 1) {
            continue;
        }
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            process_line(line, length);
            length = 0;
            continue;
        }
        if (length + 1U >= sizeof(line)) {
            secure_zero(line, sizeof(line));
            length = 0;
            disarm_with_result(ESP_ERR_INVALID_SIZE);
            ESP_LOGW(TAG, "open-network maintenance request rejected: %s",
                     esp_err_to_name(ESP_ERR_INVALID_SIZE));
            write_result_reply(ESP_ERR_INVALID_SIZE);
            continue;
        }
        line[length++] = byte;
    }
}

esp_err_t provisioning_service_start(void)
{
    taskENTER_CRITICAL(&s_status_lock);
    if (s_started) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    taskEXIT_CRITICAL(&s_status_lock);

    if (usb_serial_jtag_is_driver_installed()) {
        disarm_with_result(ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }

    usb_serial_jtag_driver_config_t usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    const esp_err_t install_result = usb_serial_jtag_driver_install(&usb_config);
    if (install_result != ESP_OK) {
        disarm_with_result(install_result);
        return install_result;
    }

    const BaseType_t task_created = xTaskCreate(provisioning_task, "np2_provision",
                                                 NP2_PROVISIONING_TASK_STACK_BYTES, NULL,
                                                 NP2_PROVISIONING_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        (void)usb_serial_jtag_driver_uninstall();
        disarm_with_result(ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t provisioning_service_arm_open_network(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_status_lock);
    const int64_t now_us = esp_timer_get_time();
    s_accept_commands_after_us = now_us + (int64_t)NP2_PROVISIONING_INPUT_SETTLE_MS * 1000LL;
    s_armed_until_us = now_us + (int64_t)NP2_PROVISIONING_ARM_WINDOW_MS * 1000LL;
    s_status.armed = true;
    s_status.remaining_seconds = NP2_PROVISIONING_ARM_WINDOW_MS / 1000U;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t provisioning_service_touch_begin(void)
{
    if (!s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_status_lock);
    secure_zero(s_touch_ssid, sizeof(s_touch_ssid));
    secure_zero(s_touch_password, sizeof(s_touch_password));
    s_status.touch_active = true;
    s_status.touch_stage = PROVISIONING_TOUCH_STAGE_SSID;
    s_touch_secure = true;
    s_status.touch_ssid_length = 0;
    s_status.touch_password_length = 0;
    s_status.touch_password_visible = false;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t provisioning_service_touch_begin_for_ssid(const char *ssid)
{
    return provisioning_service_touch_begin_for_network(ssid, true);
}

esp_err_t provisioning_service_touch_begin_for_network(const char *ssid, bool secure)
{
    if (ssid == NULL || !s_started) return ESP_ERR_INVALID_ARG;
    const size_t length = strnlen(ssid, NP2_PROVISIONING_SSID_BYTES);
    if (length == 0U || length >= NP2_PROVISIONING_SSID_BYTES) return ESP_ERR_INVALID_SIZE;

    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.touch_active) {
        taskEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }
    secure_zero(s_touch_ssid, sizeof(s_touch_ssid));
    secure_zero(s_touch_password, sizeof(s_touch_password));
    memcpy(s_touch_ssid, ssid, length);
    s_status.touch_active = true;
    s_status.touch_stage = PROVISIONING_TOUCH_STAGE_PASSWORD;
    s_touch_secure = secure;
    s_status.touch_ssid_length = (uint8_t)length;
    s_status.touch_password_length = 0U;
    s_status.touch_password_visible = false;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

esp_err_t provisioning_service_touch_append_ssid(char character)
{
    if (!is_printable_ssid_char((uint8_t)character)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_SSID) {
        result = ESP_ERR_INVALID_STATE;
    } else if (s_status.touch_ssid_length >= NP2_PROVISIONING_SSID_BYTES - 1U) {
        result = ESP_ERR_INVALID_SIZE;
    } else {
        s_touch_ssid[s_status.touch_ssid_length++] = character;
        s_touch_ssid[s_status.touch_ssid_length] = '\0';
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t provisioning_service_touch_backspace_ssid(void)
{
    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_SSID) {
        result = ESP_ERR_INVALID_STATE;
    } else if (s_status.touch_ssid_length > 0U) {
        --s_status.touch_ssid_length;
        s_touch_ssid[s_status.touch_ssid_length] = '\0';
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t provisioning_service_touch_begin_password(void)
{
    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_SSID ||
        s_status.touch_ssid_length == 0U) {
        result = ESP_ERR_INVALID_STATE;
    } else {
        secure_zero(s_touch_password, sizeof(s_touch_password));
        s_status.touch_password_length = 0;
        s_status.touch_password_visible = false;
        s_status.touch_stage = PROVISIONING_TOUCH_STAGE_PASSWORD;
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t provisioning_service_touch_append_password(char character)
{
    if (!is_printable_ssid_char((uint8_t)character)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_PASSWORD) {
        result = ESP_ERR_INVALID_STATE;
    } else if (s_status.touch_password_length >= NP2_PROVISIONING_PASSWORD_BYTES - 1U) {
        result = ESP_ERR_INVALID_SIZE;
    } else {
        s_touch_password[s_status.touch_password_length++] = character;
        s_touch_password[s_status.touch_password_length] = '\0';
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t provisioning_service_touch_backspace_password(void)
{
    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_PASSWORD) {
        result = ESP_ERR_INVALID_STATE;
    } else if (s_status.touch_password_length > 0U) {
        --s_status.touch_password_length;
        s_touch_password[s_status.touch_password_length] = '\0';
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);
    return result;
}

esp_err_t provisioning_service_touch_set_password_visible(bool visible)
{
    taskENTER_CRITICAL(&s_status_lock);
    const bool active = s_status.touch_active &&
                        s_status.touch_stage == PROVISIONING_TOUCH_STAGE_PASSWORD;
    s_status.touch_password_visible = active && visible;
    taskEXIT_CRITICAL(&s_status_lock);
    return active ? ESP_OK : ESP_ERR_INVALID_STATE;
}

uint32_t provisioning_service_touch_display_character(uint8_t index)
{
    uint32_t character = 0U;
    taskENTER_CRITICAL(&s_status_lock);
    if (s_status.touch_active && s_status.touch_stage == PROVISIONING_TOUCH_STAGE_PASSWORD &&
        index < s_status.touch_password_length) {
        character = s_status.touch_password_visible ? (uint8_t)s_touch_password[index] : '*';
    }
    taskEXIT_CRITICAL(&s_status_lock);
    return character;
}

esp_err_t provisioning_service_touch_submit(void)
{
    char ssid[NP2_PROVISIONING_SSID_BYTES] = {0};
    char password[NP2_PROVISIONING_PASSWORD_BYTES] = {0};
    esp_err_t result = ESP_OK;

    taskENTER_CRITICAL(&s_status_lock);
    if (!s_status.touch_active || s_status.touch_stage != PROVISIONING_TOUCH_STAGE_PASSWORD ||
        s_status.touch_ssid_length == 0U ||
        (s_touch_secure ? s_status.touch_password_length < 8U
                        : s_status.touch_password_length != 0U)) {
        result = ESP_ERR_INVALID_ARG;
    } else {
        memcpy(ssid, s_touch_ssid, sizeof(ssid));
        memcpy(password, s_touch_password, sizeof(password));
        secure_zero(s_touch_ssid, sizeof(s_touch_ssid));
        secure_zero(s_touch_password, sizeof(s_touch_password));
        s_status.touch_active = false;
        s_status.touch_stage = PROVISIONING_TOUCH_STAGE_IDLE;
        s_status.touch_ssid_length = 0;
        s_status.touch_password_length = 0;
        s_status.touch_password_visible = false;
    }
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_status_lock);

    if (result == ESP_OK) {
        result = connectivity_diagnostic_request_join(ssid, password);
        taskENTER_CRITICAL(&s_status_lock);
        s_status.last_result = result;
        taskEXIT_CRITICAL(&s_status_lock);
    }
    secure_zero(ssid, sizeof(ssid));
    secure_zero(password, sizeof(password));
    return result;
}

void provisioning_service_touch_cancel(void)
{
    taskENTER_CRITICAL(&s_status_lock);
    secure_zero(s_touch_ssid, sizeof(s_touch_ssid));
    secure_zero(s_touch_password, sizeof(s_touch_password));
    s_status.touch_active = false;
    s_status.touch_stage = PROVISIONING_TOUCH_STAGE_IDLE;
    s_status.touch_ssid_length = 0;
    s_status.touch_password_length = 0;
    s_status.touch_password_visible = false;
    s_status.last_result = ESP_OK;
    taskEXIT_CRITICAL(&s_status_lock);
}

esp_err_t provisioning_service_touch_copy_ssid(char *out_ssid, size_t out_size)
{
    if (out_ssid == NULL || out_size < NP2_PROVISIONING_SSID_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    taskENTER_CRITICAL(&s_status_lock);
    memcpy(out_ssid, s_touch_ssid, sizeof(s_touch_ssid));
    taskEXIT_CRITICAL(&s_status_lock);
    return ESP_OK;
}

void provisioning_service_get_status(provisioning_service_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    (void)is_armed();
    taskENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_status_lock);
}
