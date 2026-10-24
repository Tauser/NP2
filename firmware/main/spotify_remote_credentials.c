#include "spotify_remote.h"
#include "cache_record.h"
#include "flash_coordinator.h"

#include <stddef.h>
#include <string.h>
#include "google_agenda_codec.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "esp_flash_encrypt.h"
#include "freertos/FreeRTOS.h"

typedef struct { cache_record_header_t header; spotify_remote_credentials_t credentials; } credential_record_t;
static spotify_remote_credentials_t s_credentials;
static bool s_linked;
static uint32_t s_version;
static bool s_rotation_dirty, s_rotation_inflight;
static uint32_t s_rotation_completion, s_rotation_version;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static void scrub(void *data, size_t length)
{
    volatile uint8_t *p = data;
    while (length-- > 0U) *p++ = 0U;
}

static bool valid_text(const char *s, size_t n)
{
    const size_t length = strnlen(s, n);
    if (length == 0U || length >= n) return false;
    for (size_t i = 0U; i < length; ++i)
        if ((unsigned char)s[i] < 33U || (unsigned char)s[i] > 126U) return false;
    return true;
}

static bool valid_credentials(const spotify_remote_credentials_t *c)
{
    return c != NULL && valid_text(c->client_id, sizeof(c->client_id)) &&
        valid_text(c->refresh_token, sizeof(c->refresh_token)) &&
        strlen(c->client_id) == 32U && strspn(c->client_id, "0123456789abcdefABCDEF") == 32U;
}

bool spotify_remote_linked(void)
{
    portENTER_CRITICAL(&s_lock);
    bool linked = s_linked;
    portEXIT_CRITICAL(&s_lock);
    return linked;
}

/* Private consumer function deliberately absent from UI headers. */
esp_err_t spotify_remote_credentials_copy_private(spotify_remote_credentials_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    bool linked = s_linked;
    if (linked) *out = s_credentials;
    portEXIT_CRITICAL(&s_lock);
    return linked ? ESP_OK : ESP_ERR_NOT_FOUND;
}

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

esp_err_t spotify_remote_credentials_owner_load(void)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    /* Two small vault records; no token appears in status or logs. */
    credential_record_t record = {0};
    uint32_t selected = 0U;
    esp_err_t outcome = ESP_ERR_NOT_FOUND;
    nvs_handle_t handle;
    esp_err_t result = nvs_open("np2_spotify", NVS_READONLY, &handle);
    if (result != ESP_OK) return result == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : result;
    for (unsigned slot = 0; slot < 2U; ++slot) {
        size_t size = sizeof(record);
        result = nvs_get_blob(handle, slot == 0U ? "auth0" : "auth1", &record, &size);
        if (result == ESP_ERR_NVS_NOT_FOUND) continue;
        if (result != ESP_OK || size != sizeof(record) ||
            !cache_record_header_is_valid(&record.header, sizeof(record.credentials)) ||
            record.header.payload_size != sizeof(record.credentials) ||
            record.header.payload_crc32 != cache_record_crc32((const uint8_t *)&record.credentials,
                sizeof(record.credentials)) || !valid_credentials(&record.credentials)) {
            if (outcome != ESP_OK) outcome = ESP_ERR_INVALID_CRC;
            continue;
        }
        if (record.header.generation > selected) {
            selected = record.header.generation;
            portENTER_CRITICAL(&s_lock);
            if (memcmp(&s_credentials, &record.credentials, sizeof(s_credentials)) != 0) ++s_version;
            s_credentials = record.credentials;
            s_linked = true;
            portEXIT_CRITICAL(&s_lock);
            outcome = ESP_OK;
        }
    }
    nvs_close(handle);
    scrub(&record, sizeof(record));
    return outcome;
}

