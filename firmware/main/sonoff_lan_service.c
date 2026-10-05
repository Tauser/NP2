#include "sonoff_lan_service.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "connectivity_diagnostic.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "lwip/inet.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "mdns.h"
#include "np_ewelink.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#define SONOFF_LAN_TASK_STACK_BYTES (8U * 1024U)
#define SONOFF_LAN_TASK_PRIORITY 2U
#define SONOFF_LAN_SCAN_TIMEOUT_MS 3000U
#define SONOFF_LAN_POLL_MS 100U
#define SONOFF_LAN_SCAN_INTERVAL_MS 30000U
#define SONOFF_LAN_SCAN_CONFIRM_INTERVAL_MS 5000U
#define SONOFF_LAN_MISSES_BEFORE_OFFLINE 2U
#define SONOFF_LAN_HTTP_TIMEOUT_MS 2500
#define SONOFF_LAN_HTTP_BODY_BYTES 2048U
/* Four TXT data fields may each carry 255 bytes of Base64 state. */
#define SONOFF_LAN_STATE_JSON_BYTES 1024U
#define SONOFF_LAN_CIPHER_BYTES 768U
#define SONOFF_LAN_REQUEST_BYTES 1024U
#define SONOFF_LAN_ID_BYTES 11U
#define SONOFF_LAN_KEY_BYTES NP_EWELINK_DEVICE_KEY_BYTES

static const char *const TAG = "sonoff_lan";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static sonoff_lan_status_t s_status = {.last_result = ESP_ERR_INVALID_STATE};
static bool s_started;
static bool s_scan_requested;
static bool s_command_requested;
static bool s_mdns_initialized;
static uint64_t s_last_sequence_ms;
typedef struct {
    np_ewelink_public_inventory_t inventory;
    sonoff_lan_status_t next;
} sonoff_lan_scan_workspace_t;
static sonoff_lan_scan_workspace_t *s_scan_workspace;
static struct {
    char device_id[SONOFF_LAN_ID_BYTES];
    uint8_t channel;
    bool enabled;
} s_pending_command;
static char s_http_response[SONOFF_LAN_HTTP_BODY_BYTES + 1U];
static size_t s_http_response_size;
static bool s_http_response_overflow;

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) *bytes++ = 0U;
}

static void copy_text(char *out, size_t out_size, const char *value)
{
    if (out == NULL || out_size == 0U) return;
    out[0] = '\0';
    if (value == NULL) return;
    const size_t length = strnlen(value, out_size - 1U);
    memcpy(out, value, length);
    out[length] = '\0';
}

static const char *txt_value(const mdns_result_t *result, const char *key)
{
    if (result == NULL || key == NULL) return NULL;
    for (size_t i = 0U; i < result->txt_count; ++i) {
        if (result->txt[i].key != NULL &&
            strcmp(result->txt[i].key, key) == 0) return result->txt[i].value;
    }
    return NULL;
}

static bool parse_switch_state(const cJSON *root, bool known[3], bool enabled[3])
{
    if (!cJSON_IsObject(root) || known == NULL || enabled == NULL) return false;
    bool found = false;
    const cJSON *single = cJSON_GetObjectItemCaseSensitive(root, "switch");
    if (cJSON_IsString(single) && single->valuestring != NULL) {
        known[0] = true;
        enabled[0] = strcmp(single->valuestring, "on") == 0;
        found = true;
    }
    const cJSON *switches = cJSON_GetObjectItemCaseSensitive(root, "switches");
    if (cJSON_IsArray(switches)) {
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, switches) {
            const cJSON *outlet = cJSON_GetObjectItemCaseSensitive(item, "outlet");
            const cJSON *state = cJSON_GetObjectItemCaseSensitive(item, "switch");
            const int index = cJSON_IsNumber(outlet) ? outlet->valueint : 0;
            if (index < 0 || index >= 3 || !cJSON_IsString(state) ||
                state->valuestring == NULL) continue;
            known[index] = true;
            enabled[index] = strcmp(state->valuestring, "on") == 0;
            found = true;
        }
    }
    return found;
}

