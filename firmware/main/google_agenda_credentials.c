#include "google_agenda_sync.h"
#include "cache_record.h"
#include "flash_coordinator.h"

#include <stddef.h>
#include <string.h>
#include "cJSON.h"
#include "nvs.h"
#include "esp_flash_encrypt.h"
#include "freertos/FreeRTOS.h"

typedef struct { cache_record_header_t header; google_agenda_credentials_t credentials; } credential_record_t;
static google_agenda_credentials_t s_credentials;
static bool s_linked;
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

static bool valid_credentials(const google_agenda_credentials_t *c)
{
    return c != NULL && valid_text(c->client_id, sizeof(c->client_id)) &&
        valid_text(c->client_secret, sizeof(c->client_secret)) &&
        valid_text(c->refresh_token, sizeof(c->refresh_token)) &&
        valid_text(c->subject, sizeof(c->subject)) &&
        strstr(c->client_id, ".apps.googleusercontent.com") != NULL;
}

bool google_agenda_linked(void)
{
    portENTER_CRITICAL(&s_lock);
    bool linked = s_linked;
    portEXIT_CRITICAL(&s_lock);
    return linked;
}

/* Private consumer function deliberately absent from UI headers. */
esp_err_t google_agenda_credentials_copy_private(google_agenda_credentials_t *out)
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

esp_err_t google_agenda_credentials_owner_load(void)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    /* Two small vault records; no token appears in status or logs. */
    credential_record_t record = {0};
    uint32_t selected = 0U;
    esp_err_t outcome = ESP_ERR_NOT_FOUND;
    nvs_handle_t handle;
    esp_err_t result = nvs_open("np2_google", NVS_READONLY, &handle);
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

esp_err_t google_agenda_credentials_owner_write(const google_agenda_credentials_t *credentials)
{
    if (!vault_allowed()) return ESP_ERR_NOT_SUPPORTED;
    if (!valid_credentials(credentials)) return ESP_ERR_INVALID_ARG;
    /* Reauthorization may renew the same account. Never reuse persisted
     * calendar/task identifiers against another Google account. */
    portENTER_CRITICAL(&s_lock);
    const bool wrong_account = s_linked && strcmp(s_credentials.subject, credentials->subject) != 0;
    portEXIT_CRITICAL(&s_lock);
    if (wrong_account) return ESP_ERR_INVALID_STATE;
    /* Provisioning writes both generations successively; the previous valid
     * slot is kept until the new record has committed and verified. */
    nvs_handle_t handle;
    esp_err_t result = nvs_open("np2_google", NVS_READWRITE, &handle);
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
    if (result == ESP_OK) result = google_agenda_credentials_owner_load();
    return result;
}

esp_err_t google_agenda_provision_json(const char *json, size_t length)
{
    if (json == NULL || length == 0U || length > 1536U) return ESP_ERR_INVALID_SIZE;
    unsigned depth = 0U;
    bool quoted = false, escaped = false;
    for (size_t i = 0U; i < length; ++i) {
        char c = json[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 2U) return ESP_ERR_INVALID_ARG; }
        else if (c == '}' || c == ']') { if (depth == 0U) return ESP_ERR_INVALID_ARG; --depth; }
    }
    /* This JSON is a local bounded private handoff, not a provider payload. */
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(json, length, &end, false);
    google_agenda_credentials_t credentials = {0};
    esp_err_t result = ESP_ERR_INVALID_ARG;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "client_id");
    const cJSON *secret = cJSON_GetObjectItemCaseSensitive(root, "client_secret");
    const cJSON *token = cJSON_GetObjectItemCaseSensitive(root, "refresh_token");
    const cJSON *subject = cJSON_GetObjectItemCaseSensitive(root, "subject");
    if (cJSON_IsObject(root) && end == json + length && cJSON_GetArraySize(root) == 4 &&
        cJSON_IsString(id) && cJSON_IsString(secret) && cJSON_IsString(token) &&
        cJSON_IsString(subject) && strlen(subject->valuestring) < sizeof(credentials.subject) &&
        strlen(id->valuestring) < sizeof(credentials.client_id) &&
        strlen(secret->valuestring) < sizeof(credentials.client_secret) &&
        strlen(token->valuestring) < sizeof(credentials.refresh_token)) {
        strcpy(credentials.client_id, id->valuestring);
        strcpy(credentials.client_secret, secret->valuestring);
        strcpy(credentials.refresh_token, token->valuestring);
        strcpy(credentials.subject, subject->valuestring);
        if (valid_credentials(&credentials))
            result = flash_coordinator_request_google_credentials_write(&credentials);
    }
    if (cJSON_IsString(id)) scrub(id->valuestring, strlen(id->valuestring));
    if (cJSON_IsString(secret)) scrub(secret->valuestring, strlen(secret->valuestring));
    if (cJSON_IsString(token)) scrub(token->valuestring, strlen(token->valuestring));
    if (cJSON_IsString(subject)) scrub(subject->valuestring, strlen(subject->valuestring));
    cJSON_Delete(root);
    scrub(&credentials, sizeof(credentials));
    return result;
}
