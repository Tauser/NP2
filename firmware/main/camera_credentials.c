#include "camera_credentials.h"

#include <stddef.h>
#include <string.h>

#include "cache_record.h"
#include "esp_flash_encrypt.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"

#define CAMERA_NVS_NAMESPACE "np2_camera"
/* The shared record header only accepts NP2_CACHE_RECORD_SCHEMA_VERSION. The move from one
 * account to a table is told apart by the payload size, which the validation also checks. */

typedef struct {
    uint8_t count;
    camera_credentials_t entries[CAMERA_CREDENTIALS_MAX];
} credential_table_t;

typedef struct {
    cache_record_header_t header;
    credential_table_t table;
} credential_record_t;

static credential_table_t s_table;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static void scrub(void *data, size_t length)
{
    volatile uint8_t *p = data;
    while (length-- > 0U) *p++ = 0U;
}

static bool valid_text(const char *text, size_t capacity, bool allow_empty)
{
    const size_t length = strnlen(text, capacity);
    if (length >= capacity || (!allow_empty && length == 0U)) return false;
    for (size_t i = 0U; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (c < 0x20U || c == 0x7FU) return false;
    }
    return true;
}

static bool valid_credentials(const camera_credentials_t *c)
{
    /* Empty username and password mean a camera that needs no account. */
    return c != NULL && valid_text(c->ipv4, sizeof(c->ipv4), false) &&
           valid_text(c->username, sizeof(c->username), true) &&
           valid_text(c->password, sizeof(c->password), true);
}

static bool valid_table(const credential_table_t *table)
{
    if (table->count > CAMERA_CREDENTIALS_MAX) return false;
    for (uint8_t i = 0U; i < table->count; ++i) {
        if (!valid_credentials(&table->entries[i])) return false;
    }
    return true;
}

/* Same retention policy as the Wi-Fi and Spotify accounts. */
static bool vault_allowed(void)
{
#if defined(NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION) && NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION
    return true;
#elif defined(CONFIG_NVS_ENCRYPTION)
    return esp_flash_encryption_enabled();
#else
    return false;
#endif
}

static bool record_is_valid(const credential_record_t *record, size_t size)
{
    return size == sizeof(*record) &&
           cache_record_header_is_valid(&record->header, sizeof(record->table)) &&
           record->header.payload_size == sizeof(record->table) &&
           record->header.payload_crc32 == cache_record_crc32((const uint8_t *)&record->table,
                                                              sizeof(record->table)) &&
           valid_table(&record->table);
}

static void publish(const credential_table_t *table)
{
    portENTER_CRITICAL(&s_lock);
    s_table = *table;
    portEXIT_CRITICAL(&s_lock);
}

static const camera_credentials_t *find_locked(const credential_table_t *table, const char *ipv4)
{
    for (uint8_t i = 0U; i < table->count; ++i) {
        if (strncmp(table->entries[i].ipv4, ipv4, sizeof(table->entries[i].ipv4)) == 0)
            return &table->entries[i];
    }
    return NULL;
}

bool camera_credentials_saved_for(const char *ipv4)
{
    if (ipv4 == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    const bool saved = find_locked(&s_table, ipv4) != NULL;
    portEXIT_CRITICAL(&s_lock);
    return saved;
}

esp_err_t camera_credentials_copy_private(const char *ipv4, camera_credentials_t *out)
{
    if (ipv4 == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    const camera_credentials_t *const found = find_locked(&s_table, ipv4);
    if (found != NULL) *out = *found;
    portEXIT_CRITICAL(&s_lock);
    return found != NULL ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t camera_credentials_owner_load(void)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    nvs_handle_t handle;
    esp_err_t result = nvs_open(CAMERA_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result != ESP_OK) return result == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : result;
    credential_record_t record = {0};
    uint32_t selected = 0U;
    esp_err_t outcome = ESP_ERR_NOT_FOUND;
    for (unsigned slot = 0U; slot < 2U; ++slot) {
        size_t size = sizeof(record);
        result = nvs_get_blob(handle, slot == 0U ? "auth0" : "auth1", &record, &size);
        if (result == ESP_ERR_NVS_NOT_FOUND) continue;
        if (result != ESP_OK || !record_is_valid(&record, size)) {
            if (outcome != ESP_OK) outcome = ESP_ERR_INVALID_CRC;
            continue;
        }
        if (record.header.generation > selected) {
            selected = record.header.generation;
            publish(&record.table);
            outcome = ESP_OK;
        }
    }
    nvs_close(handle);
    scrub(&record, sizeof(record));
    return outcome;
}

/* Adds or replaces the account of one camera; the others are kept. */
esp_err_t camera_credentials_owner_write(const camera_credentials_t *credentials)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    if (!valid_credentials(credentials)) return ESP_ERR_INVALID_ARG;

    credential_table_t next;
    portENTER_CRITICAL(&s_lock);
    next = s_table;
    portEXIT_CRITICAL(&s_lock);
    uint8_t slot = next.count;
    for (uint8_t i = 0U; i < next.count; ++i) {
        if (strncmp(next.entries[i].ipv4, credentials->ipv4, sizeof(next.entries[i].ipv4)) == 0) {
            slot = i;
            break;
        }
    }
    if (slot >= CAMERA_CREDENTIALS_MAX) {
        scrub(&next, sizeof(next));
        return ESP_ERR_NO_MEM;
    }
    next.entries[slot] = *credentials;
    if (slot == next.count) ++next.count;

    nvs_handle_t handle;
    esp_err_t result = nvs_open(CAMERA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        scrub(&next, sizeof(next));
        return result;
    }
    credential_record_t record = {0};
    uint32_t generation = 0U;
    for (unsigned slot_index = 0U; slot_index < 2U; ++slot_index) {
        size_t size = sizeof(record);
        if (nvs_get_blob(handle, slot_index == 0U ? "auth0" : "auth1", &record, &size) == ESP_OK &&
            record_is_valid(&record, size) && record.header.generation > generation) {
            generation = record.header.generation;
        }
    }
    if (generation == UINT32_MAX) {
        nvs_close(handle);
        scrub(&record, sizeof(record));
        scrub(&next, sizeof(next));
        return ESP_ERR_INVALID_STATE;
    }
    /* The older generation is replaced; the newest valid one survives a power cut. */
    memset(&record, 0, sizeof(record));
    record.table = next;
    record.header = (cache_record_header_t){
        .magic = NP2_CACHE_RECORD_MAGIC,
        .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
        .header_size = sizeof(record.header),
        .generation = generation + 1U,
        .payload_size = sizeof(record.table),
    };
    record.header.payload_crc32 = cache_record_crc32((const uint8_t *)&record.table, sizeof(record.table));
    record.header.header_crc32 =
        cache_record_crc32((const uint8_t *)&record.header, offsetof(cache_record_header_t, header_crc32));
    const char *const key = (record.header.generation & 1U) != 0U ? "auth1" : "auth0";
    result = nvs_set_blob(handle, key, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    if (result == ESP_OK) {
        size_t size = sizeof(record);
        memset(&record, 0, sizeof(record));
        result = nvs_get_blob(handle, key, &record, &size);
        if (result == ESP_OK && (!record_is_valid(&record, size) ||
                                 memcmp(&record.table, &next, sizeof(next)) != 0)) {
            result = ESP_FAIL;
        }
    }
    nvs_close(handle);
    scrub(&record, sizeof(record));
    if (result == ESP_OK) publish(&next);
    scrub(&next, sizeof(next));
    return result;
}
