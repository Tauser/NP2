/* Exercises the actual service, with platform I/O discarded by the linker. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/main/provisioning_service.c"

static esp_err_t join_result = ESP_OK;
static size_t join_password_length;
static unsigned joins;

esp_err_t connectivity_diagnostic_request_join(const char *ssid, const char *password)
{
    assert(strcmp(ssid, "host-test") == 0);
    join_password_length = strlen(password);
    ++joins;
    return join_result;
}

static void assert_wiped(void)
{
    for (size_t i = 0; i < sizeof(s_touch_password); ++i) assert(s_touch_password[i] == 0);
    for (size_t i = 0; i < sizeof(s_touch_ssid); ++i) assert(s_touch_ssid[i] == 0);
    assert(!s_status.touch_active && !s_status.touch_password_visible);
    assert(s_status.touch_password_length == 0);
    assert(provisioning_service_touch_display_character(0) == 0);
}

int main(void)
{
    assert(provisioning_service_touch_begin_for_ssid("host-test") != ESP_OK);
    s_started = true;
    assert(provisioning_service_touch_begin_for_ssid("host-test") == ESP_OK);
    assert(provisioning_service_touch_begin_for_ssid("host-test") != ESP_OK);
    assert(provisioning_service_touch_append_password('\n') == ESP_ERR_INVALID_ARG);
    assert(provisioning_service_touch_append_password((char)0x80) == ESP_ERR_INVALID_ARG);
    for (unsigned i = 0; i < 7; ++i) assert(provisioning_service_touch_append_password('x') == ESP_OK);
    assert(provisioning_service_touch_submit() == ESP_ERR_INVALID_ARG);
    assert(joins == 0 && s_status.touch_active);
    assert(provisioning_service_touch_display_character(0) == '*');
    assert(provisioning_service_touch_set_password_visible(true) == ESP_OK);
    assert(provisioning_service_touch_display_character(0) == 'x');
    assert(provisioning_service_touch_display_character(7) == 0);
    assert(provisioning_service_touch_set_password_visible(false) == ESP_OK);
    assert(provisioning_service_touch_append_password(' ') == ESP_OK);
    assert(provisioning_service_touch_submit() == ESP_OK);
    assert(joins == 1 && join_password_length == 8);
    assert_wiped();
    assert(provisioning_service_touch_set_password_visible(true) == ESP_ERR_INVALID_STATE);

    assert(provisioning_service_touch_begin_for_ssid("host-test") == ESP_OK);
    for (unsigned i = 0; i < 63; ++i) assert(provisioning_service_touch_append_password('A') == ESP_OK);
    assert(provisioning_service_touch_append_password('B') == ESP_ERR_INVALID_SIZE);
    assert(provisioning_service_touch_set_password_visible(true) == ESP_OK);
    assert(provisioning_service_touch_display_character(62) == 'A');
    assert(provisioning_service_touch_backspace_password() == ESP_OK);
    assert(s_touch_password[62] == 0 && s_status.touch_password_length == 62);
    assert(provisioning_service_touch_display_character(62) == 0);
    provisioning_service_touch_cancel();
    assert_wiped();

    assert(provisioning_service_touch_begin_for_network("host-test", false) == ESP_OK);
    assert(provisioning_service_touch_submit() == ESP_OK);
    assert(join_password_length == 0);
    assert_wiped();
    assert(provisioning_service_touch_begin_for_network("host-test", false) == ESP_OK);
    assert(provisioning_service_touch_append_password('x') == ESP_OK);
    assert(provisioning_service_touch_submit() == ESP_ERR_INVALID_ARG);
    provisioning_service_touch_cancel();

    assert(provisioning_service_touch_begin_for_ssid("host-test") == ESP_OK);
    assert(!s_status.touch_password_visible);
    for (unsigned i = 0; i < 8; ++i) assert(provisioning_service_touch_append_password('x') == ESP_OK);
    join_result = ESP_ERR_INVALID_STATE;
    assert(provisioning_service_touch_submit() == ESP_ERR_INVALID_STATE);
    assert_wiped();
    puts("Provisioning touch: validation, masking, reveal, limits and zeroization passed");
    return 0;
}