esp_err_t spotify_remote_credentials_owner_write(const spotify_remote_credentials_t *credentials)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    if (!valid_credentials(credentials)) return ESP_ERR_INVALID_ARG;
    /* Provisioning writes both generations successively; the previous valid
     * slot is kept until the new record has committed and verified. */
    nvs_handle_t handle;
    esp_err_t result = nvs_open("np2_spotify", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    credential_record_t record = {0};
    uint32_t generation = 0U;
    for (unsigned slot = 0U; slot < 2U; ++slot) {
        size_t size = sizeof(record);
        if (nvs_get_blob(handle, slot == 0U ? "auth0" : "auth1", &record, &size) == ESP_OK &&
            size == sizeof(record) && cache_record_header_is_valid(&record.header, sizeof(record.credentials)) &&
            record.header.payload_crc32 == cache_record_crc32((const uint8_t *)&record.credentials, sizeof(record.credentials)) &&
            valid_credentials(&record.credentials) && record.header.generation > generation)
            generation = record.header.generation;
    }
    if (generation == UINT32_MAX) { nvs_close(handle); scrub(&record, sizeof(record)); return ESP_ERR_INVALID_STATE; }
    memset(&record, 0, sizeof(record));
    record.credentials = *credentials;
    record.header = (cache_record_header_t){.magic = NP2_CACHE_RECORD_MAGIC,
        .schema_version = 1U, .header_size = sizeof(record.header),
        .generation = generation + 1U, .payload_size = sizeof(record.credentials)};
    record.header.payload_crc32 = cache_record_crc32((const uint8_t *)&record.credentials, sizeof(record.credentials));
    record.header.header_crc32 = cache_record_crc32((const uint8_t *)&record.header,
        offsetof(cache_record_header_t, header_crc32));
    const char *key = (record.header.generation & 1U) ? "auth1" : "auth0";
    result = nvs_set_blob(handle, key, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    if (result == ESP_OK) {
        size_t size = sizeof(record);
        result = nvs_get_blob(handle, key, &record, &size);
        if (result == ESP_OK && (size != sizeof(record) ||
            memcmp(&record.credentials, credentials, sizeof(*credentials)) != 0 ||
            !cache_record_header_is_valid(&record.header, sizeof(record.credentials)))) result = ESP_FAIL;
    }
    nvs_close(handle);
    scrub(&record, sizeof(record));
    if (result == ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        if (memcmp(&s_credentials, credentials, sizeof(s_credentials)) != 0) ++s_version;
        s_credentials = *credentials; s_linked = true;
        portEXIT_CRITICAL(&s_lock);
    }
    return result;
}

/* Rotation stays in the private RAM vault and retries coordinated persistence.
 * A reset before commit may require reauthorization; never log a token. */
void spotify_remote_credentials_tick(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (s_rotation_inflight && status.spotify_auth_completion != s_rotation_completion) {
        s_rotation_inflight = false;
        if (status.spotify_auth_result == ESP_OK) {
            portENTER_CRITICAL(&s_lock);
            if (s_version == s_rotation_version) s_rotation_dirty = false;
            portEXIT_CRITICAL(&s_lock);
        }
    }
    portENTER_CRITICAL(&s_lock);
    bool dirty = s_rotation_dirty;
    portEXIT_CRITICAL(&s_lock);
    if (!dirty || s_rotation_inflight) return;
    spotify_remote_credentials_t copy = {0};
    if (spotify_remote_credentials_copy_private(&copy) == ESP_OK &&
        flash_coordinator_request_spotify_credentials_write(&copy) == ESP_OK) {
        s_rotation_completion = status.spotify_auth_completion;
        portENTER_CRITICAL(&s_lock); s_rotation_version = s_version; portEXIT_CRITICAL(&s_lock);
        s_rotation_inflight = true;
    }
    scrub(&copy, sizeof(copy));
}
esp_err_t spotify_remote_credentials_rotate_private(const spotify_remote_credentials_t *c)
{
    if (!valid_credentials(c)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    s_credentials = *c;
    ++s_version;
    s_linked = true;
    s_rotation_dirty = true;
    portEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}
esp_err_t spotify_remote_provision_json(const char *json, size_t length)
{
    if (json == NULL || length == 0U || length > 768U || memchr(json, 0, length)) return ESP_ERR_INVALID_SIZE;
    /* Strict bounded flat JSON; duplicate/unknown members are rejected. */
    google_json_t *doc = heap_caps_calloc(1U, sizeof(*doc), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (doc == NULL) return ESP_ERR_NO_MEM;
    spotify_remote_credentials_t credentials = {0};
    esp_err_t result = ESP_ERR_INVALID_ARG;
    if (google_json_parse(doc, json, length) && doc->count == 5U &&
        google_json_string(doc, google_json_field(doc, 0, "client_id"), credentials.client_id, sizeof(credentials.client_id)) &&
        google_json_string(doc, google_json_field(doc, 0, "refresh_token"), credentials.refresh_token, sizeof(credentials.refresh_token)) &&
        valid_credentials(&credentials)) {
        if (!spotify_remote_provision_begin()) result = ESP_ERR_INVALID_STATE;
        else {
            result = flash_coordinator_request_spotify_credentials_write(&credentials);
            if (result != ESP_OK) spotify_remote_provision_end();
        }
    }
    scrub(&credentials, sizeof(credentials));
    scrub(doc, sizeof(*doc));
    heap_caps_free(doc);
    return result;
}

uint32_t spotify_remote_credentials_version(void)
{ portENTER_CRITICAL(&s_lock); uint32_t v = s_version; portEXIT_CRITICAL(&s_lock); return v; }
bool spotify_remote_credentials_settled(void)
{ portENTER_CRITICAL(&s_lock); bool ready = !s_rotation_dirty; portEXIT_CRITICAL(&s_lock); return ready; }
