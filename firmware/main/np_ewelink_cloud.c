#include "np_ewelink_cloud.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "np_ewelink_auth.h"

#define EWELINK_HTTP_BODY_MAX_BYTES (48U * 1024U)
#define EWELINK_LOGIN_BODY_MAX_BYTES 1024U
#define EWELINK_HTTP_TIMEOUT_MS 10000
#define EWELINK_ACCESS_TOKEN_BYTES 512U

static const char *const TAG = "np_ewelink_cloud";
static uint8_t *s_response_body;
static size_t s_response_size;
static bool s_response_overflow;

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) *bytes++ = 0U;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL || event->event_id != HTTP_EVENT_ON_DATA || event->data == NULL ||
        event->data_len <= 0) return ESP_OK;
    if (s_response_body == NULL) return ESP_ERR_INVALID_STATE;
    const size_t chunk = (size_t)event->data_len;
    if (chunk > EWELINK_HTTP_BODY_MAX_BYTES - s_response_size) {
        s_response_overflow = true;
        return ESP_FAIL;
    }
    memcpy(s_response_body + s_response_size, event->data, chunk);
    s_response_size += chunk;
    s_response_body[s_response_size] = 0U;
    return ESP_OK;
}

static esp_err_t http_request(const char *url, esp_http_client_method_t method,
                              const char *authorization, const uint8_t *post_body,
                              size_t post_body_size, int *out_status,
                              size_t *out_response_size)
{
    if (url == NULL || out_status == NULL || out_response_size == NULL ||
        (post_body == NULL && post_body_size != 0U)) return ESP_ERR_INVALID_ARG;
    if (s_response_body == NULL) return ESP_ERR_INVALID_STATE;
    secure_zero(s_response_body, EWELINK_HTTP_BODY_MAX_BYTES + 1U);
    s_response_size = 0U;
    s_response_overflow = false;
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = EWELINK_HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* JSON is streamed to PSRAM by http_event_handler; keep the HTTP
         * client's own buffers bounded independently of TLS/AES allocation. */
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_set_header(client, "X-CK-Appid",
                                                  np_ewelink_auth_app_id());
    if (result == ESP_OK) result = esp_http_client_set_header(client, "Accept", "application/json");
    if (result == ESP_OK && authorization != NULL)
        result = esp_http_client_set_header(client, "Authorization", authorization);
    if (result == ESP_OK && method == HTTP_METHOD_POST) {
        result = esp_http_client_set_header(client, "Content-Type", "application/json");
        if (result == ESP_OK) result = esp_http_client_set_post_field(
            client, (const char *)post_body, (int)post_body_size);
    }
    if (result == ESP_OK) result = esp_http_client_perform(client);
    *out_status = esp_http_client_get_status_code(client);
    *out_response_size = s_response_size;
    if (s_response_overflow) result = ESP_ERR_INVALID_SIZE;
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

static esp_err_t append_json_escaped(char *out, size_t out_size, size_t *used,
                                    const char *input)
{
    if (out == NULL || out_size == 0U || used == NULL || input == NULL)
        return ESP_ERR_INVALID_ARG;
    for (const unsigned char *cursor = (const unsigned char *)input; *cursor != 0U; ++cursor) {
        const char *escape = NULL;
        char encoded[7];
        size_t encoded_size = 0U;
        switch (*cursor) {
        case '"': escape = "\\\""; encoded_size = 2U; break;
        case '\\': escape = "\\\\"; encoded_size = 2U; break;
        case '\b': escape = "\\b"; encoded_size = 2U; break;
        case '\f': escape = "\\f"; encoded_size = 2U; break;
        case '\n': escape = "\\n"; encoded_size = 2U; break;
        case '\r': escape = "\\r"; encoded_size = 2U; break;
        case '\t': escape = "\\t"; encoded_size = 2U; break;
        default:
            if (*cursor < 0x20U) {
                const int count = snprintf(encoded, sizeof(encoded), "\\u%04x", *cursor);
                if (count != 6) return ESP_FAIL;
                escape = encoded;
                encoded_size = 6U;
            } else {
                encoded[0] = (char)*cursor;
                escape = encoded;
                encoded_size = 1U;
            }
        }
        if (encoded_size >= out_size - *used) return ESP_ERR_INVALID_SIZE;
        memcpy(out + *used, escape, encoded_size);
        *used += encoded_size;
    }
    return ESP_OK;
}

static esp_err_t make_login_body(const char *username, const char *password,
                                 char *out_body, size_t out_size, size_t *out_length)
{
    if (username == NULL || password == NULL || out_body == NULL || out_size == 0U ||
        out_length == NULL) return ESP_ERR_INVALID_ARG;
    const size_t user_length = strnlen(username, 96U);
    const size_t pass_length = strnlen(password, 128U);
    if (user_length == 0U || user_length >= 96U || pass_length == 0U || pass_length >= 128U)
        return ESP_ERR_INVALID_SIZE;
    const bool is_email = strchr(username, '@') != NULL;
    char phone_number[100] = {0};
    const char *login_value = username;
    if (!is_email && username[0] != '+') {
        const int n = snprintf(phone_number, sizeof(phone_number), "+%s", username);
        if (n <= 0 || (size_t)n >= sizeof(phone_number)) return ESP_ERR_INVALID_SIZE;
        login_value = phone_number;
    }

    size_t used = 0U;
    const char *prefix = "{\"password\": \"";
    const char *middle = is_email ? "\", \"countryCode\": \"+55\", \"email\": \""
                                  : "\", \"countryCode\": \"+55\", \"phoneNumber\": \"";
    const char *suffix = "\"}";
    size_t prefix_len = strlen(prefix);
    if (prefix_len >= out_size) return ESP_ERR_INVALID_SIZE;
    memcpy(out_body, prefix, prefix_len); used += prefix_len;
    esp_err_t result = append_json_escaped(out_body, out_size, &used, password);
    size_t middle_len = strlen(middle);
    if (result == ESP_OK && middle_len < out_size - used) {
        memcpy(out_body + used, middle, middle_len); used += middle_len;
    } else if (result == ESP_OK) result = ESP_ERR_INVALID_SIZE;
    if (result == ESP_OK) result = append_json_escaped(out_body, out_size, &used, login_value);
    size_t suffix_len = strlen(suffix);
    if (result == ESP_OK && suffix_len < out_size - used) {
        memcpy(out_body + used, suffix, suffix_len); used += suffix_len;
        out_body[used] = '\0';
        *out_length = used;
    } else if (result == ESP_OK) result = ESP_ERR_INVALID_SIZE;
    secure_zero(phone_number, sizeof(phone_number));
    return result;
}

static const cJSON *object_item(const cJSON *object, const char *name)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsObject(item) || cJSON_IsArray(item) || cJSON_IsString(item) ||
           cJSON_IsNumber(item) || cJSON_IsBool(item) ? item : NULL;
}

