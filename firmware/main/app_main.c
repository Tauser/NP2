#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_log.h"

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

    /* Hardware initialization begins only after the reproducibility gate closes. */
}
