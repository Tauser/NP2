#include "np_ewelink.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "network_validation_service.h"
#include "np_ewelink_cloud.h"
#include "np_ewelink_storage.h"

#define EWELINK_USERNAME_BYTES 96U
#define EWELINK_PASSWORD_BYTES 128U

static const char *const TAG = "np_ewelink";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized;
static np_ewelink_status_t s_status = {.last_result = ESP_ERR_INVALID_STATE};
static np_ewelink_inventory_t s_inventory;
typedef struct {
    np_ewelink_inventory_t cloud;
    np_ewelink_inventory_t reconciled;
    np_ewelink_inventory_t local_snapshot;
} ewelink_sync_workspace_t;
static SemaphoreHandle_t s_inventory_mutex;
static char s_pending_username[EWELINK_USERNAME_BYTES];
static char s_pending_password[EWELINK_PASSWORD_BYTES];
static bool s_credentials_pending;

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) *bytes++ = 0U;
}

static void set_sync_stage(np_ewelink_sync_stage_t stage)
{
    taskENTER_CRITICAL(&s_lock);
    s_status.sync_stage = stage;
    taskEXIT_CRITICAL(&s_lock);
}

static void cloud_progress(np_ewelink_sync_stage_t stage, void *context)
{
    (void)context;
    set_sync_stage(stage);
}

static bool copy_if_nonempty(char *target, size_t target_size,
                             const char *source, size_t source_size)
{
    if (source[0] == '\0') return false;
    const size_t length = strnlen(source, source_size);
    if (length == 0U || length >= source_size || length >= target_size) return false;
    memcpy(target, source, length + 1U);
    return true;
}

static void merge_cloud_device(np_ewelink_device_t *target,
                               const np_ewelink_device_t *cloud)
{
    const bool is_existing = target->device_id[0] != '\0';
    uint8_t local_settings[NP_EWELINK_LOCAL_SETTINGS_BYTES] = {0};
    if (is_existing) memcpy(local_settings, target->local_settings, sizeof(local_settings));

    memcpy(target->device_id, cloud->device_id, sizeof(target->device_id));
    (void)copy_if_nonempty(target->device_key, sizeof(target->device_key),
                           cloud->device_key, sizeof(cloud->device_key));
    (void)copy_if_nonempty(target->api_key, sizeof(target->api_key),
                           cloud->api_key, sizeof(cloud->api_key));
    (void)copy_if_nonempty(target->name, sizeof(target->name), cloud->name, sizeof(cloud->name));
    (void)copy_if_nonempty(target->brand_name, sizeof(target->brand_name),
                           cloud->brand_name, sizeof(cloud->brand_name));
    (void)copy_if_nonempty(target->product_model, sizeof(target->product_model),
                           cloud->product_model, sizeof(cloud->product_model));
    (void)copy_if_nonempty(target->internal_model, sizeof(target->internal_model),
                           cloud->internal_model, sizeof(cloud->internal_model));
    (void)copy_if_nonempty(target->sta_mac, sizeof(target->sta_mac),
                           cloud->sta_mac, sizeof(cloud->sta_mac));
    if (cloud->uiid != 0) target->uiid = cloud->uiid;
    target->online = cloud->online;
    target->present_last_sync = true;
    if (cloud->model != NP_EWELINK_UNKNOWN) {
        target->model = cloud->model;
        target->channel_count = cloud->channel_count;
    }
    for (uint8_t channel = 0U; channel < cloud->channel_count; ++channel) {
        char fallback[32] = {0};
        (void)snprintf(fallback, sizeof(fallback), "Canal %u", (unsigned int)(channel + 1U));
        if (!is_existing || strcmp(cloud->channel_names[channel], fallback) != 0 ||
            target->channel_names[channel][0] == '\0') {
            (void)copy_if_nonempty(target->channel_names[channel],
                                   sizeof(target->channel_names[channel]),
                                   cloud->channel_names[channel],
                                   sizeof(cloud->channel_names[channel]));
        }
    }
    (void)copy_if_nonempty(target->params_json, sizeof(target->params_json),
                           cloud->params_json, sizeof(cloud->params_json));
    memcpy(target->local_settings, local_settings, sizeof(local_settings));
    secure_zero(local_settings, sizeof(local_settings));
}