static void copy_json_string(char *out, size_t capacity, const cJSON *object,
                             const char *name)
{
    if (out == NULL || capacity == 0U) return;
    out[0] = '\0';
    const cJSON *value = object_item(object, name);
    if (!cJSON_IsString(value) || value->valuestring == NULL) return;
    const size_t length = strnlen(value->valuestring, capacity);
    if (length < capacity) memcpy(out, value->valuestring, length + 1U);
}

static bool read_error(const cJSON *root, int *out_error)
{
    const cJSON *error = object_item(root, "error");
    if (!cJSON_IsNumber(error) || out_error == NULL) return false;
    *out_error = error->valueint;
    return true;
}

static esp_err_t parse_login_token(size_t body_size, char *out_token,
                                   size_t token_size, char *out_region,
                                   size_t region_size, int *out_error)
{
    cJSON *root = cJSON_ParseWithLength((const char *)s_response_body, body_size);
    if (root == NULL) return ESP_ERR_INVALID_RESPONSE;
    const cJSON *data = object_item(root, "data");
    int error = -1;
    if (!read_error(root, &error)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (out_error != NULL) *out_error = error;
    if (error == 10004) copy_json_string(out_region, region_size, data, "region");
    if (error == 0) copy_json_string(out_token, token_size, data, "at");
    cJSON_Delete(root);
    return error == 0 || error == 10004 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t do_login(const char *username, const char *password,
                          char *out_token, size_t token_size, char *out_region,
                          size_t region_size)
{
    char login_body[EWELINK_LOGIN_BODY_MAX_BYTES] = {0};
    char signature[64] = {0};
    char authorization[80] = {0};
    size_t login_body_size = 0U, response_size = 0U;
    int http_status = 0;
    int api_error = -1;
    const char *stage = "serialize";
    esp_err_t result = make_login_body(username, password, login_body,
                                       sizeof(login_body), &login_body_size);
    if (result == ESP_OK) {
        stage = "sign";
        result = np_ewelink_auth_sign_json((const uint8_t *)login_body,
            login_body_size, signature, sizeof(signature));
    }
    if (result == ESP_OK) {
        const int count = snprintf(authorization, sizeof(authorization), "Sign %s", signature);
        if (count <= 0 || (size_t)count >= sizeof(authorization)) result = ESP_ERR_INVALID_SIZE;
    }
    if (result == ESP_OK) {
        const char *region = np_ewelink_auth_region_for_country("+55");
        const char *host = np_ewelink_auth_api_host(region);
        char url[160] = {0};
        int n = host != NULL ? snprintf(url, sizeof(url), "%s/v2/user/login", host) : -1;
        if (n <= 0 || (size_t)n >= sizeof(url)) result = ESP_ERR_INVALID_ARG;
        if (result == ESP_OK) {
            stage = "login_http";
            result = http_request(url, HTTP_METHOD_POST, authorization,
                                  (const uint8_t *)login_body, login_body_size,
                                  &http_status, &response_size);
        }
        if (result == ESP_OK && http_status != 200) result = ESP_FAIL;
        if (result == ESP_OK) {
            stage = "login_response";
            result = parse_login_token(response_size, out_token, token_size,
                                       out_region, region_size, &api_error);
        }
        if (result == ESP_OK && api_error == 10004) {
            host = np_ewelink_auth_api_host(out_region);
            n = host != NULL ? snprintf(url, sizeof(url), "%s/v2/user/login", host) : -1;
            if (n <= 0 || (size_t)n >= sizeof(url)) result = ESP_ERR_INVALID_RESPONSE;
            if (result == ESP_OK) {
                stage = "regional_login_http";
                result = http_request(url, HTTP_METHOD_POST, authorization,
                    (const uint8_t *)login_body, login_body_size, &http_status,
                    &response_size);
            }
            if (result == ESP_OK && http_status != 200) result = ESP_FAIL;
            if (result == ESP_OK) {
                stage = "regional_login_response";
                result = parse_login_token(response_size, out_token, token_size,
                                           out_region, region_size, &api_error);
            }
            if (result == ESP_OK && api_error != 0) result = ESP_ERR_INVALID_RESPONSE;
        } else if (result == ESP_OK && api_error != 0) {
            result = ESP_ERR_INVALID_RESPONSE;
        }
    }
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "login succeeded region=%s", out_region);
    } else {
        ESP_LOGW(TAG, "login failed stage=%s result=%s http=%d api_error=%d region=%s",
                 stage, esp_err_to_name(result), http_status, api_error, out_region);
    }
    secure_zero(login_body, sizeof(login_body));
    secure_zero(signature, sizeof(signature));
    secure_zero(authorization, sizeof(authorization));
    secure_zero(s_response_body, EWELINK_HTTP_BODY_MAX_BYTES + 1U);
    s_response_size = 0U;
    return result;
}

