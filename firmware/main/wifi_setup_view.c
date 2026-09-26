/*
 * WPA2 setup view for the temporary G3 RAM-only credential path.
 *
 * LVGL owns only presentation: an SSID preview and password bullet count.
 * The provisioning service owns the actual SSID/password buffers and sends a
 * private copy to the connectivity worker when the user submits.
 */
#include "wifi_setup_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "provisioning_service.h"

#define WIFI_SETUP_KEY_COUNT 30U
#define WIFI_SETUP_SSID_BYTES 33U
#define WIFI_SETUP_PASSWORD_BYTES 64U

typedef enum {
    WIFI_SETUP_KEYS_LOWER = 0,
    WIFI_SETUP_KEYS_UPPER,
    WIFI_SETUP_KEYS_SYMBOLS,
} wifi_setup_keys_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *ssid_label;
    lv_obj_t *password_label;
    lv_obj_t *hint_label;
    lv_obj_t *key_buttons[WIFI_SETUP_KEY_COUNT];
    lv_obj_t *key_labels[WIFI_SETUP_KEY_COUNT];
    wifi_setup_keys_t key_mode;
} wifi_setup_view_state_t;

static wifi_setup_view_state_t s_view;

static const char s_lower_keys[] = "qwertyuiopasdfghjklzxcvbnm";
static const char s_upper_keys[] = "QWERTYUIOPASDFGHJKLZXCVBNM";
static const char s_symbol_keys[] = "1234567890-_.!@#$%&*";

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) {
        *bytes++ = 0;
    }
}

static const char *active_key_set(void)
{
    switch (s_view.key_mode) {
    case WIFI_SETUP_KEYS_UPPER:
        return s_upper_keys;
    case WIFI_SETUP_KEYS_SYMBOLS:
        return s_symbol_keys;
    case WIFI_SETUP_KEYS_LOWER:
    default:
        return s_lower_keys;
    }
}

static void set_hint_result(const char *prefix, esp_err_t result)
{
    lv_label_set_text_fmt(s_view.hint_label, "%s: %s", prefix, esp_err_to_name(result));
}

static void refresh_view(void)
{
    provisioning_service_status_t status = {0};
    char ssid[WIFI_SETUP_SSID_BYTES] = {0};
    char bullets[WIFI_SETUP_PASSWORD_BYTES] = {0};
    provisioning_service_get_status(&status);
    (void)provisioning_service_touch_copy_ssid(ssid, sizeof(ssid));

    lv_label_set_text_fmt(s_view.ssid_label, "Rede: %s", ssid[0] == '\0' ? "(informe o SSID)" : ssid);
    const uint8_t bullets_count = status.touch_password_length < sizeof(bullets) - 1U
                                      ? status.touch_password_length
                                      : sizeof(bullets) - 1U;
    memset(bullets, '*', bullets_count);
    lv_label_set_text_fmt(s_view.password_label, "Senha: %s", bullets_count == 0U ? "(minimo 8)" : bullets);

    if (status.touch_stage == PROVISIONING_TOUCH_STAGE_SSID) {
        lv_label_set_text(s_view.hint_label, "Informe o nome da rede e toque em PROXIMO");
    } else if (status.touch_stage == PROVISIONING_TOUCH_STAGE_PASSWORD) {
        lv_label_set_text_fmt(s_view.hint_label, "Senha WPA2: %u/63 caracteres", (unsigned int)bullets_count);
    }

    const char *const keys = active_key_set();
    const size_t key_count = strlen(keys);
    for (size_t index = 0; index < WIFI_SETUP_KEY_COUNT; ++index) {
        if (index < key_count) {
            char key[2] = {keys[index], '\0'};
            lv_label_set_text(s_view.key_labels[index], key);
            lv_obj_remove_flag(s_view.key_buttons[index], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_view.key_buttons[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
    secure_zero(ssid, sizeof(ssid));
    secure_zero(bullets, sizeof(bullets));
}

static void destroy_view_async(void)
{
    lv_obj_t *const overlay = s_view.overlay;
    memset(&s_view, 0, sizeof(s_view));
    if (overlay != NULL) {
        lv_obj_delete_async(overlay);
    }
}

static void close_view(void)
{
    provisioning_service_touch_cancel();
    destroy_view_async();
}

static void key_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    provisioning_service_status_t status = {0};
    provisioning_service_get_status(&status);
    lv_obj_t *const button = lv_event_get_target(event);
    lv_obj_t *const label = lv_obj_get_child(button, 0);
    const char *const key = label == NULL ? NULL : lv_label_get_text(label);
    if (key == NULL || key[0] == '\0' || key[1] != '\0') {
        return;
    }

    const esp_err_t result = status.touch_stage == PROVISIONING_TOUCH_STAGE_PASSWORD
                                 ? provisioning_service_touch_append_password(key[0])
                                 : provisioning_service_touch_append_ssid(key[0]);
    if (result != ESP_OK) {
        set_hint_result("Entrada recusada", result);
        return;
    }
    refresh_view();
}

static void mode_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    s_view.key_mode = (wifi_setup_keys_t)(uintptr_t)lv_event_get_user_data(event);
    refresh_view();
}

static void backspace_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    provisioning_service_status_t status = {0};
    provisioning_service_get_status(&status);
    const esp_err_t result = status.touch_stage == PROVISIONING_TOUCH_STAGE_PASSWORD
                                 ? provisioning_service_touch_backspace_password()
                                 : provisioning_service_touch_backspace_ssid();
    if (result != ESP_OK) {
        set_hint_result("Apagar recusado", result);
        return;
    }
    refresh_view();
}

static void next_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    const esp_err_t result = provisioning_service_touch_begin_password();
    if (result != ESP_OK) {
        set_hint_result("Informe uma rede", result);
        return;
    }
    refresh_view();
}

