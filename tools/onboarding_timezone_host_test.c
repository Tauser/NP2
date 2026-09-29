#include <assert.h>
#include <stdio.h>
#include "../firmware/main/onboarding_service.c"

static flash_coordinator_status_t storage;
static esp_err_t enqueue_result = ESP_OK;
static onboarding_profile_t queued;
static unsigned writes;

bool timezone_catalog_is_valid(uint16_t index) { return index < TIMEZONE_CATALOG_COUNT; }
void flash_coordinator_get_status(flash_coordinator_status_t *out) { *out = storage; }
esp_err_t flash_coordinator_request_onboarding_profile_write(const onboarding_profile_t *profile)
{
    ++writes;
    if (enqueue_result != ESP_OK) return enqueue_result;
    queued = *profile;
    storage.pending = true;
    return ESP_OK;
}

static void expect(uint16_t index, bool pending)
{
    onboarding_service_status_t status;
    onboarding_service_get_status(&status);
    assert(status.timezone_index == index);
    assert(status.timezone_persistence_pending == pending);
}

static void finish_write(void)
{
    storage.onboarding_profile = queued;
    storage.pending = false;
    storage.busy = false;
    onboarding_service_refresh();
}

int main(void)
{
    storage.ready = true;
    storage.onboarding_profile_valid = true;
    storage.onboarding_profile = (onboarding_profile_t){true, true, 0};
    assert(onboarding_service_start() == ESP_OK);
    /* An unrelated write must not prevent immediate preference changes. */
    storage.busy = true;
    assert(onboarding_service_request_timezone_update(3) == ESP_OK);
    onboarding_service_refresh();
    expect(3, true);
    assert(writes == 0);
    storage.busy = false;
    /* Simulate a competing enqueue after the free-storage snapshot. */
    enqueue_result = ESP_ERR_INVALID_STATE;
    onboarding_service_refresh();
    onboarding_service_refresh();
    expect(3, true);
    enqueue_result = ESP_OK;
    onboarding_service_refresh();
    expect(3, true);
    /* A newer choice while a write is in flight must survive its completion. */
    assert(onboarding_service_request_timezone_update(4) == ESP_OK);
    finish_write();
    expect(4, true);
    assert(queued.timezone_index == 4);
    finish_write();
    expect(4, false);
    onboarding_service_refresh();
    expect(4, false);
    /* A hard persistence failure must not silently restore the old timezone. */
    enqueue_result = ESP_FAIL;
    assert(onboarding_service_request_timezone_update(2) == ESP_OK);
    onboarding_service_refresh();
    onboarding_service_refresh();
    expect(2, false);
    assert(s_status.last_result == ESP_FAIL);
    enqueue_result = ESP_OK;
    assert(onboarding_service_request_timezone_update(2) == ESP_OK);
    onboarding_service_refresh();
    finish_write();
    expect(2, false);
    assert(onboarding_service_request_timezone_update(TIMEZONE_CATALOG_COUNT) == ESP_ERR_INVALID_ARG);
    /* Simulated process restart: hydrate the existing stored profile. This
     * verifies the service contract, not physical NVS/power-loss behavior. */
    s_started = false;
    s_editing = false;
    s_timezone_update_pending = false;
    s_timezone_write_enqueued = false;
    s_timezone_selected = false;
    s_status = (onboarding_service_status_t){0};
    assert(onboarding_service_start() == ESP_OK);
    expect(2, false);
    /* Simulated process restart: hydrate the existing stored profile. This
     * verifies the service contract, not physical NVS/power-loss behavior. */
    s_started = false;
    s_editing = false;
    s_timezone_update_pending = false;
    s_timezone_write_enqueued = false;
    s_timezone_selected = false;
    s_status = (onboarding_service_status_t){0};
    assert(onboarding_service_start() == ESP_OK);
    expect(2, false);
    puts("Onboarding timezone regression: PASS");
    return 0;
}
