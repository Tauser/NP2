#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "device_control_profile.h"
#include "device_control_service.h"
#include "freertos/FreeRTOS.h"
#define ESP_ERR_TIMEOUT 0x107
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define DEVICE_CONTROL_SAVE_DELAY_MS 100U
static void test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
#define ESP_LOGI test_log
static const char *TAG = "fixture";
static int s_lock;
static device_control_status_t s_status;
static device_control_profile_t s_submitted_profile, captured;
static bool s_save_enqueued, busy;
static uint32_t s_pending_flash_sequence, sequence;
static uint8_t s_dirty_mask, s_submitted_mask;
static TickType_t s_last_change_tick, tick;
TickType_t xTaskGetTickCount(void) { return tick; }
typedef struct {
    uint32_t device_control_profile_completed_sequence;
    esp_err_t device_control_profile_last_write_result;
    bool device_control_profile_valid;
    device_control_profile_t device_control_profile;
} flash_coordinator_status_t;
static flash_coordinator_status_t flash_snapshot;
static void flash_coordinator_get_status(flash_coordinator_status_t *out) { *out = flash_snapshot; }
static esp_err_t flash_coordinator_request_device_control_profile_write(
    const device_control_profile_t *profile, uint32_t *out_sequence)
{
    if (busy) return ESP_ERR_INVALID_STATE;
    captured = *profile;
    *out_sequence = ++sequence;
    return ESP_OK;
}
#include "photo_controls_under_test.inc"
static void acknowledge(esp_err_t result)
{
    flash_snapshot = (flash_coordinator_status_t){
        .device_control_profile_completed_sequence = s_pending_flash_sequence,
        .device_control_profile_last_write_result = result,
        .device_control_profile_valid = true,
        .device_control_profile = captured,
    };
    finish_save_if_complete();
}
int main(void)
{
    assert(device_control_set_photo_selection(0) == ESP_ERR_INVALID_STATE);
    s_status = (device_control_status_t){.ready=true,.brightness_percent=26,.volume_percent=50,
        .photo_config=1,.photo_bitcoin_widget=30,.photo_clock_widget=30,
        .photo_idle_choice=3,.photo_interval_choice=4,.photo_enabled_mask=UINT64_MAX};
    assert(device_control_set_photo_settings(true,8,0,0) == ESP_ERR_INVALID_ARG);
    assert(device_control_set_photo_settings(true,0,10,0) == ESP_ERR_INVALID_ARG);
    assert(device_control_set_photo_settings(true,0,0,PHOTO_EXIT_CHOICE_COUNT) == ESP_ERR_INVALID_ARG);
    assert(device_control_set_photo_settings(false,0,0,0) == ESP_OK);
    assert(device_control_set_photo_selection((UINT64_C(1)<<63)|2U) == ESP_OK);
    tick=1000;submit_dirty_profile();assert(s_save_enqueued);
    assert(captured.photo_enabled_mask==((UINT64_C(1)<<63)|2U));
    assert(device_control_set_photo_settings(true,7,9,0)==ESP_OK);
    assert(device_control_set_photo_selection(2U)==ESP_OK);
    acknowledge(ESP_OK);
    assert(s_dirty_mask==DEVICE_CONTROL_PHOTO_MASK && s_status.persistence_pending);
    assert(s_status.save_completion_id==0);
    tick+=1000;submit_dirty_profile();assert(s_save_enqueued);
    assert(captured.photo_idle_choice==7 && captured.photo_interval_choice==9 && captured.photo_enabled_mask==2U);
    acknowledge(ESP_OK);assert(!s_dirty_mask && !s_status.persistence_pending && s_status.save_completion_id==1);
    /* A timer-only edit arriving during a save must survive its old ACK. */
    assert(device_control_set_photo_settings(true,7,9,1)==ESP_OK);
    tick+=1000;submit_dirty_profile();assert(captured.photo_exit_choice==1);
    assert(device_control_set_photo_settings(true,7,9,8)==ESP_OK);
    acknowledge(ESP_OK);assert(s_dirty_mask==DEVICE_CONTROL_PHOTO_MASK && s_status.persistence_pending);
    tick+=1000;submit_dirty_profile();assert(captured.photo_exit_choice==8);
    acknowledge(ESP_OK);assert(!s_status.persistence_pending);
    assert(device_control_set_photo_selection(0)==ESP_OK);
    busy=true;tick+=1000;submit_dirty_profile();assert(s_dirty_mask && !s_save_enqueued);
    busy=false;submit_dirty_profile();assert(s_save_enqueued);
    acknowledge(ESP_FAIL);assert(s_status.save_result==ESP_FAIL && !s_status.persistence_pending);
    puts("Photo controls: choice bounds, selection bit63, in-flight selection/timer edits retained, busy retry and failed ACK PASS");
}
