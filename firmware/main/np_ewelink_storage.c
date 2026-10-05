#include "np_ewelink_storage.h"

#include <string.h>

#include "flash_coordinator.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EWELINK_STORAGE_WAIT_MS 20000U
#define EWELINK_STORAGE_POLL_MS 10U

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) *bytes++ = 0U;
}

esp_err_t np_ewelink_storage_load(np_ewelink_inventory_t *out_inventory)
{
    if (out_inventory == NULL) return ESP_ERR_INVALID_ARG;
    memset(out_inventory, 0, sizeof(*out_inventory));
    return flash_coordinator_copy_ewelink_inventory(out_inventory);
}

esp_err_t np_ewelink_storage_save(const np_ewelink_inventory_t *inventory)
{
    if (inventory == NULL) return ESP_ERR_INVALID_ARG;
    uint32_t sequence = 0U;
    esp_err_t result = flash_coordinator_request_ewelink_inventory_write(inventory,
                                                                          &sequence);
    if (result != ESP_OK) return result;
    for (uint32_t elapsed = 0U; elapsed < EWELINK_STORAGE_WAIT_MS;
         elapsed += EWELINK_STORAGE_POLL_MS) {
        bool completed = false;
        esp_err_t write_result = ESP_ERR_INVALID_STATE;
        result = flash_coordinator_get_ewelink_inventory_write_result(
            sequence, &completed, &write_result);
        if (result != ESP_OK) return result;
        if (completed) {
            result = write_result;
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(EWELINK_STORAGE_POLL_MS));
    }
    secure_zero(&sequence, sizeof(sequence));
    return ESP_ERR_TIMEOUT;
}
