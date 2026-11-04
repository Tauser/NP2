#include "camera_inventory.h"

#include <stddef.h>
#include <string.h>

#include "cache_record.h"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "nvs.h"

#define CAMERA_NVS_NAMESPACE "np2_camera"

typedef struct {
    cache_record_header_t header;
    camera_inventory_t inventory;
} inventory_record_t;

static camera_inventory_t s_saved;
static bool s_load_attempted;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool printable(const char *text, size_t capacity, bool allow_empty)
{
    const size_t length = strnlen(text, capacity);
    if (length == capacity || (!allow_empty && length == 0U)) return false;
    for (size_t i = 0U; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (c < 0x20U || c == 0x7FU) return false;
    }
    return true;
}

static bool private_ipv4(const char *address)
{
    struct in_addr ip = {0};
    if (inet_aton(address, &ip) == 0) return false;
    const uint32_t host = ntohl(ip.s_addr);
    return (host >> 24U) == 10U || (host >> 20U) == 0xAC1U || (host >> 16U) == 0xC0A8U;
}

static bool valid_inventory(const camera_inventory_t *inventory)
{
    if (inventory == NULL || inventory->count > CAMERA_INVENTORY_MAX) return false;
    for (uint8_t i = 0U; i < inventory->count; ++i) {
        if (!printable(inventory->cameras[i].address, sizeof(inventory->cameras[i].address), false) ||
            !private_ipv4(inventory->cameras[i].address) ||
            !printable(inventory->cameras[i].model, sizeof(inventory->cameras[i].model), true)) {
            return false;
        }
    }
    return true;
}

static bool record_is_valid(const inventory_record_t *record, size_t size)
{
    return size == sizeof(*record) &&
           cache_record_header_is_valid(&record->header, sizeof(record->inventory)) &&
           record->header.payload_size == sizeof(record->inventory) &&
           record->header.payload_crc32 == cache_record_crc32((const uint8_t *)&record->inventory,
                                                              sizeof(record->inventory)) &&
           valid_inventory(&record->inventory);
}

static void publish(const camera_inventory_t *inventory)
{
    portENTER_CRITICAL(&s_lock);
    s_saved = *inventory;
    portEXIT_CRITICAL(&s_lock);
}

bool camera_inventory_load_attempted(void)
{
    portENTER_CRITICAL(&s_lock);
    const bool attempted = s_load_attempted;
    portEXIT_CRITICAL(&s_lock);
    return attempted;
}

void camera_inventory_get(camera_inventory_t *out)
{
    if (out == NULL) return;
    portENTER_CRITICAL(&s_lock);
    *out = s_saved;
    portEXIT_CRITICAL(&s_lock);
}

esp_err_t camera_inventory_owner_load(void)
{
    esp_err_t outcome = ESP_ERR_NOT_FOUND;
    nvs_handle_t handle;
    esp_err_t result = nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result == ESP_OK) {
        inventory_record_t record = {0};
        uint32_t selected = 0U;
        for (unsigned slot = 0U; slot < 2U; ++slot) {
            size_t size = sizeof(record);
            result = nvs_get_blob(handle, slot == 0U ? "inv0" : "inv1", &record, &size);
            if (result == ESP_ERR_NVS_NOT_FOUND) continue;
            if (result != ESP_OK || !record_is_valid(&record, size)) {
                if (outcome != ESP_OK) outcome = ESP_ERR_INVALID_CRC;
                continue;
            }
            if (record.header.generation > selected) {
                selected = record.header.generation;
                publish(&record.inventory);
                outcome = ESP_OK;
            }
        }
        nvs_close(handle);
    } else if (result != ESP_ERR_NVS_NOT_FOUND) {
        outcome = result;
    }
    portENTER_CRITICAL(&s_lock);
    s_load_attempted = true;
    portEXIT_CRITICAL(&s_lock);
    return outcome;
}

esp_err_t camera_inventory_owner_write(const camera_inventory_t *inventory)
{
    if (!valid_inventory(inventory)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t result = nvs_open(CAMERA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    inventory_record_t record = {0};
    uint32_t generation = 0U;
    for (unsigned slot = 0U; slot < 2U; ++slot) {
        size_t size = sizeof(record);
        if (nvs_get_blob(handle, slot == 0U ? "inv0" : "inv1", &record, &size) == ESP_OK &&
            record_is_valid(&record, size) && record.header.generation > generation) {
            generation = record.header.generation;
        }
    }
    if (generation == UINT32_MAX) {
        nvs_close(handle);
        return ESP_ERR_INVALID_STATE;
    }
    /* The older slot is replaced; the newest valid one survives a power cut. */
    memset(&record, 0, sizeof(record));
    record.inventory = *inventory;
    record.header = (cache_record_header_t){
        .magic = NP2_CACHE_RECORD_MAGIC,
        .schema_version = 1U,
        .header_size = sizeof(record.header),
        .generation = generation + 1U,
        .payload_size = sizeof(record.inventory),
    };
    record.header.payload_crc32 =
        cache_record_crc32((const uint8_t *)&record.inventory, sizeof(record.inventory));
    record.header.header_crc32 =
        cache_record_crc32((const uint8_t *)&record.header, offsetof(cache_record_header_t, header_crc32));
    const char *const key = (record.header.generation & 1U) != 0U ? "inv1" : "inv0";
    result = nvs_set_blob(handle, key, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    if (result == ESP_OK) {
        size_t size = sizeof(record);
        memset(&record, 0, sizeof(record));
        result = nvs_get_blob(handle, key, &record, &size);
        if (result == ESP_OK && (!record_is_valid(&record, size) ||
                                 memcmp(&record.inventory, inventory, sizeof(*inventory)) != 0)) {
            result = ESP_FAIL;
        }
    }
    nvs_close(handle);
    if (result == ESP_OK) publish(inventory);
    return result;
}