static bool decrypt_state_json(const char *data_b64, const char *iv_b64,
                               const char *device_key,
                               char *out_json, size_t out_size)
{
    if (data_b64 == NULL || iv_b64 == NULL || device_key == NULL ||
        out_json == NULL || out_size == 0U) return false;
    uint8_t cipher[SONOFF_LAN_CIPHER_BYTES] = {0};
    uint8_t iv[16] = {0};
    uint8_t key[16] = {0};
    size_t cipher_size = 0U;
    size_t iv_size = 0U;
    size_t key_length = strnlen(device_key, SONOFF_LAN_KEY_BYTES);
    bool valid = false;
    mbedtls_md5_context md5;
    mbedtls_md5_init(&md5);
    const bool crypto_ok =
        mbedtls_md5_starts(&md5) == 0 &&
        mbedtls_md5_update(&md5, (const unsigned char *)device_key,
                           key_length) == 0 &&
        mbedtls_md5_finish(&md5, key) == 0;
    mbedtls_md5_free(&md5);
    if (!crypto_ok ||
        mbedtls_base64_decode(cipher, sizeof(cipher), &cipher_size,
            (const unsigned char *)data_b64, strlen(data_b64)) != 0 ||
        mbedtls_base64_decode(iv, sizeof(iv), &iv_size,
            (const unsigned char *)iv_b64, strlen(iv_b64)) != 0 ||
        iv_size != sizeof(iv) || cipher_size == 0U ||
        cipher_size % 16U != 0U || cipher_size >= out_size) goto done;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const bool decrypt_ok = mbedtls_aes_setkey_dec(&aes, key, 128U) == 0 &&
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipher_size, iv,
                              cipher, (unsigned char *)out_json) == 0;
    mbedtls_aes_free(&aes);
    if (!decrypt_ok) goto done;
    const uint8_t *const plain = (const uint8_t *)out_json;
    const uint8_t pad = plain[cipher_size - 1U];
    if (pad == 0U || pad > 16U || pad > cipher_size) goto done;
    for (uint8_t i = 0U; i < pad; ++i) {
        if (plain[cipher_size - 1U - i] != pad) goto done;
    }
    const size_t plain_size = cipher_size - pad;
    if (plain_size == 0U || plain_size >= out_size) goto done;
    out_json[plain_size] = '\0';
    valid = true;

done:
    secure_zero(cipher, sizeof(cipher));
    secure_zero(iv, sizeof(iv));
    secure_zero(key, sizeof(key));
    if (!valid) secure_zero(out_json, out_size);
    return valid;
}

static bool decode_mdns_state(const mdns_result_t *result,
                              sonoff_lan_device_t *out)
{
    const char *const encrypted = txt_value(result, "encrypt");
    char raw[SONOFF_LAN_STATE_JSON_BYTES] = {0};
    size_t used = 0U;
    for (unsigned int part = 1U; part <= 4U; ++part) {
        char key[8] = {0};
        (void)snprintf(key, sizeof(key), "data%u", part);
        const char *const value = txt_value(result, key);
        if (value == NULL) continue;
        const size_t length = strnlen(value, sizeof(raw));
        if (length >= sizeof(raw) - used) return false;
        memcpy(raw + used, value, length);
        used += length;
    }
    if (used == 0U) return false;

    char device_key[SONOFF_LAN_KEY_BYTES] = {0};
    char plaintext[SONOFF_LAN_STATE_JSON_BYTES] = {0};
    bool decoded = false;
    if (encrypted != NULL && strcmp(encrypted, "true") == 0) {
        const char *const iv = txt_value(result, "iv");
        if (np_ewelink_copy_device_key(out->device_id, device_key,
                                       sizeof(device_key)) == ESP_OK) {
            decoded = decrypt_state_json(raw, iv, device_key, plaintext,
                                          sizeof(plaintext));
        }
    } else {
        memcpy(plaintext, raw, used);
        plaintext[used] = '\0';
        decoded = true;
    }
    if (decoded) {
        cJSON *const root = cJSON_ParseWithLength(plaintext, strlen(plaintext));
        if (root != NULL) {
            (void)parse_switch_state(root, out->state_known, out->switch_on);
            cJSON_Delete(root);
        }
    }
    secure_zero(device_key, sizeof(device_key));
    secure_zero(raw, sizeof(raw));
    secure_zero(plaintext, sizeof(plaintext));
    return decoded;
}

static bool starts_with_ewelink(const char *instance)
{
    static const char prefix[] = "ewelink_";
    if (instance == NULL || strnlen(instance, 9U) < sizeof(prefix) - 1U) return false;
    for (size_t i = 0U; i < sizeof(prefix) - 1U; ++i) {
        if ((char)tolower((unsigned char)instance[i]) != prefix[i]) return false;
    }
    return true;
}