static np_ewelink_model_t model_from_product(const char *product, uint8_t *channels)
{
    *channels = 0U;
    if (strcmp(product, "TX1C") == 0) { *channels = 1U; return NP_EWELINK_TX1C; }
    if (strcmp(product, "TX2C") == 0) { *channels = 2U; return NP_EWELINK_TX2C; }
    if (strcmp(product, "TX3C") == 0) { *channels = 3U; return NP_EWELINK_TX3C; }
    return NP_EWELINK_UNKNOWN;
}

static void parse_channel_names(const cJSON *tags, np_ewelink_device_t *device)
{
    const cJSON *names = object_item(tags, "ck_channel_name");
    for (uint8_t index = 0U; index < device->channel_count; ++index) {
        char key[4] = {0};
        (void)snprintf(key, sizeof(key), "%u", (unsigned int)index);
        const cJSON *name = names != NULL ? cJSON_GetObjectItemCaseSensitive(names, key) : NULL;
        if (cJSON_IsString(name) && name->valuestring != NULL &&
            strnlen(name->valuestring, sizeof(device->channel_names[index])) <
                sizeof(device->channel_names[index])) {
            memcpy(device->channel_names[index], name->valuestring,
                   strlen(name->valuestring) + 1U);
        } else {
            (void)snprintf(device->channel_names[index],
                           sizeof(device->channel_names[index]), "Canal %u",
                           (unsigned int)(index + 1U));
        }
    }
}

