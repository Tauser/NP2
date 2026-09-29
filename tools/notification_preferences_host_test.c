/* Actual service; only FreeRTOS, coordinator storage and audio are simulated.
 * This does not qualify NVS durability or physical power cycling. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/main/notification_service.c"

static flash_coordinator_status_t flash;
static notification_profile_t queued;
static uint32_t sequence;
static unsigned writes;
static unsigned sounds;
static esp_err_t submission_result = ESP_OK;
static bool write_in_flight;

void flash_coordinator_get_status(flash_coordinator_status_t *out)
{
    *out = flash;
}

esp_err_t flash_coordinator_request_notification_profile_write(
    const notification_profile_t *profile, uint32_t *out_sequence)
{
    if (submission_result != ESP_OK) return submission_result;
    assert(!write_in_flight);
    queued = *profile;
    *out_sequence = ++sequence;
    write_in_flight = true;
    ++writes;
    return ESP_OK;
}

esp_err_t device_control_play_notification_tone(void)
{
    ++sounds;
    return ESP_OK;
}

static void complete_write(esp_err_t result)
{
    assert(write_in_flight);
    flash.notification_profile_completed_sequence = sequence;
    flash.notification_profile_last_write_result = result;
    if (result == ESP_OK) {
        flash.notification_profile = queued;
        flash.notification_profile_valid = true;
        ++flash.notification_profile_generation;
    }
    write_in_flight = false;
    restore_preferences_if_available();
}

static void reboot_service(void)
{
    assert(!write_in_flight);
    s_task = NULL;
    s_started = false;
    s_restore_applied = false;
    s_save_enqueued = false;
    s_pending_flash_sequence = 0U;
    s_submitted_profile = (notification_profile_t){0};
    s_status = (notification_service_status_t){
        .general_enabled = true, .sound_enabled = true,
        .system_alerts_enabled = true, .last_result = ESP_OK};
    assert(notification_service_start() == ESP_OK);
    assert(notification_service_set_general_enabled(false) == ESP_ERR_INVALID_STATE);
    restore_preferences_if_available();
}

static void set_profile(unsigned mask)
{
    assert(notification_service_set_general_enabled((mask & 1U) != 0) == ESP_OK);
    assert(notification_service_set_sound_enabled((mask & 2U) != 0) == ESP_OK);
    assert(notification_service_set_system_alerts_enabled((mask & 4U) != 0) == ESP_OK);
}

static void expect_profile(unsigned mask, bool pending)
{
    notification_service_status_t status;
    notification_service_get_status(&status);
    assert(status.ready);
    assert(status.general_enabled == ((mask & 1U) != 0));
    assert(status.sound_enabled == ((mask & 2U) != 0));
    assert(status.system_alerts_enabled == ((mask & 4U) != 0));
    assert(status.persistence_pending == pending);
}

int main(void)
{
    flash.ready = true;
    flash.notification_profile_valid = true;
    flash.notification_profile_generation = 1U;
    flash.notification_profile = (notification_profile_t){true, true, true};
    reboot_service();
    expect_profile(7U, false);

    /* Three edits before the worker submission produce a single final profile.
     * Exercise all combinations, commit acknowledgement and cold restoration. */
    const unsigned profiles[] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
        const unsigned before = writes;
        set_profile(profiles[i]);
        expect_profile(profiles[i], true);
        submit_pending_profile();
        submit_pending_profile();
        assert(writes == before + 1U);
        complete_write(ESP_OK);
        expect_profile(profiles[i], false);
        reboot_service();
        expect_profile(profiles[i], false);
        assert(s_status.persisted_generation == flash.notification_profile_generation);
    }

    /* New preference during an in-flight write must remain dirty after its ACK. */
    set_profile(0U);
    submit_pending_profile();
    const unsigned before = writes;
    set_profile(5U);
    submit_pending_profile();
    assert(writes == before);
    complete_write(ESP_OK);
    expect_profile(5U, true);
    submit_pending_profile();
    assert(writes == before + 1U);
    complete_write(ESP_OK);
    reboot_service();
    expect_profile(5U, false);

    /* Failed submission/commit must not claim a persisted new generation. */
    const uint32_t persisted = s_status.persisted_generation;
    set_profile(0U);
    submission_result = ESP_ERR_TIMEOUT;
    submit_pending_profile();
    assert(s_status.last_result == ESP_ERR_TIMEOUT && !s_status.persistence_pending);
    assert(s_status.persisted_generation == persisted);
    submission_result = ESP_OK;
    reboot_service();
    expect_profile(5U, false);
    set_profile(0U);
    submit_pending_profile();
    complete_write(ESP_FAIL);
    assert(s_status.last_result == ESP_FAIL && s_status.persisted_generation == persisted);
    reboot_service();
    expect_profile(5U, false);

    assert(notification_service_request_alert_sound() == ESP_ERR_INVALID_STATE);
    assert(sounds == 0U);
    set_profile(3U);
    assert(notification_service_request_alert_sound() == ESP_OK);
    assert(sounds == 1U);
    puts("Notification service: restoration, coalescing, in-flight edits, failures and sound: PASS");
    return 0;
}