static bool parse_device(const mdns_result_t *result, sonoff_lan_device_t *out)
{
    if (result == NULL || out == NULL ||
        !starts_with_ewelink(result->instance_name)) return false;
    const char *const id = result->instance_name + 8U;
    if (strnlen(id, sizeof(out->device_id)) != 10U) return false;
    for (size_t i = 0U; i < 10U; ++i) {
        if (!isalnum((unsigned char)id[i])) return false;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->device_id, id, 10U);
    out->device_id[10] = '\0';
    for (const mdns_ip_addr_t *address = result->addr;
         address != NULL && out->address[0] == '\0';
         address = address->next) {
        if (address->addr.type != ESP_IPADDR_TYPE_V4) continue;
        const int written = snprintf(out->address, sizeof(out->address), IPSTR,
                                     IP2STR(&address->addr.u_addr.ip4));
        if (written <= 0 || (size_t)written >= sizeof(out->address)) return false;
    }
    copy_text(out->local_type, sizeof(out->local_type), txt_value(result, "type"));
    out->port = result->port != 0U ? result->port : 8081U;
    const char *const encrypted = txt_value(result, "encrypt");
    out->encrypted = encrypted != NULL && strcmp(encrypted, "true") == 0;
    out->online = out->address[0] != '\0';
    out->last_result = out->online ? ESP_OK : ESP_ERR_NOT_FOUND;
    (void)decode_mdns_state(result, out);
    return out->online;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL || event->event_id != HTTP_EVENT_ON_DATA ||
        event->data == NULL || event->data_len <= 0) return ESP_OK;
    const size_t chunk = (size_t)event->data_len;
    if (chunk > SONOFF_LAN_HTTP_BODY_BYTES - s_http_response_size) {
        s_http_response_overflow = true;
        return ESP_FAIL;
    }
    memcpy(s_http_response + s_http_response_size, event->data, chunk);
    s_http_response_size += chunk;
    s_http_response[s_http_response_size] = '\0';
    return ESP_OK;
}

static esp_err_t http_post_local(const char *address, uint16_t port,
                                 const char *command, const char *body,
                                 size_t body_size, int *out_status)
{
    if (address == NULL || command == NULL || body == NULL ||
        out_status == NULL) return ESP_ERR_INVALID_ARG;
    char url[96] = {0};
    const int written = snprintf(url, sizeof(url), "http://%s:%u/zeroconf/%s",
        address, (unsigned int)port, command);
    if (written <= 0 || (size_t)written >= sizeof(url))
        return ESP_ERR_INVALID_SIZE;
    s_http_response_size = 0U;
    s_http_response_overflow = false;
    s_http_response[0] = '\0';
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = SONOFF_LAN_HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .buffer_size = 768,
        .buffer_size_tx = 768,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_set_header(client, "Content-Type",
                                                  "application/json");
    if (result == ESP_OK)
        result = esp_http_client_set_header(client, "Connection", "close");
    if (result == ESP_OK)
        result = esp_http_client_set_post_field(client, body, (int)body_size);
    if (result == ESP_OK) result = esp_http_client_perform(client);
    *out_status = esp_http_client_get_status_code(client);
    if (s_http_response_overflow) result = ESP_ERR_INVALID_SIZE;
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

static esp_err_t build_encrypted_data_request(const char *device_id,
                                             const char *device_key,
                                             const char *data_json,
                                             char *out_body, size_t out_size)
{
    if (device_id == NULL || device_key == NULL || data_json == NULL ||
        out_body == NULL || out_size == 0U)
        return ESP_ERR_INVALID_ARG;

    uint8_t key[16] = {0};
    uint8_t iv[16] = {0};
    uint8_t iv_work[16] = {0};
    uint8_t plaintext[128] = {0};
    uint8_t ciphertext[128] = {0};
    char cipher_b64[192] = {0};
    char iv_b64[32] = {0};
    size_t cipher_size = 0U;
    size_t cipher_b64_size = 0U;
    size_t iv_b64_size = 0U;
    const size_t data_size = strnlen(data_json, sizeof(plaintext) - 16U);
    if (data_size >= sizeof(plaintext) - 16U) {
        secure_zero(key, sizeof(key));
        return ESP_ERR_INVALID_SIZE;
    }
    esp_fill_random(iv, sizeof(iv));
    memcpy(iv_work, iv, sizeof(iv));
    memcpy(plaintext, data_json, data_size);
    const uint8_t padding = (uint8_t)(16U - (data_size % 16U));
    memset(plaintext + data_size, padding, padding);
    cipher_size = data_size + padding;

    mbedtls_md5_context md5;
    mbedtls_md5_init(&md5);
    const size_t key_size = strnlen(device_key, SONOFF_LAN_KEY_BYTES);
    const bool hash_ok = mbedtls_md5_starts(&md5) == 0 &&
        mbedtls_md5_update(&md5, (const unsigned char *)device_key,
                           key_size) == 0 &&
        mbedtls_md5_finish(&md5, key) == 0;
    mbedtls_md5_free(&md5);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const bool encrypted = hash_ok &&
        mbedtls_aes_setkey_enc(&aes, key, 128U) == 0 &&
        mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, cipher_size, iv_work,
                              plaintext, ciphertext) == 0;
    mbedtls_aes_free(&aes);
    esp_err_t result = ESP_FAIL;
    if (!encrypted ||
        mbedtls_base64_encode((unsigned char *)cipher_b64, sizeof(cipher_b64),
            &cipher_b64_size, ciphertext, cipher_size) != 0 ||
        mbedtls_base64_encode((unsigned char *)iv_b64, sizeof(iv_b64),
            &iv_b64_size, iv, sizeof(iv)) != 0) goto done;
    cipher_b64[cipher_b64_size] = '\0';
    iv_b64[iv_b64_size] = '\0';

    const uint64_t now_ms = (uint64_t)esp_timer_get_time() / 1000U;
    const uint64_t sequence = now_ms > s_last_sequence_ms
        ? now_ms : s_last_sequence_ms + 1U;
    s_last_sequence_ms = sequence;
    char sequence_text[24] = {0};
    (void)snprintf(sequence_text, sizeof(sequence_text), "%llu",
                   (unsigned long long)sequence);
    cJSON *request = cJSON_CreateObject();
    if (request == NULL) { result = ESP_ERR_NO_MEM; goto done; }
    if (!cJSON_AddStringToObject(request, "sequence", sequence_text) ||
        !cJSON_AddStringToObject(request, "deviceid", device_id) ||
        !cJSON_AddStringToObject(request, "selfApikey", "123") ||
        !cJSON_AddBoolToObject(request, "encrypt", true) ||
        !cJSON_AddStringToObject(request, "data", cipher_b64) ||
        !cJSON_AddStringToObject(request, "iv", iv_b64)) {
        cJSON_Delete(request);
        result = ESP_ERR_NO_MEM;
        goto done;
    }
    char *serialized = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (serialized == NULL) { result = ESP_ERR_NO_MEM; goto done; }
    const size_t serialized_size = strnlen(serialized, out_size);
    if (serialized_size >= out_size) result = ESP_ERR_INVALID_SIZE;
    else {
        memcpy(out_body, serialized, serialized_size + 1U);
        result = ESP_OK;
    }
    cJSON_free(serialized);

done:
    secure_zero(key, sizeof(key));
    secure_zero(iv, sizeof(iv));
    secure_zero(iv_work, sizeof(iv_work));
    secure_zero(plaintext, sizeof(plaintext));
    secure_zero(ciphertext, sizeof(ciphertext));
    secure_zero(cipher_b64, sizeof(cipher_b64));
    secure_zero(iv_b64, sizeof(iv_b64));
    return result;
}