static void connect_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    const esp_err_t result = provisioning_service_touch_submit();
    if (result != ESP_OK) {
        set_hint_result("Senha WPA2 deve ter 8 a 63 caracteres", result);
        return;
    }
    /* touch_submit has wiped the owned secret buffers before returning. */
    destroy_view_async();
}

static void cancel_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        close_view();
    }
}

static lv_obj_t *create_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t width,
                               int32_t height, const char *text, lv_event_cb_t callback,
                               void *user_data)
{
    lv_obj_t *const button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x183554), LV_PART_MAIN);
    lv_obj_t *const label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    return button;
}

esp_err_t wifi_setup_view_open(lv_obj_t *parent)
{
    if (parent == NULL || s_view.overlay != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t begin_result = provisioning_service_touch_begin();
    if (begin_result != ESP_OK) {
        return begin_result;
    }

    s_view.overlay = lv_obj_create(parent);
    if (s_view.overlay == NULL) {
        provisioning_service_touch_cancel();
        return ESP_ERR_NO_MEM;
    }
    lv_obj_set_size(s_view.overlay, 1024, 600);
    lv_obj_set_pos(s_view.overlay, 0, 0);
    lv_obj_set_style_bg_color(s_view.overlay, lv_color_hex(0x09111F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_view.overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_view.overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_view.overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *const panel = lv_obj_create(s_view.overlay);
    lv_obj_set_size(panel, 920, 540);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x10243D), LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x6491C6), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *const title = lv_label_create(panel);
    lv_label_set_text(title, "CONFIGURAR WI-FI WPA2");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_pos(title, 28, 18);

    s_view.ssid_label = lv_label_create(panel);
    lv_obj_set_style_text_color(s_view.ssid_label, lv_color_hex(0x68E0B8), LV_PART_MAIN);
    lv_obj_set_pos(s_view.ssid_label, 28, 58);

    s_view.password_label = lv_label_create(panel);
    lv_obj_set_style_text_color(s_view.password_label, lv_color_hex(0xF4F7FB), LV_PART_MAIN);
    lv_obj_set_pos(s_view.password_label, 28, 88);

    s_view.hint_label = lv_label_create(panel);
    lv_obj_set_style_text_color(s_view.hint_label, lv_color_hex(0x9DB4D1), LV_PART_MAIN);
    lv_obj_set_pos(s_view.hint_label, 28, 118);

    for (size_t index = 0; index < WIFI_SETUP_KEY_COUNT; ++index) {
        const int32_t row = (int32_t)(index / 10U);
        const int32_t column = (int32_t)(index % 10U);
        lv_obj_t *const button = create_button(panel, 28 + column * 86, 160 + row * 54,
                                                76, 44, "", key_event_cb, NULL);
        s_view.key_buttons[index] = button;
        s_view.key_labels[index] = lv_obj_get_child(button, 0);
    }

    (void)create_button(panel, 28, 330, 82, 42, "abc", mode_event_cb,
                        (void *)(uintptr_t)WIFI_SETUP_KEYS_LOWER);
    (void)create_button(panel, 120, 330, 82, 42, "ABC", mode_event_cb,
                        (void *)(uintptr_t)WIFI_SETUP_KEYS_UPPER);
    (void)create_button(panel, 212, 330, 82, 42, "123", mode_event_cb,
                        (void *)(uintptr_t)WIFI_SETUP_KEYS_SYMBOLS);
    (void)create_button(panel, 304, 330, 120, 42, "APAGAR", backspace_event_cb, NULL);
    (void)create_button(panel, 434, 330, 130, 42, "PROXIMO", next_event_cb, NULL);
    (void)create_button(panel, 574, 330, 140, 42, "CONECTAR", connect_event_cb, NULL);
    (void)create_button(panel, 724, 330, 150, 42, "CANCELAR", cancel_event_cb, NULL);

    lv_obj_t *const note = lv_label_create(panel);
    lv_label_set_text(note, "Senha somente em RAM; nao e gravada, exibida ou enviada por USB.");
    lv_obj_set_style_text_color(note, lv_color_hex(0xF4C95D), LV_PART_MAIN);
    lv_obj_set_pos(note, 28, 408);

    s_view.key_mode = WIFI_SETUP_KEYS_LOWER;
    refresh_view();
    return ESP_OK;
}