static esp_err_t parse_device_item(const cJSON *item_data, np_ewelink_device_t *out,
                                  bool *out_valid)
{
    if (out_valid == NULL || out == NULL) return ESP_ERR_INVALID_ARG;
    *out_valid = false;
    if (!cJSON_IsObject(item_data)) return ESP_OK;
    memset(out, 0, sizeof(*out));
    copy_json_string(out->device_id, sizeof(out->device_id), item_data, "deviceid");
    if (out->device_id[0] == '\0') return ESP_OK;
    copy_json_string(out->device_key, sizeof(out->device_key), item_data, "devicekey");
    copy_json_string(out->api_key, sizeof(out->api_key), item_data, "apikey");
    copy_json_string(out->name, sizeof(out->name), item_data, "name");
    copy_json_string(out->brand_name, sizeof(out->brand_name), item_data, "brandName");
    copy_json_string(out->product_model, sizeof(out->product_model), item_data, "productModel");
    const cJSON *extra = object_item(item_data, "extra");
    copy_json_string(out->internal_model, sizeof(out->internal_model), extra, "model");
    copy_json_string(out->sta_mac, sizeof(out->sta_mac), extra, "staMac");
    const cJSON *uiid = object_item(extra, "uiid");
    if (cJSON_IsNumber(uiid)) out->uiid = uiid->valueint;
    const cJSON *online = object_item(item_data, "online");
    out->online = cJSON_IsTrue(online) || (cJSON_IsNumber(online) && online->valueint != 0);
    out->model = model_from_product(out->product_model, &out->channel_count);
    parse_channel_names(object_item(item_data, "tags"), out);
    const cJSON *params = object_item(item_data, "params");
    if (params != NULL) {
        char *serialized = cJSON_PrintUnformatted(params);
        if (serialized == NULL) return ESP_ERR_NO_MEM;
        const size_t length = strnlen(serialized, sizeof(out->params_json));
        if (length >= sizeof(out->params_json)) {
            cJSON_free(serialized);
            return ESP_ERR_INVALID_SIZE;
        }
        memcpy(out->params_json, serialized, length + 1U);
        cJSON_free(serialized);
    }
    out->present_last_sync = true;
    *out_valid = true;
    return ESP_OK;
}