static esp_err_t build_encrypted_request(const char *device_id,
                                        const char *device_key,
                                        uint8_t channel_count,
                                        uint8_t channel, bool enabled,
                                        char *out_body, size_t out_size)
{
    if (device_id == NULL || device_key == NULL || out_body == NULL ||
        channel_count == 0U || channel_count > 3U || channel >= channel_count)
        return ESP_ERR_INVALID_ARG;

    cJSON *data = cJSON_CreateObject();
    if (data == NULL) return ESP_ERR_NO_MEM;
    char state[4] = {0};
    (void)snprintf(state, sizeof(state), "%s", enabled ? "on" : "off");
    if (channel_count == 1U) {
        if (!cJSON_AddStringToObject(data, "switch", state)) {
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
    } else {
        cJSON *switches = cJSON_AddArrayToObject(data, "switches");
        cJSON *switch_item = cJSON_CreateObject();
        if (switches == NULL || switch_item == NULL ||
            !cJSON_AddNumberToObject(switch_item, "outlet", channel) ||
            !cJSON_AddStringToObject(switch_item, "switch", state)) {
            if (switch_item != NULL) cJSON_Delete(switch_item);
            cJSON_Delete(data);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddItemToArray(switches, switch_item);
    }
    char *data_json = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);
    if (data_json == NULL) return ESP_ERR_NO_MEM;
    const esp_err_t result = build_encrypted_data_request(device_id, device_key,
        data_json, out_body, out_size);
    secure_zero(data_json, strlen(data_json));
    cJSON_free(data_json);
    return result;
}

static esp_err_t build_encrypted_query_request(const char *device_id,
                                              const char *device_key,
                                              char *out_body, size_t out_size)
{
    return build_encrypted_data_request(device_id, device_key, "{}",
                                        out_body, out_size);
}

static int find_device(const sonoff_lan_status_t *status, const char *device_id)
{
    if (status == NULL || device_id == NULL) return -1;
    for (uint8_t i = 0U; i < status->device_count; ++i) {
        if (strcmp(status->devices[i].device_id, device_id) == 0) return (int)i;
    }
    return -1;
}

static bool parse_response_state(const char *key, bool known[3], bool enabled[3])
{
    if (key == NULL || known == NULL || enabled == NULL ||
        s_http_response_size == 0U) return false;
    cJSON *root = cJSON_ParseWithLength(s_http_response, s_http_response_size);
    if (root == NULL) return false;
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *iv = cJSON_GetObjectItemCaseSensitive(root, "iv");
    char plaintext[SONOFF_LAN_STATE_JSON_BYTES] = {0};
    bool found = false;
    if (cJSON_IsNumber(error) && error->valueint == 0) {
        if (cJSON_IsString(data) && data->valuestring != NULL &&
            cJSON_IsString(iv) && iv->valuestring != NULL) {
            if (decrypt_state_json(data->valuestring, iv->valuestring,
                                   key, plaintext, sizeof(plaintext))) {
                cJSON *state = cJSON_ParseWithLength(plaintext, strlen(plaintext));
                if (state != NULL) {
                    found = parse_switch_state(state, known, enabled);
                    cJSON_Delete(state);
                }
            }
        } else if (cJSON_IsObject(data)) {
            found = parse_switch_state(data, known, enabled);
        }
    }
    secure_zero(plaintext, sizeof(plaintext));
    cJSON_Delete(root);
    return found;
}

static void update_response_state(const char *device_id, const char *key,
                                  uint8_t commanded_channel, bool desired_state)
{
    bool known[3] = {0};
    bool enabled[3] = {0};
    if (device_id == NULL || commanded_channel >= 3U) return;
    const bool response_has_state = parse_response_state(key, known, enabled);
    taskENTER_CRITICAL(&s_lock);
    const int index = find_device(&s_status, device_id);
    if (index >= 0) {
        if (response_has_state) {
            for (uint8_t channel = 0U; channel < 3U; ++channel) {
                if (!known[channel]) continue;
                s_status.devices[index].state_known[channel] = true;
                s_status.devices[index].switch_on[channel] = enabled[channel];
            }
        }
        if (!known[commanded_channel]) {
            s_status.devices[index].state_known[commanded_channel] = true;
            s_status.devices[index].switch_on[commanded_channel] = desired_state;
        }
        s_status.devices[index].online = true;
        s_status.devices[index].last_result = ESP_OK;
    }
    taskEXIT_CRITICAL(&s_lock);
}

static esp_err_t query_local_switch_state(const char *device_id,
                                          const char *device_key,
                                          const char *address, uint16_t port,
                                          bool known[3], bool enabled[3])
{
    if (device_id == NULL || device_key == NULL || address == NULL ||
        known == NULL || enabled == NULL) return ESP_ERR_INVALID_ARG;
    static const char *const commands[] = {"getState", "info"};
    esp_err_t result = ESP_ERR_NOT_SUPPORTED;
    char body[SONOFF_LAN_REQUEST_BYTES] = {0};
    for (size_t i = 0U; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (i > 0U) vTaskDelay(pdMS_TO_TICKS(200U));
        int http_status = 0;
        result = build_encrypted_query_request(device_id, device_key,
                                                body, sizeof(body));
        if (result == ESP_OK) {
            result = http_post_local(address, port, commands[i], body,
                                     strlen(body), &http_status);
        }
        secure_zero(body, sizeof(body));
        if (result == ESP_OK && http_status == 200 &&
            parse_response_state(device_key, known, enabled)) {
            result = ESP_OK;
            break;
        }
        if (result != ESP_OK) break;
        if (http_status != 200 && http_status != 404) {
            result = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        result = ESP_ERR_NOT_SUPPORTED;
    }
    secure_zero(s_http_response, sizeof(s_http_response));
    s_http_response_size = 0U;
    s_http_response_overflow = false;
    secure_zero(body, sizeof(body));
    return result;
}

static esp_err_t process_switch_command(const char *device_id, uint8_t channel,
                                       bool enabled)
{
    sonoff_lan_status_t snapshot = {0};
    np_ewelink_public_inventory_t inventory = {0};
    char device_key[SONOFF_LAN_KEY_BYTES] = {0};
    char body[SONOFF_LAN_REQUEST_BYTES] = {0};
    char address[16] = {0};
    uint16_t port = 0U;
    uint8_t channel_count = 0U;
    int http_status = 0;

    sonoff_lan_service_get_status(&snapshot);
    const int index = find_device(&snapshot, device_id);
    if (index < 0 || !snapshot.devices[index].online) return ESP_ERR_NOT_FOUND;
    copy_text(address, sizeof(address), snapshot.devices[index].address);
    port = snapshot.devices[index].port;
    esp_err_t result = np_ewelink_get_public_inventory(&inventory);
    if (result != ESP_OK) goto done;
    for (uint8_t i = 0U; i < inventory.count; ++i) {
        if (strcmp(inventory.devices[i].device_id, device_id) != 0) continue;
        channel_count = inventory.devices[i].channel_count;
        break;
    }
    if (channel_count == 0U || channel_count > 3U) {
        result = ESP_ERR_NOT_SUPPORTED;
        goto done;
    }
    result = np_ewelink_copy_device_key(device_id, device_key,
                                        sizeof(device_key));
    if (result != ESP_OK) goto done;
    result = build_encrypted_request(device_id, device_key, channel_count,
                                     channel, enabled, body, sizeof(body));
    if (result != ESP_OK) goto done;
    result = http_post_local(address, port,
        channel_count == 1U ? "switch" : "switches", body, strlen(body),
        &http_status);
    if (result == ESP_OK && http_status != 200) result = ESP_FAIL;
    if (result == ESP_OK) {
        cJSON *ack = cJSON_ParseWithLength(s_http_response, s_http_response_size);
        const cJSON *error = ack != NULL
            ? cJSON_GetObjectItemCaseSensitive(ack, "error") : NULL;
        if (!cJSON_IsNumber(error) || error->valueint != 0)
            result = ESP_ERR_INVALID_RESPONSE;
        if (ack != NULL) cJSON_Delete(ack);
    }
    if (result == ESP_OK) {
        update_response_state(device_id, device_key, channel, enabled);
    }

done:
    taskENTER_CRITICAL(&s_lock);
    const int current = find_device(&s_status, device_id);
    if (current >= 0) {
        s_status.devices[current].channel_pending[channel] = false;
        s_status.devices[current].last_result = result;
    }
    s_status.command_busy = false;
    s_status.last_result = result;
    taskEXIT_CRITICAL(&s_lock);
    secure_zero(&inventory, sizeof(inventory));
    secure_zero(device_key, sizeof(device_key));
    secure_zero(body, sizeof(body));
    secure_zero(address, sizeof(address));
    secure_zero(s_http_response, sizeof(s_http_response));
    s_http_response_size = 0U;
    return result;
}

static void scan_devices(void)
{
    connectivity_diagnostic_status_t network = {0};
    connectivity_diagnostic_get_status(&network);
    if (!network.online) {
        taskENTER_CRITICAL(&s_lock);
        s_status.last_result = ESP_ERR_INVALID_STATE;
        s_status.scan_busy = false;
        s_status.scan_generation++;
        taskEXIT_CRITICAL(&s_lock);
        return;
    }

    if (!s_mdns_initialized) {
        const esp_err_t init_result = mdns_init();
        if (init_result != ESP_OK) {
            taskENTER_CRITICAL(&s_lock);
            s_status.last_result = init_result;
            s_status.scan_busy = false;
            s_status.scan_generation++;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
        s_mdns_initialized = true;
        (void)mdns_hostname_set("novapanel");
    }

    mdns_result_t *results = NULL;
    const esp_err_t query_result = mdns_query_ptr(
        "_ewelink", "_tcp", SONOFF_LAN_SCAN_TIMEOUT_MS,
        SONOFF_LAN_MAX_DISCOVERED, &results);
    sonoff_lan_device_t discovered[SONOFF_LAN_MAX_DISCOVERED] = {0};
    uint8_t count = 0U;
    size_t max_mdns_data_bytes = 0U;
    if (query_result == ESP_OK) {
        for (const mdns_result_t *result = results;
             result != NULL && count < SONOFF_LAN_MAX_DISCOVERED;
             result = result->next) {
            size_t data_bytes = 0U;
            for (unsigned int part = 1U; part <= 4U; ++part) {
                char key[8] = {0};
                (void)snprintf(key, sizeof(key), "data%u", part);
                const char *const value = txt_value(result, key);
                if (value != NULL)
                    data_bytes += strnlen(value, SONOFF_LAN_STATE_JSON_BYTES);
            }
            if (data_bytes > max_mdns_data_bytes)
                max_mdns_data_bytes = data_bytes;
            sonoff_lan_device_t candidate = {0};
            if (!parse_device(result, &candidate)) continue;
            bool duplicate = false;
            for (uint8_t i = 0U; i < count; ++i)
                duplicate |= strcmp(discovered[i].device_id,
                                    candidate.device_id) == 0;
            if (!duplicate) discovered[count++] = candidate;
        }
    }
    if (results != NULL) mdns_query_results_free(results);

    secure_zero(s_scan_workspace, sizeof(*s_scan_workspace));
    np_ewelink_public_inventory_t *const inventory = &s_scan_workspace->inventory;
    sonoff_lan_status_t *const next = &s_scan_workspace->next;
    (void)np_ewelink_get_public_inventory(inventory);
    next->ready = true;
    next->last_result = query_result;
    next->discovered_count = count;
    for (uint8_t i = 0U; i < count; ++i) {
        next->devices[next->device_count++] = discovered[i];
    }
    uint8_t state_query_count = 0U;
    uint8_t state_query_success_count = 0U;
    uint8_t state_query_timeout_count = 0U;
    uint8_t state_query_unsupported_count = 0U;
    uint8_t state_query_other_error_count = 0U;
    for (uint8_t i = 0U; i < next->device_count; ++i) {
        sonoff_lan_device_t *const device = &next->devices[i];
        if (!device->online) continue;
        bool has_state = false;
        for (uint8_t channel = 0U; channel < 3U; ++channel)
            has_state |= device->state_known[channel];
        if (has_state) continue;
        if (state_query_count > 0U) vTaskDelay(pdMS_TO_TICKS(200U));
        state_query_count++;
        char device_key[SONOFF_LAN_KEY_BYTES] = {0};
        bool known[3] = {0};
        bool enabled[3] = {0};
        esp_err_t query_result = np_ewelink_copy_device_key(
            device->device_id, device_key, sizeof(device_key));
        if (query_result == ESP_OK) {
            query_result = query_local_switch_state(device->device_id,
                device_key, device->address, device->port, known, enabled);
        }
        if (query_result == ESP_OK) {
            for (uint8_t channel = 0U; channel < 3U; ++channel) {
                if (!known[channel]) continue;
                device->state_known[channel] = true;
                device->switch_on[channel] = enabled[channel];
            }
            state_query_success_count++;
        } else if (query_result == ESP_ERR_HTTP_EAGAIN ||
                   query_result == ESP_ERR_TIMEOUT) {
            state_query_timeout_count++;
        } else if (query_result == ESP_ERR_NOT_SUPPORTED) {
            state_query_unsupported_count++;
        } else {
            state_query_other_error_count++;
        }
        secure_zero(device_key, sizeof(device_key));
    }
    taskENTER_CRITICAL(&s_lock);
    next->scan_generation = s_status.scan_generation + 1U;
    next->scan_busy = false;
    next->command_busy = s_status.command_busy;
    for (uint8_t i = 0U; i < next->device_count; ++i) {
        const int previous = find_device(&s_status, next->devices[i].device_id);
        if (previous < 0) continue;
        const sonoff_lan_device_t *const old = &s_status.devices[previous];
        if (next->devices[i].online) {
            next->devices[i].consecutive_scan_misses = 0U;
            /* TXT state can be absent from a valid announcement. Keep the
             * last decoded state while this same device remains online. */
            if (old->online &&
                old->consecutive_scan_misses < SONOFF_LAN_MISSES_BEFORE_OFFLINE) {
                for (uint8_t channel = 0U; channel < 3U; ++channel) {
                    if (next->devices[i].state_known[channel] ||
                        !old->state_known[channel]) continue;
                    next->devices[i].state_known[channel] = true;
                    next->devices[i].switch_on[channel] = old->switch_on[channel];
                }
            }
        }
        memcpy(next->devices[i].channel_pending,
               old->channel_pending,
               sizeof(next->devices[i].channel_pending));
    }
    /* A single mDNS miss is not enough evidence to flip a working device
     * offline. The next successful scan confirms absence; the second miss
     * keeps the inventory card but disables stale channel state. */
    for (uint8_t i = 0U; i < s_status.device_count &&
         next->device_count < SONOFF_LAN_MAX_DISCOVERED; ++i) {
        const sonoff_lan_device_t *const old = &s_status.devices[i];
        if (find_device(next, old->device_id) >= 0) continue;
        sonoff_lan_device_t *const retained = &next->devices[next->device_count++];
        *retained = *old;
        const uint8_t misses = old->consecutive_scan_misses < UINT8_MAX
            ? (uint8_t)(old->consecutive_scan_misses + 1U) : UINT8_MAX;
        retained->consecutive_scan_misses = misses;
        if (!old->online || misses >= SONOFF_LAN_MISSES_BEFORE_OFFLINE) {
            retained->online = false;
            retained->address[0] = '\0';
            memset(retained->state_known, 0, sizeof(retained->state_known));
            memset(retained->switch_on, 0, sizeof(retained->switch_on));
            retained->last_result = ESP_ERR_NOT_FOUND;
        }
    }
    for (uint8_t i = 0U; i < inventory->count &&
         next->device_count < SONOFF_LAN_MAX_DISCOVERED; ++i) {
        const np_ewelink_public_device_t *const known = &inventory->devices[i];
        if (find_device(next, known->device_id) >= 0) continue;
        sonoff_lan_device_t *const offline = &next->devices[next->device_count++];
        copy_text(offline->device_id, sizeof(offline->device_id), known->device_id);
        offline->port = 8081U;
        offline->last_result = ESP_ERR_NOT_FOUND;
    }
    s_status = *next;
    taskEXIT_CRITICAL(&s_lock);
    uint8_t online_count = 0U;
    uint8_t state_known_count = 0U;
    uint8_t channels_known_count = 0U;
    uint8_t channels_expected_count = 0U;
    for (uint8_t i = 0U; i < next->device_count; ++i) {
        if (!next->devices[i].online) continue;
        online_count++;
        for (uint8_t j = 0U; j < inventory->count; ++j) {
            if (strcmp(next->devices[i].device_id,
                       inventory->devices[j].device_id) != 0) continue;
            channels_expected_count += inventory->devices[j].channel_count;
            break;
        }
        bool device_has_state = false;
        for (uint8_t channel = 0U; channel < 3U; ++channel) {
            if (!next->devices[i].state_known[channel]) continue;
            channels_known_count++;
            device_has_state = true;
        }
        if (device_has_state) state_known_count++;
    }
    ESP_LOGI(TAG, "LAN discovery result=%s devices=%u imported=%u online=%u stateful=%u channels_known=%u channels_expected=%u max_mdns_b64=%u state_queries=%u query_ok=%u query_timeout=%u query_unsupported=%u query_other=%u stack_min_free=%u",
             esp_err_to_name(query_result), (unsigned int)count,
             (unsigned int)inventory->count, (unsigned int)online_count,
             (unsigned int)state_known_count,
             (unsigned int)channels_known_count,
             (unsigned int)channels_expected_count,
             (unsigned int)max_mdns_data_bytes,
             (unsigned int)state_query_count,
             (unsigned int)state_query_success_count,
             (unsigned int)state_query_timeout_count,
             (unsigned int)state_query_unsupported_count,
             (unsigned int)state_query_other_error_count,
             (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    secure_zero(discovered, sizeof(discovered));
    secure_zero(s_scan_workspace, sizeof(*s_scan_workspace));
}

static void sonoff_lan_worker(void *arg)
{
    (void)arg;
    uint64_t next_scan_ms = 0U;
    while (true) {
        bool scan = false;
        bool command = false;
        char device_id[SONOFF_LAN_ID_BYTES] = {0};
        uint8_t channel = 0U;
        bool enabled = false;

        taskENTER_CRITICAL(&s_lock);
        if (s_command_requested) {
            command = true;
            copy_text(device_id, sizeof(device_id), s_pending_command.device_id);
            channel = s_pending_command.channel;
            enabled = s_pending_command.enabled;
            s_command_requested = false;
            secure_zero(&s_pending_command, sizeof(s_pending_command));
        } else if (s_scan_requested) {
            scan = true;
            s_scan_requested = false;
        } else if (!s_status.scan_busy &&
                   ((uint64_t)esp_timer_get_time() / 1000U) >= next_scan_ms) {
            scan = true;
        }
        taskEXIT_CRITICAL(&s_lock);

        if (command) {
            const esp_err_t result = process_switch_command(device_id, channel,
                                                            enabled);
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "LAN switch failed device=%s channel=%u result=%s",
                         device_id, (unsigned int)channel,
                         esp_err_to_name(result));
            }
            secure_zero(device_id, sizeof(device_id));
        } else if (scan) {
            taskENTER_CRITICAL(&s_lock);
            s_status.scan_busy = true;
            taskEXIT_CRITICAL(&s_lock);
            scan_devices();
            uint32_t scan_interval_ms = SONOFF_LAN_SCAN_INTERVAL_MS;
            taskENTER_CRITICAL(&s_lock);
            for (uint8_t i = 0U; i < s_status.device_count; ++i) {
                if (s_status.devices[i].consecutive_scan_misses > 0U &&
                    s_status.devices[i].online) {
                    scan_interval_ms = SONOFF_LAN_SCAN_CONFIRM_INTERVAL_MS;
                    break;
                }
            }
            taskEXIT_CRITICAL(&s_lock);
            next_scan_ms = (uint64_t)esp_timer_get_time() / 1000U +
                           scan_interval_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(SONOFF_LAN_POLL_MS));
    }
}

esp_err_t sonoff_lan_service_start(void)
{
    if (s_started) return ESP_OK;
    s_scan_workspace = heap_caps_calloc(1U, sizeof(*s_scan_workspace),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_scan_workspace == NULL) return ESP_ERR_NO_MEM;
    const BaseType_t created = xTaskCreate(sonoff_lan_worker, "sonoff_lan",
        SONOFF_LAN_TASK_STACK_BYTES, NULL, SONOFF_LAN_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        heap_caps_free(s_scan_workspace);
        s_scan_workspace = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    taskENTER_CRITICAL(&s_lock);
    s_status.ready = true;
    s_status.scan_busy = true;
    s_scan_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t sonoff_lan_service_request_scan(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&s_lock);
    if (s_status.scan_busy || s_scan_requested) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_status.scan_busy = true;
    s_status.last_result = ESP_ERR_TIMEOUT;
    s_scan_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t sonoff_lan_service_request_switch(const char *device_id,
                                             uint8_t channel, bool enabled)
{
    if (!s_started || device_id == NULL ||
        strnlen(device_id, SONOFF_LAN_ID_BYTES) != 10U || channel >= 3U)
        return ESP_ERR_INVALID_ARG;
    for (size_t i = 0U; i < 10U; ++i) {
        const unsigned char c = (unsigned char)device_id[i];
        if (!isalnum(c)) return ESP_ERR_INVALID_ARG;
    }
    taskENTER_CRITICAL(&s_lock);
    if (s_status.command_busy || s_command_requested) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    const int index = find_device(&s_status, device_id);
    if (index < 0 || !s_status.devices[index].online) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    s_status.command_busy = true;
    s_status.devices[index].channel_pending[channel] = true;
    s_status.devices[index].last_result = ESP_ERR_TIMEOUT;
    memcpy(s_pending_command.device_id, device_id, 10U);
    s_pending_command.device_id[10] = '\0';
    s_pending_command.channel = channel;
    s_pending_command.enabled = enabled;
    s_command_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

void sonoff_lan_service_get_status(sonoff_lan_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
