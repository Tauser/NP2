#include "update_development.h"

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "flash_coordinator.h"
#include "network_validation_service.h"

#ifdef NP2_OTA_DEVELOPMENT_CONFIGURED
#include "np2_ota_development_config.h"
#endif

static const char *const TAG = "update_development";

void update_development_log_status(void)
{
    const esp_partition_t *const running = esp_ota_get_running_partition();
    const esp_partition_t *const next = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *const slots[] = {running, next};
    for (unsigned i = 0U; i < 2U; ++i) {
        if (slots[i] == NULL) continue;
        esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
        const esp_err_t result = esp_ota_get_state_partition(slots[i], &state);
        ESP_LOGI(TAG, "slot=%s running=%u address=0x%lx size=%lu state=%u lookup=%s",
                 slots[i]->label, i == 0U, (unsigned long)slots[i]->address,
                 (unsigned long)slots[i]->size, (unsigned)state, esp_err_to_name(result));
    }
    flash_coordinator_status_t flash = {0};
    flash_coordinator_get_status(&flash);
    ESP_LOGI(TAG, "journal valid=%u state=%u generation=%lu result=%s",
             flash.update_journal_valid, flash.update_journal_state,
             (unsigned long)flash.update_journal_generation,
             esp_err_to_name(flash.update_journal_result));
#ifdef NP2_OTA_DEVELOPMENT_CONFIGURED
    ESP_LOGW(TAG, "laboratory keyring configured; explicit maintenance command required");
#else
    ESP_LOGI(TAG, "laboratory OTA disabled: no build-time endpoints/keyring");
#endif
}

esp_err_t update_development_request(bool apply)
{
#ifndef NP2_OTA_DEVELOPMENT_CONFIGURED
    (void)apply;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!apply) {
        return network_validation_service_request_p4_update_preflight(&s_development_endpoints);
    }
    const esp_partition_t *const running = esp_ota_get_running_partition();
    const esp_partition_t *const next = esp_ota_get_next_update_partition(NULL);
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == NULL || next == NULL || next == running ||
        esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_VALID) {
        ESP_LOGW(TAG, "OTA refused: current slot must be a verified VALID fallback");
        return ESP_ERR_INVALID_STATE;
    }
    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);
    const network_p4_update_request_t request = {
        .endpoints = s_development_endpoints,
        .keyring_entries = s_development_keys,
        .keyring_entries_count = 1U,
        .environment = {
            .product_id = NP2_DEVELOPMENT_PRODUCT_ID,
            .board_id = NP2_DEVELOPMENT_BOARD_ID,
            .revision = chip.revision,
            .inactive_slot_bytes = next->size,
            .security_version = esp_app_get_description()->secure_version,
            .persisted_schema = 1U,
            /* The worker obtains the live C6 version before admission. */
            .current_app_confirmed = true,
        },
    };
    return network_validation_service_request_p4_update_apply(&request);
#endif
}
