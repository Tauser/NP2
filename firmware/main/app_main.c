#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_log.h"

#include "app_state.h"
#include "board_bringup.h"
#include "camera_stream_service.h"
#include "connectivity_diagnostic.h"
#include "device_control_service.h"
#include "flash_coordinator.h"
#include "network_validation_service.h"
#include "notification_service.h"
#include "onvif_discovery_service.h"
#include "onboarding_service.h"
#include "provisioning_service.h"
#include "sonoff_lan_service.h"
#include "time_service.h"
#include "update_boot_supervisor.h"
#include "weather_asset_service.h"

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

    /* Start recovery before display initialization, which can fail or stall. */
    const esp_err_t flash_coordinator_err = flash_coordinator_start();
    if (flash_coordinator_err != ESP_OK) {
        ESP_LOGE(TAG, "Flash coordinator unavailable: %s", esp_err_to_name(flash_coordinator_err));
    }
    const esp_err_t update_boot_err = update_boot_supervisor_start();
    if (update_boot_err != ESP_OK) {
        ESP_LOGE(TAG, "P4 OTA boot supervisor unavailable: %s", esp_err_to_name(update_boot_err));
    }

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

    /* The microSD owns slot 0 and needs a clean power window after a warm
     * reset. Start it before ESP-Hosted claims slot 1 for the C6. */
    const esp_err_t weather_assets_err = weather_asset_service_start();
    if (weather_assets_err != ESP_OK) {
        ESP_LOGE(TAG, "Weather-asset SD service unavailable: %s",
                 esp_err_to_name(weather_assets_err));
    }

    const esp_err_t device_controls_err = device_control_service_start();
    if (device_controls_err != ESP_OK) {
        ESP_LOGE(TAG, "Device controls unavailable: %s",
                 esp_err_to_name(device_controls_err));
    }

    const esp_err_t connectivity_err = connectivity_diagnostic_start();
    if (connectivity_err != ESP_OK) {
        ESP_LOGE(TAG, "Phase 3 connectivity probe unavailable: %s",
                 esp_err_to_name(connectivity_err));
    }

    const esp_err_t sonoff_lan_err = sonoff_lan_service_start();
    if (sonoff_lan_err != ESP_OK) {
        ESP_LOGE(TAG, "Sonoff LAN discovery unavailable: %s",
                 esp_err_to_name(sonoff_lan_err));
    }

    const esp_err_t onvif_discovery_err = onvif_discovery_service_start();
    if (onvif_discovery_err != ESP_OK) {
        ESP_LOGE(TAG, "ONVIF camera discovery unavailable: %s",
                 esp_err_to_name(onvif_discovery_err));
    }

    const esp_err_t camera_stream_err = camera_stream_service_start();
    if (camera_stream_err != ESP_OK) {
        ESP_LOGE(TAG, "Camera stream service unavailable: %s",
                 esp_err_to_name(camera_stream_err));
    }

    const esp_err_t onboarding_err = onboarding_service_start();
    if (onboarding_err != ESP_OK) {
        ESP_LOGE(TAG, "Onboarding service unavailable: %s", esp_err_to_name(onboarding_err));
    }

    const esp_err_t provisioning_err = provisioning_service_start();
    if (provisioning_err != ESP_OK) {
        ESP_LOGE(TAG, "Open-network maintenance service unavailable: %s",
                 esp_err_to_name(provisioning_err));
    }

    const esp_err_t time_service_err = time_service_start();
    if (time_service_err != ESP_OK) {
        ESP_LOGE(TAG, "Time service unavailable: %s", esp_err_to_name(time_service_err));
    }

    const esp_err_t notifications_err = notification_service_start();
    if (notifications_err != ESP_OK) {
        ESP_LOGE(TAG, "Notification service unavailable: %s",
                 esp_err_to_name(notifications_err));
    }

    const esp_err_t app_state_err = app_state_start();
    if (app_state_err != ESP_OK) {
        ESP_LOGE(TAG, "Phase 4 app state unavailable: %s", esp_err_to_name(app_state_err));
    }

    /* Start the EventBus consumer before the producer. Product-data updates
     * are then always delivered through app_loop, never directly to LVGL. */
    const esp_err_t validation_err = network_validation_service_start();
    if (validation_err != ESP_OK) {
        ESP_LOGE(TAG, "Network validation service unavailable: %s",
                 esp_err_to_name(validation_err));
    }

    ESP_LOGI(TAG, "P4 local bring-up ready; Phase 4 state projection runs asynchronously");
}