static esp_err_t reconcile_inventory(const np_ewelink_inventory_t *current,
                                     const np_ewelink_inventory_t *cloud,
                                     np_ewelink_inventory_t *out_inventory)
{
    if (current == NULL || cloud == NULL || out_inventory == NULL ||
        current->count > NP_EWELINK_MAX_DEVICES || cloud->count > NP_EWELINK_MAX_DEVICES)
        return ESP_ERR_INVALID_ARG;
    *out_inventory = *current;
    for (uint8_t index = 0U; index < out_inventory->count; ++index)
        out_inventory->devices[index].present_last_sync = false;
    for (uint8_t cloud_index = 0U; cloud_index < cloud->count; ++cloud_index) {
        const np_ewelink_device_t *const item = &cloud->devices[cloud_index];
        int existing_index = -1;
        for (uint8_t local_index = 0U; local_index < out_inventory->count; ++local_index) {
            if (strcmp(out_inventory->devices[local_index].device_id, item->device_id) == 0) {
                existing_index = (int)local_index;
                break;
            }
        }
        if (existing_index < 0) {
            if (out_inventory->count >= NP_EWELINK_MAX_DEVICES) return ESP_ERR_INVALID_SIZE;
            existing_index = (int)out_inventory->count++;
            memset(&out_inventory->devices[existing_index], 0,
                   sizeof(out_inventory->devices[existing_index]));
        }
        merge_cloud_device(&out_inventory->devices[existing_index], item);
    }
    return ESP_OK;
}

esp_err_t np_ewelink_init(void)
{
    if (s_initialized) return ESP_ERR_INVALID_STATE;
    s_inventory_mutex = xSemaphoreCreateMutex();
    if (s_inventory_mutex == NULL) return ESP_ERR_NO_MEM;
    const esp_err_t load_result = np_ewelink_storage_load(&s_inventory);
    if (load_result != ESP_OK && load_result != ESP_ERR_NOT_FOUND) {
        secure_zero(&s_inventory, sizeof(s_inventory));
        vSemaphoreDelete(s_inventory_mutex);
        s_inventory_mutex = NULL;
        return load_result;
    }
    if (load_result == ESP_ERR_NOT_FOUND) secure_zero(&s_inventory, sizeof(s_inventory));
    s_initialized = true;
    s_status.ready = true;
    s_status.device_count = s_inventory.count;
    s_status.generation = 0U;
    s_status.last_result = load_result == ESP_OK ? ESP_OK : ESP_ERR_NOT_FOUND;
    s_status.sync_stage = NP_EWELINK_SYNC_IDLE;
    return ESP_OK;
}

esp_err_t np_ewelink_load_inventory(void)
{
    if (!s_initialized || s_inventory_mutex == NULL) return ESP_ERR_INVALID_STATE;
    np_ewelink_inventory_t loaded = {0};
    esp_err_t result = np_ewelink_storage_load(&loaded);
    if (result == ESP_ERR_NOT_FOUND) {
        secure_zero(&loaded, sizeof(loaded));
        result = ESP_OK;
    }
    if (result == ESP_OK) {
        if (xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            result = ESP_ERR_TIMEOUT;
        } else {
            secure_zero(&s_inventory, sizeof(s_inventory));
            s_inventory = loaded;
            xSemaphoreGive(s_inventory_mutex);
            taskENTER_CRITICAL(&s_lock);
            s_status.device_count = loaded.count;
            taskEXIT_CRITICAL(&s_lock);
        }
    }
    secure_zero(&loaded, sizeof(loaded));
    return result;
}

