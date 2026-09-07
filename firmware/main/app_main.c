#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board_bringup.h"
#include "flash_coordinator.h"

static const char *const TAG = "np2_boot";

void app_main(void)
{
    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);

    ESP_LOGI(TAG, "NP2 firmware scaffold started");
    ESP_LOGI(TAG, "target=%s, revision=%d, cores=%d",
             CONFIG_IDF_TARGET, chip.revision, chip.cores);
    ESP_LOGI(TAG, "app=%s, version=%s", esp_app_get_description()->project_name,
             esp_app_get_description()->version);

    const esp_err_t bringup_err = board_bringup_start();
    if (bringup_err != ESP_OK) {
        /*
         * Do not retry panel initialization in a tight loop. The physical gate
         * needs the first failure log, and recovery policy will be added only
         * after this baseline is proven on the target board.
         */
        ESP_LOGE(TAG, "P4 local bring-up stopped: %s", esp_err_to_name(bringup_err));
        return;
    }

    const esp_err_t flash_coordinator_err = flash_coordinator_start();
    if (flash_coordinator_err != ESP_OK) {
        ESP_LOGE(TAG, "Flash coordinator unavailable: %s", esp_err_to_name(flash_coordinator_err));
    }

    ESP_LOGI(TAG, "P4 local bring-up ready; network services are not configured yet");
}