static esp_err_t fetch_inventory(const char *region, const char *token,
                                 np_ewelink_inventory_t *out_inventory)
{
    const char *host = np_ewelink_auth_api_host(region);
    if (host == NULL || token == NULL || out_inventory == NULL) return ESP_ERR_INVALID_ARG;
    char url[160] = {0};
    char authorization[EWELINK_ACCESS_TOKEN_BYTES + 16U] = {0};
    int n = snprintf(url, sizeof(url), "%s/v2/device/thing?num=0", host);
    if (n <= 0 || (size_t)n >= sizeof(url)) return ESP_ERR_INVALID_SIZE;
    n = snprintf(authorization, sizeof(authorization), "Bearer %s", token);
    if (n <= 0 || (size_t)n >= sizeof(authorization)) return ESP_ERR_INVALID_SIZE;
    int http_status = 0;
    size_t response_size = 0U;
    esp_err_t result = http_request(url, HTTP_METHOD_GET, authorization, NULL, 0U,
                                    &http_status, &response_size);
    secure_zero(authorization, sizeof(authorization));
    if (result != ESP_OK) return result;
    if (http_status != 200) result = ESP_FAIL;
    cJSON *root = result == ESP_OK
                      ? cJSON_ParseWithLength((const char *)s_response_body, response_size)
                      : NULL;
    if (root == NULL) result = ESP_ERR_INVALID_RESPONSE;
    int api_error = -1;
    if (result == ESP_OK && (!read_error(root, &api_error) || api_error != 0))
        result = ESP_ERR_INVALID_RESPONSE;
    const cJSON *data = result == ESP_OK ? object_item(root, "data") : NULL;
    const cJSON *thing_list = object_item(data, "thingList");
    if (result == ESP_OK && !cJSON_IsArray(thing_list)) result = ESP_ERR_INVALID_RESPONSE;

    secure_zero(out_inventory, sizeof(*out_inventory));
    if (result == ESP_OK) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, thing_list) {
            const cJSON *item_data = object_item(entry, "itemData");
            np_ewelink_device_t candidate = {0};
            bool valid = false;
            result = parse_device_item(item_data, &candidate, &valid);
            if (result != ESP_OK) break;
            if (!valid) continue;
            bool duplicate = false;
            for (uint8_t index = 0U; index < out_inventory->count; ++index)
                duplicate |= strcmp(out_inventory->devices[index].device_id,
                                    candidate.device_id) == 0;
            if (duplicate) continue;
            if (out_inventory->count >= NP_EWELINK_MAX_DEVICES) {
                secure_zero(&candidate, sizeof(candidate));
                result = ESP_ERR_INVALID_SIZE;
                break;
            }
            out_inventory->devices[out_inventory->count++] = candidate;
            secure_zero(&candidate, sizeof(candidate));
        }
    }
    if (root != NULL) cJSON_Delete(root);
    secure_zero(s_response_body, EWELINK_HTTP_BODY_MAX_BYTES + 1U);
    s_response_size = 0U;
    if (result != ESP_OK) secure_zero(out_inventory, sizeof(*out_inventory));
    return result;
}

esp_err_t np_ewelink_cloud_login_and_fetch(const char *username,
                                          const char *password,
                                          np_ewelink_cloud_progress_cb_t progress,
                                          void *progress_context,
                                          np_ewelink_inventory_t *out_inventory)
{
    if (username == NULL || password == NULL || out_inventory == NULL)
        return ESP_ERR_INVALID_ARG;
    if (s_response_body != NULL) return ESP_ERR_INVALID_STATE;
    /* Keep the bounded cloud payload out of scarce internal SRAM. In the
     * devices UI, the largest internal block can be smaller than 48 KiB; the
     * TLS client itself continues to use its configured internal buffers. */
    s_response_body = heap_caps_calloc(1U, EWELINK_HTTP_BODY_MAX_BYTES + 1U,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_response_body == NULL) return ESP_ERR_NO_MEM;
    char token[EWELINK_ACCESS_TOKEN_BYTES] = {0};
    char region[8] = {0};
    const char *initial_region = np_ewelink_auth_region_for_country("+55");
    (void)snprintf(region, sizeof(region), "%s", initial_region != NULL ? initial_region : "us");
    esp_err_t result = do_login(username, password, token, sizeof(token), region, sizeof(region));
    if (result == ESP_OK && token[0] != '\0') {
        if (progress != NULL) progress(NP_EWELINK_SYNC_FETCHING, progress_context);
        result = fetch_inventory(region, token, out_inventory);
    }
    else if (result == ESP_OK) result = ESP_ERR_INVALID_RESPONSE;
    if (result == ESP_OK) ESP_LOGI(TAG, "cloud inventory fetched count=%u",
                                   (unsigned int)out_inventory->count);
    secure_zero(token, sizeof(token));
    secure_zero(region, sizeof(region));
    secure_zero(s_response_body, EWELINK_HTTP_BODY_MAX_BYTES + 1U);
    s_response_size = 0U;
    heap_caps_free(s_response_body);
    s_response_body = NULL;
    return result;
}