esp_err_t np_ewelink_get_inventory(np_ewelink_inventory_t *out_inventory)
{
    if (out_inventory == NULL) return ESP_ERR_INVALID_ARG;
    secure_zero(out_inventory, sizeof(*out_inventory));
    if (!s_initialized || s_inventory_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    *out_inventory = s_inventory;
    xSemaphoreGive(s_inventory_mutex);
    return ESP_OK;
}

esp_err_t np_ewelink_get_public_inventory(
    np_ewelink_public_inventory_t *out_inventory)
{
    if (out_inventory == NULL) return ESP_ERR_INVALID_ARG;
    memset(out_inventory, 0, sizeof(*out_inventory));
    if (!s_initialized || s_inventory_mutex == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    out_inventory->count = s_inventory.count;
    for (uint8_t index = 0U; index < s_inventory.count; ++index) {
        const np_ewelink_device_t *const source = &s_inventory.devices[index];
        np_ewelink_public_device_t *const target = &out_inventory->devices[index];
        memcpy(target->device_id, source->device_id, sizeof(target->device_id));
        memcpy(target->name, source->name, sizeof(target->name));
        memcpy(target->product_model, source->product_model,
               sizeof(target->product_model));
        target->channel_count = source->channel_count;
        memcpy(target->channel_names, source->channel_names,
               sizeof(target->channel_names));
        target->present_last_sync = source->present_last_sync;
    }
    xSemaphoreGive(s_inventory_mutex);
    return ESP_OK;
}

esp_err_t np_ewelink_sync_begin(const char *username, const char *password)
{
    if (!s_initialized || username == NULL || password == NULL) return ESP_ERR_INVALID_ARG;
    /* Match the validated Python POC's input(...).strip() behavior. */
    const size_t raw_user_length = strnlen(username, sizeof(s_pending_username));
    const size_t pass_length = strnlen(password, sizeof(s_pending_password));
    if (raw_user_length >= sizeof(s_pending_username) ||
        pass_length == 0U || pass_length >= sizeof(s_pending_password)) return ESP_ERR_INVALID_SIZE;

    size_t user_start = 0U;
    size_t user_end = raw_user_length;
    while (user_start < user_end && (unsigned char)username[user_start] <= 0x20U) ++user_start;
    while (user_end > user_start && (unsigned char)username[user_end - 1U] <= 0x20U) --user_end;
    const size_t user_length = user_end - user_start;
    if (user_length == 0U) return ESP_ERR_INVALID_ARG;

    taskENTER_CRITICAL(&s_lock);
    if (s_status.busy || s_credentials_pending) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    memcpy(s_pending_username, username + user_start, user_length);
    s_pending_username[user_length] = '\0';
    memcpy(s_pending_password, password, pass_length + 1U);
    s_credentials_pending = true;
    s_status.busy = true;
    s_status.last_result = ESP_ERR_TIMEOUT;
    s_status.sync_stage = NP_EWELINK_SYNC_AUTHENTICATING;
    taskEXIT_CRITICAL(&s_lock);

    const esp_err_t result = network_validation_service_request_ewelink_sync();
    if (result != ESP_OK) {
        taskENTER_CRITICAL(&s_lock);
        secure_zero(s_pending_username, sizeof(s_pending_username));
        secure_zero(s_pending_password, sizeof(s_pending_password));
        s_credentials_pending = false;
        s_status.busy = false;
        s_status.last_result = result;
        s_status.sync_stage = NP_EWELINK_SYNC_FAILED;
        taskEXIT_CRITICAL(&s_lock);
    }
    return result;
}

void np_ewelink_get_status(np_ewelink_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}

esp_err_t np_ewelink_service_process_sync(void)
{
    char username[EWELINK_USERNAME_BYTES] = {0};
    char password[EWELINK_PASSWORD_BYTES] = {0};
    ewelink_sync_workspace_t *workspace = NULL;
    const char *stage = "take_credentials";
    taskENTER_CRITICAL(&s_lock);
    if (!s_credentials_pending || !s_status.busy) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(username, s_pending_username, sizeof(username));
    memcpy(password, s_pending_password, sizeof(password));
    secure_zero(s_pending_username, sizeof(s_pending_username));
    secure_zero(s_pending_password, sizeof(s_pending_password));
    s_credentials_pending = false;
    taskEXIT_CRITICAL(&s_lock);

    stage = "allocate_workspace";
    workspace = heap_caps_calloc(1U, sizeof(*workspace),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    esp_err_t result = workspace == NULL ? ESP_ERR_NO_MEM : ESP_OK;
    if (result == ESP_OK) stage = "snapshot_inventory";
    if (result == ESP_OK && xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        result = ESP_ERR_TIMEOUT;
    } else if (result == ESP_OK) {
        workspace->local_snapshot = s_inventory;
        xSemaphoreGive(s_inventory_mutex);
    }
    if (result == ESP_OK) {
        stage = "cloud_login_fetch";
        result = np_ewelink_cloud_login_and_fetch(username, password,
            cloud_progress, NULL, &workspace->cloud);
    }
    if (result == ESP_OK) {
        stage = "reconcile_inventory";
        set_sync_stage(NP_EWELINK_SYNC_RECONCILING);
        result = reconcile_inventory(&workspace->local_snapshot,
            &workspace->cloud, &workspace->reconciled);
    }
    if (result == ESP_OK) {
        stage = "save_inventory";
        set_sync_stage(NP_EWELINK_SYNC_SAVING);
        result = np_ewelink_storage_save(&workspace->reconciled);
    }
    if (result == ESP_OK) {
        stage = "publish_inventory";
        if (xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            secure_zero(&s_inventory, sizeof(s_inventory));
            s_inventory = workspace->reconciled;
            xSemaphoreGive(s_inventory_mutex);
        } else {
            result = ESP_ERR_TIMEOUT;
        }
    }
    if (result == ESP_OK) {
        taskENTER_CRITICAL(&s_lock);
        s_status.device_count = s_inventory.count;
        ++s_status.generation;
        ++s_status.completed_syncs;
        taskEXIT_CRITICAL(&s_lock);
        ESP_LOGI(TAG, "inventory reconciliation complete devices=%u",
                 (unsigned int)s_inventory.count);
    }
    secure_zero(username, sizeof(username));
    secure_zero(password, sizeof(password));
    if (workspace != NULL) {
        secure_zero(workspace, sizeof(*workspace));
        heap_caps_free(workspace);
    }
    taskENTER_CRITICAL(&s_lock);
    s_status.last_result = result;
    s_status.busy = false;
    s_status.sync_stage = result == ESP_OK
        ? NP_EWELINK_SYNC_COMPLETE : NP_EWELINK_SYNC_FAILED;
    taskEXIT_CRITICAL(&s_lock);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "sync failed stage=%s result=%s psram_free=%u psram_largest=%u internal_free=%u internal_largest=%u",
                 stage, esp_err_to_name(result),
                 (unsigned int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
                 (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    return result;
}

esp_err_t np_ewelink_copy_device_key(const char *device_id,
                                     char *out_key, size_t out_key_size)
{
    if (device_id == NULL || out_key == NULL || out_key_size == 0U)
        return ESP_ERR_INVALID_ARG;
    out_key[0] = '\0';
    if (s_inventory_mutex == NULL ||
        xSemaphoreTake(s_inventory_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    esp_err_t result = ESP_ERR_NOT_FOUND;
    for (uint8_t index = 0U; index < s_inventory.count; ++index) {
        const np_ewelink_device_t *const device = &s_inventory.devices[index];
        if (strcmp(device->device_id, device_id) != 0) continue;
        const size_t length = strnlen(device->device_key, sizeof(device->device_key));
        if (length == 0U || length >= sizeof(device->device_key)) break;
        if (length >= out_key_size) { result = ESP_ERR_INVALID_SIZE; break; }
        memcpy(out_key, device->device_key, length + 1U);
        result = ESP_OK;
        break;
    }
    xSemaphoreGive(s_inventory_mutex);
    return result;
}
