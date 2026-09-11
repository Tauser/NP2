/*
 * Controlled Phase 2 persistence probe.
 *
 * Flash access is serialized here because this board's flash cannot suspend
 * erase/write traffic safely while the MIPI-DSI pipeline reads PSRAM. This
 * diagnostic intentionally exercises one small NVS commit; larger writes,
 * filesystem GC, staging and maintenance mode are separate future requests.
 */
#include "flash_coordinator.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define FLASH_COORDINATOR_QUEUE_LENGTH 1
#define FLASH_COORDINATOR_TASK_STACK_BYTES 6144
#define FLASH_COORDINATOR_TASK_PRIORITY 2
#define FLASH_COORDINATOR_NVS_MIN_INTERVAL_US (60LL * 1000LL * 1000LL)
#define FLASH_COORDINATOR_NVS_COMPACTION_WRITES 64U
#define FLASH_COORDINATOR_NVS_COMPACTION_BLOB_BYTES 512U
#define FLASH_COORDINATOR_LITTLEFS_WRITES 64U
#define FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES 4096U
#define FLASH_COORDINATOR_LITTLEFS_VERIFY_CHUNK_BYTES 256U
#define FLASH_COORDINATOR_CACHE_MAGIC UINT32_C(0x4e503243)
#define FLASH_COORDINATOR_CACHE_SCHEMA_VERSION 1U

static const char *const TAG = "flash_coord";
static const char *const NVS_PARTITION = "nvs";
static const char *const NVS_NAMESPACE = "np2_diag";
static const char *const CONFIG_NAMESPACE = "np2_config";
static const char *const NVS_KEY = "probe_seq";
static const char *const CONFIG_SLOT_ZERO_KEY = "cfg0";
static const char *const CONFIG_SLOT_ONE_KEY = "cfg1";
static const char *const LITTLEFS_PARTITION = "storage";
static const char *const LITTLEFS_BASE_PATH = "/lfsdiag";
static const char *const LITTLEFS_TMP_PATH = "/lfsdiag/cache.tmp";
static const char *const LITTLEFS_GENERATION_ZERO_PATH = "/lfsdiag/cache.0";
static const char *const LITTLEFS_GENERATION_ONE_PATH = "/lfsdiag/cache.1";

typedef struct {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t header_size;
    uint32_t generation;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t header_crc32;
} cache_record_header_t;

typedef struct {
    cache_record_header_t header;
    uint32_t value;
} config_record_t;

typedef enum {
    FLASH_REQUEST_NVS_PROBE,
    FLASH_REQUEST_NVS_COMPACTION_PROBE,
    FLASH_REQUEST_LITTLEFS_PROBE,
    FLASH_REQUEST_LITTLEFS_FORMAT,
    FLASH_REQUEST_CACHE_CORRUPT_NEWEST,
    FLASH_REQUEST_CONFIG_JOURNAL_WRITE,
    FLASH_REQUEST_CONFIG_CORRUPT_NEWEST,
} flash_request_kind_t;

typedef struct {
    flash_request_kind_t kind;
    uint32_t sequence;
} flash_request_t;

static QueueHandle_t s_request_queue;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static flash_coordinator_status_t s_status;
static int64_t s_last_success_us;

static void refresh_cache_status(void);
static void refresh_config_status(void);

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint32_t bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static esp_err_t mount_littlefs(void)
{
    if (esp_littlefs_mounted(LITTLEFS_PARTITION)) {
        return ESP_OK;
    }

    const esp_vfs_littlefs_conf_t config = {
        .base_path = LITTLEFS_BASE_PATH,
        .partition_label = LITTLEFS_PARTITION,
        .format_if_mount_failed = false,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    return esp_vfs_littlefs_register(&config);
}

static esp_err_t format_and_mount_littlefs(void)
{
    if (esp_littlefs_mounted(LITTLEFS_PARTITION)) {
        ESP_RETURN_ON_ERROR(esp_vfs_littlefs_unregister(LITTLEFS_PARTITION), TAG,
                            "LittleFS unmount before format failed");
    }
    ESP_RETURN_ON_ERROR(esp_littlefs_format(LITTLEFS_PARTITION), TAG,
                        "LittleFS format failed");
    const esp_err_t result = mount_littlefs();
    if (result == ESP_OK) {
        refresh_cache_status();
    }
    return result;
}

static void set_busy(bool busy, bool pending)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.busy = busy;
    s_status.pending = pending;
    portEXIT_CRITICAL(&s_status_lock);
}

static void complete_request(uint32_t sequence, esp_err_t result, uint32_t duration_ms,
                             uint32_t batch_writes, uint32_t free_entries_before,
                             uint32_t free_entries_after, uint32_t littlefs_writes,
                             uint32_t littlefs_verified_bytes, bool littlefs_format,
                             bool littlefs_ready)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.busy = false;
    s_status.pending = false;
    s_status.last_sequence = sequence;
    s_status.last_duration_ms = duration_ms;
    s_status.last_batch_writes = batch_writes;
    s_status.last_free_entries_before = free_entries_before;
    s_status.last_free_entries_after = free_entries_after;
    s_status.last_littlefs_writes = littlefs_writes;
    s_status.last_littlefs_verified_bytes = littlefs_verified_bytes;
    s_status.last_littlefs_format = littlefs_format;
    s_status.littlefs_ready = littlefs_ready;
    s_status.last_result = result;
    if (result == ESP_OK) {
        s_status.completed_count++;
    } else {
        s_status.rejected_count++;
    }
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t commit_nvs_compaction_probe(uint32_t sequence, uint32_t *out_writes,
                                              uint32_t *out_free_before,
                                              uint32_t *out_free_after)
{
    nvs_stats_t stats = {0};
    ESP_RETURN_ON_ERROR(nvs_get_stats(NVS_PARTITION, &stats), TAG, "NVS stats before failed");
    *out_free_before = stats.free_entries;

    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(NVS_PARTITION, NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    static uint8_t payload[FLASH_COORDINATOR_NVS_COMPACTION_BLOB_BYTES];
    for (uint32_t write_index = 0; write_index < FLASH_COORDINATOR_NVS_COMPACTION_WRITES;
         ++write_index) {
        for (size_t byte_index = 0; byte_index < sizeof(payload); ++byte_index) {
            payload[byte_index] = (uint8_t)(sequence + write_index + byte_index);
        }
        err = nvs_set_blob(handle, "gc_probe", payload, sizeof(payload));
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        if (err != ESP_OK) {
            break;
        }
        (*out_writes)++;
    }
    nvs_close(handle);

    if (nvs_get_stats(NVS_PARTITION, &stats) == ESP_OK) {
        *out_free_after = stats.free_entries;
    }
    return err;
}

static esp_err_t commit_nvs_probe(uint32_t sequence)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(NVS_PARTITION, NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u32(handle, NVS_KEY, sequence);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t write_all(int fd, const uint8_t *buffer, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = write(fd, buffer + offset, size - offset);
        if (written <= 0) {
            ESP_LOGE(TAG, "LittleFS write failed: errno=%d", errno);
            return ESP_FAIL;
        }
        offset += (size_t)written;
    }
    return ESP_OK;
}

static bool cache_header_is_valid(const cache_record_header_t *header)
{
    if (header->magic != FLASH_COORDINATOR_CACHE_MAGIC ||
        header->schema_version != FLASH_COORDINATOR_CACHE_SCHEMA_VERSION ||
        header->header_size != sizeof(*header) ||
        header->payload_size > FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES) {
        return false;
    }
    return header->header_crc32 ==
           crc32((const uint8_t *)header, offsetof(cache_record_header_t, header_crc32));
}

static esp_err_t validate_cache_generation(const char *path, cache_record_header_t *out_header)
{
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    cache_record_header_t header = {0};
    esp_err_t result = ESP_OK;
    if (read(fd, &header, sizeof(header)) != (ssize_t)sizeof(header) ||
        !cache_header_is_valid(&header)) {
        result = ESP_ERR_INVALID_CRC;
    }

    uint32_t payload_crc = UINT32_MAX;
    uint32_t remaining = header.payload_size;
    uint8_t buffer[FLASH_COORDINATOR_LITTLEFS_VERIFY_CHUNK_BYTES];
    while (result == ESP_OK && remaining > 0U) {
        const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (read(fd, buffer, chunk) != (ssize_t)chunk) {
            result = ESP_ERR_INVALID_SIZE;
            break;
        }
        for (size_t index = 0; index < chunk; ++index) {
            payload_crc ^= buffer[index];
            for (uint32_t bit = 0; bit < 8U; ++bit) {
                payload_crc = (payload_crc >> 1U) ^
                              (UINT32_C(0xedb88320) & (0U - (payload_crc & 1U)));
            }
        }
        remaining -= chunk;
    }
    if (result == ESP_OK && ~payload_crc != header.payload_crc32) {
        result = ESP_ERR_INVALID_CRC;
    }
    if (result == ESP_OK && lseek(fd, 0, SEEK_END) !=
                            (off_t)(sizeof(header) + header.payload_size)) {
        result = ESP_ERR_INVALID_SIZE;
    }
    if (close(fd) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    if (result == ESP_OK && out_header != NULL) {
        *out_header = header;
    }
    return result;
}

static esp_err_t select_latest_cache(cache_record_header_t *out_header)
{
    cache_record_header_t generation_zero = {0};
    cache_record_header_t generation_one = {0};
    const esp_err_t zero_result =
        validate_cache_generation(LITTLEFS_GENERATION_ZERO_PATH, &generation_zero);
    const esp_err_t one_result =
        validate_cache_generation(LITTLEFS_GENERATION_ONE_PATH, &generation_one);
    if (zero_result != ESP_OK && one_result != ESP_OK) {
        return zero_result == ESP_ERR_NOT_FOUND && one_result == ESP_ERR_NOT_FOUND
                   ? ESP_ERR_NOT_FOUND
                   : ESP_ERR_INVALID_CRC;
    }
    if (out_header != NULL) {
        *out_header = (one_result == ESP_OK &&
                       (zero_result != ESP_OK || generation_one.generation > generation_zero.generation))
                          ? generation_one
                          : generation_zero;
    }
    return ESP_OK;
}

static void refresh_cache_status(void)
{
    cache_record_header_t selected = {0};
    const esp_err_t result = select_latest_cache(&selected);
    portENTER_CRITICAL(&s_status_lock);
    s_status.cache_result = result;
    s_status.cache_valid = result == ESP_OK;
    s_status.cache_generation = result == ESP_OK ? selected.generation : 0U;
    s_status.cache_schema_version = result == ESP_OK ? selected.schema_version : 0U;
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_cache_generation(uint32_t generation, const uint8_t *payload,
                                        size_t payload_size)
{
    if (payload == NULL || payload_size > FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    cache_record_header_t header = {
        .magic = FLASH_COORDINATOR_CACHE_MAGIC,
        .schema_version = FLASH_COORDINATOR_CACHE_SCHEMA_VERSION,
        .header_size = sizeof(cache_record_header_t),
        .generation = generation,
        .payload_size = payload_size,
        .payload_crc32 = crc32(payload, payload_size),
    };
    header.header_crc32 =
        crc32((const uint8_t *)&header, offsetof(cache_record_header_t, header_crc32));

    const int fd = open(LITTLEFS_TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        ESP_LOGE(TAG, "LittleFS cache temp open failed: errno=%d", errno);
        return ESP_FAIL;
    }
    esp_err_t result = write_all(fd, (const uint8_t *)&header, sizeof(header));
    if (result == ESP_OK) {
        result = write_all(fd, payload, payload_size);
    }
    if (result == ESP_OK && fsync(fd) != 0) {
        result = ESP_FAIL;
    }
    if (close(fd) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    if (result != ESP_OK) {
        return result;
    }

    const char *const destination = (generation & 1U) == 0U
                                        ? LITTLEFS_GENERATION_ZERO_PATH
                                        : LITTLEFS_GENERATION_ONE_PATH;
    if (rename(LITTLEFS_TMP_PATH, destination) != 0) {
        ESP_LOGE(TAG, "LittleFS cache rename failed: errno=%d", errno);
        return ESP_FAIL;
    }
    cache_record_header_t verified = {0};
    result = validate_cache_generation(destination, &verified);
    return result == ESP_OK && verified.generation == generation ? ESP_OK :
           (result == ESP_OK ? ESP_ERR_INVALID_RESPONSE : result);
}

static esp_err_t corrupt_newest_cache_generation(void)
{
    cache_record_header_t newest = {0};
    ESP_RETURN_ON_ERROR(select_latest_cache(&newest), TAG, "No valid cache generation");
    const char *const newest_path = (newest.generation & 1U) == 0U
                                        ? LITTLEFS_GENERATION_ZERO_PATH
                                        : LITTLEFS_GENERATION_ONE_PATH;
    const char *const older_path = (newest.generation & 1U) == 0U
                                       ? LITTLEFS_GENERATION_ONE_PATH
                                       : LITTLEFS_GENERATION_ZERO_PATH;
    cache_record_header_t older = {0};
    ESP_RETURN_ON_ERROR(validate_cache_generation(older_path, &older), TAG,
                        "No valid fallback cache generation");

    const int fd = open(newest_path, O_RDWR);
    if (fd < 0) {
        return ESP_FAIL;
    }
    esp_err_t result = ESP_OK;
    uint8_t value = 0;
    if (lseek(fd, sizeof(cache_record_header_t), SEEK_SET) < 0 || read(fd, &value, 1U) != 1) {
        result = ESP_FAIL;
    }
    value ^= UINT8_C(0xa5);
    if (result == ESP_OK &&
        (lseek(fd, sizeof(cache_record_header_t), SEEK_SET) < 0 || write(fd, &value, 1U) != 1)) {
        result = ESP_FAIL;
    }
    if (result == ESP_OK && fsync(fd) != 0) {
        result = ESP_FAIL;
    }
    if (close(fd) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    if (result != ESP_OK) {
        return result;
    }

    refresh_cache_status();
    cache_record_header_t selected = {0};
    result = select_latest_cache(&selected);
    return result == ESP_OK && selected.generation == older.generation ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_config_slot(const char *key, config_record_t *out_record)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, CONFIG_NAMESPACE,
                                               NVS_READONLY, &handle);
    if (result != ESP_OK) {
        return result;
    }
    config_record_t record = {0};
    size_t record_size = sizeof(record);
    result = nvs_get_blob(handle, key, &record, &record_size);
    nvs_close(handle);
    if (result != ESP_OK) {
        return result;
    }
    if (record_size != sizeof(record) || !cache_header_is_valid(&record.header) ||
        record.header.payload_size != sizeof(record.value) ||
        record.header.payload_crc32 != crc32((const uint8_t *)&record.value, sizeof(record.value))) {
        return ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) {
        *out_record = record;
    }
    return ESP_OK;
}

static esp_err_t select_latest_config(config_record_t *out_record)
{
    config_record_t slot_zero = {0};
    config_record_t slot_one = {0};
    const esp_err_t zero_result = read_config_slot(CONFIG_SLOT_ZERO_KEY, &slot_zero);
    const esp_err_t one_result = read_config_slot(CONFIG_SLOT_ONE_KEY, &slot_one);
    if (zero_result != ESP_OK && one_result != ESP_OK) {
        return zero_result == ESP_ERR_NVS_NOT_FOUND && one_result == ESP_ERR_NVS_NOT_FOUND
                   ? ESP_ERR_NOT_FOUND
                   : ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) {
        *out_record = (one_result == ESP_OK &&
                       (zero_result != ESP_OK || slot_one.header.generation > slot_zero.header.generation))
                          ? slot_one
                          : slot_zero;
    }
    return ESP_OK;
}

static void refresh_config_status(void)
{
    config_record_t selected = {0};
    const esp_err_t result = select_latest_config(&selected);
    portENTER_CRITICAL(&s_status_lock);
    s_status.config_result = result;
    s_status.config_valid = result == ESP_OK;
    s_status.config_generation = result == ESP_OK ? selected.header.generation : 0U;
    s_status.config_schema_version = result == ESP_OK ? selected.header.schema_version : 0U;
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_config_journal(void)
{
    config_record_t latest = {0};
    const esp_err_t latest_result = select_latest_config(&latest);
    const uint32_t generation = latest_result == ESP_OK ? latest.header.generation + 1U : 1U;
    config_record_t record = {
        .header = {
            .magic = FLASH_COORDINATOR_CACHE_MAGIC,
            .schema_version = FLASH_COORDINATOR_CACHE_SCHEMA_VERSION,
            .header_size = sizeof(cache_record_header_t),
            .generation = generation,
            .payload_size = sizeof(uint32_t),
        },
        .value = generation,
    };
    record.header.payload_crc32 = crc32((const uint8_t *)&record.value, sizeof(record.value));
    record.header.header_crc32 = crc32((const uint8_t *)&record.header,
                                      offsetof(cache_record_header_t, header_crc32));

    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, CONFIG_NAMESPACE,
                                                NVS_READWRITE, &handle), TAG,
                        "Config journal open failed");
    const char *const target = (generation & 1U) == 0U ? CONFIG_SLOT_ZERO_KEY : CONFIG_SLOT_ONE_KEY;
    esp_err_t result = nvs_set_blob(handle, target, &record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result == ESP_OK) {
        refresh_config_status();
        config_record_t selected = {0};
        result = select_latest_config(&selected);
        if (result == ESP_OK && selected.header.generation != generation) {
            result = ESP_FAIL;
        }
    }
    return result;
}

static esp_err_t corrupt_newest_config_generation(void)
{
    config_record_t newest = {0};
    ESP_RETURN_ON_ERROR(select_latest_config(&newest), TAG, "No configuration generation to corrupt");
    if (newest.header.generation < 2U) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *const older_key = (newest.header.generation & 1U) == 0U
                                     ? CONFIG_SLOT_ONE_KEY
                                     : CONFIG_SLOT_ZERO_KEY;
    config_record_t older = {0};
    ESP_RETURN_ON_ERROR(read_config_slot(older_key, &older), TAG,
                        "No valid prior configuration generation");

    const char *const newest_key = (newest.header.generation & 1U) == 0U
                                      ? CONFIG_SLOT_ZERO_KEY
                                      : CONFIG_SLOT_ONE_KEY;
    newest.value ^= UINT32_C(0xa5a5a5a5);
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, CONFIG_NAMESPACE,
                                                NVS_READWRITE, &handle), TAG,
                        "Config corruption open failed");
    esp_err_t result = nvs_set_blob(handle, newest_key, &newest, sizeof(newest));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result != ESP_OK) {
        return result;
    }

    refresh_config_status();
    config_record_t selected = {0};
    result = select_latest_config(&selected);
    return result == ESP_OK && selected.header.generation == older.header.generation ? ESP_OK : ESP_FAIL;
}

static esp_err_t commit_littlefs_probe(uint32_t sequence, uint32_t *out_writes,
                                       uint32_t *out_verified_bytes)
{
    if (!esp_littlefs_mounted(LITTLEFS_PARTITION)) {
        return ESP_ERR_INVALID_STATE;
    }

    cache_record_header_t latest = {0};
    const esp_err_t latest_result = select_latest_cache(&latest);
    uint32_t generation = latest_result == ESP_OK ? latest.generation + 1U : 1U;
    static uint8_t payload[FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES];
    for (uint32_t write_index = 0; write_index < FLASH_COORDINATOR_LITTLEFS_WRITES;
         ++write_index) {
        for (size_t byte_index = 0; byte_index < sizeof(payload); ++byte_index) {
            payload[byte_index] = (uint8_t)(sequence + write_index + byte_index);
        }

        const esp_err_t result = write_cache_generation(generation, payload, sizeof(payload));
        if (result != ESP_OK) {
            return result;
        }

        (*out_writes)++;
        *out_verified_bytes += sizeof(payload);
        ++generation;
    }
    refresh_cache_status();
    return ESP_OK;
}

static void flash_worker_task(void *arg)
{
    (void)arg;

    const esp_err_t init_result = nvs_flash_init_partition(NVS_PARTITION);
    const esp_err_t littlefs_init_result = mount_littlefs();
    portENTER_CRITICAL(&s_status_lock);
    s_status.init_result = init_result;
    s_status.ready = (init_result == ESP_OK);
    s_status.littlefs_init_result = littlefs_init_result;
    s_status.littlefs_ready = (littlefs_init_result == ESP_OK);
    s_status.cache_result = littlefs_init_result;
    s_status.config_result = init_result;
    s_status.last_result = init_result;
    portEXIT_CRITICAL(&s_status_lock);

    if (init_result != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s; refusing all writes", esp_err_to_name(init_result));
        vTaskDelete(NULL);
        return;
    }
    refresh_config_status();
    if (littlefs_init_result != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS unavailable until explicit format: %s",
                 esp_err_to_name(littlefs_init_result));
    } else {
        refresh_cache_status();
    }

    flash_request_t request;
    while (xQueueReceive(s_request_queue, &request, portMAX_DELAY) == pdTRUE) {
        set_busy(true, false);
        const int64_t started_us = esp_timer_get_time();
        uint32_t batch_writes = 0;
        uint32_t free_entries_before = 0;
        uint32_t free_entries_after = 0;
        uint32_t littlefs_writes = 0;
        uint32_t littlefs_verified_bytes = 0;
        bool littlefs_format = false;
        esp_err_t result;
        switch (request.kind) {
        case FLASH_REQUEST_NVS_COMPACTION_PROBE:
            result = commit_nvs_compaction_probe(request.sequence, &batch_writes,
                                                  &free_entries_before, &free_entries_after);
            break;
        case FLASH_REQUEST_LITTLEFS_PROBE:
            result = commit_littlefs_probe(request.sequence, &littlefs_writes,
                                           &littlefs_verified_bytes);
            break;
        case FLASH_REQUEST_LITTLEFS_FORMAT:
            littlefs_format = true;
            result = format_and_mount_littlefs();
            break;
        case FLASH_REQUEST_CACHE_CORRUPT_NEWEST:
            result = corrupt_newest_cache_generation();
            break;
        case FLASH_REQUEST_CONFIG_JOURNAL_WRITE:
            result = write_config_journal();
            break;
        case FLASH_REQUEST_CONFIG_CORRUPT_NEWEST:
            result = corrupt_newest_config_generation();
            break;
        case FLASH_REQUEST_NVS_PROBE:
        default:
            result = commit_nvs_probe(request.sequence);
            break;
        }
        const uint32_t duration_ms = (uint32_t)((esp_timer_get_time() - started_us) / 1000LL);
        if (esp_littlefs_mounted(LITTLEFS_PARTITION)) {
            refresh_cache_status();
        }
        if (result == ESP_OK) {
            portENTER_CRITICAL(&s_status_lock);
            s_last_success_us = esp_timer_get_time();
            portEXIT_CRITICAL(&s_status_lock);
            if (request.kind == FLASH_REQUEST_LITTLEFS_PROBE) {
                ESP_LOGI(TAG, "LittleFS batch %lu completed in %lums; writes=%lu verified=%luB",
                         (unsigned long)request.sequence, (unsigned long)duration_ms,
                         (unsigned long)littlefs_writes,
                         (unsigned long)littlefs_verified_bytes);
            } else if (request.kind == FLASH_REQUEST_LITTLEFS_FORMAT) {
                ESP_LOGI(TAG, "LittleFS explicit format %lu completed in %lums",
                         (unsigned long)request.sequence, (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CACHE_CORRUPT_NEWEST) {
                ESP_LOGW(TAG, "cache newest-generation corruption %lu completed in %lums",
                         (unsigned long)request.sequence, (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CONFIG_JOURNAL_WRITE) {
                flash_coordinator_status_t config_status = {0};
                flash_coordinator_get_status(&config_status);
                ESP_LOGI(TAG, "configuration journal generation %lu completed in %lums",
                         (unsigned long)config_status.config_generation,
                         (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CONFIG_CORRUPT_NEWEST) {
                ESP_LOGW(TAG, "configuration newest-generation corruption %lu completed in %lums",
                         (unsigned long)request.sequence, (unsigned long)duration_ms);
            } else {
                ESP_LOGI(TAG,
                         "diagnostic NVS %s %lu completed in %lums; writes=%lu free_entries=%lu>%lu",
                         request.kind == FLASH_REQUEST_NVS_COMPACTION_PROBE ? "batch" : "commit",
                         (unsigned long)request.sequence, (unsigned long)duration_ms,
                         (unsigned long)batch_writes, (unsigned long)free_entries_before,
                         (unsigned long)free_entries_after);
            }
        } else {
            ESP_LOGE(TAG, "diagnostic NVS commit failed: %s", esp_err_to_name(result));
        }
        complete_request(request.sequence, result, duration_ms, batch_writes,
                         free_entries_before, free_entries_after, littlefs_writes,
                         littlefs_verified_bytes, littlefs_format,
                         esp_littlefs_mounted(LITTLEFS_PARTITION));
    }
}

esp_err_t flash_coordinator_start(void)
{
    if (s_request_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_request_queue = xQueueCreate(FLASH_COORDINATOR_QUEUE_LENGTH, sizeof(flash_request_t));
    ESP_RETURN_ON_FALSE(s_request_queue != NULL, ESP_ERR_NO_MEM, TAG, "Flash queue allocation failed");

    portENTER_CRITICAL(&s_status_lock);
    s_status.init_result = ESP_ERR_INVALID_STATE;
    s_status.last_result = ESP_ERR_INVALID_STATE;
    portEXIT_CRITICAL(&s_status_lock);

    const BaseType_t task_created = xTaskCreate(flash_worker_task, "flash_worker",
                                                 FLASH_COORDINATOR_TASK_STACK_BYTES, NULL,
                                                 FLASH_COORDINATOR_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        vQueueDelete(s_request_queue);
        s_request_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t flash_coordinator_request_nvs_probe(void)
{
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    flash_coordinator_status_t status;
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }

    const int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_status_lock);
    const int64_t last_success_us = s_last_success_us;
    portEXIT_CRITICAL(&s_status_lock);
    if (last_success_us != 0 && now_us - last_success_us < FLASH_COORDINATOR_NVS_MIN_INTERVAL_US) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_INVALID_STATE;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_INVALID_STATE;
    }

    const flash_request_t request = {
        .kind = FLASH_REQUEST_NVS_PROBE,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_nvs_compaction_probe(void)
{
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    flash_coordinator_status_t status;
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }

    const flash_request_t request = {
        .kind = FLASH_REQUEST_NVS_COMPACTION_PROBE,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

static esp_err_t enqueue_littlefs_request(flash_request_kind_t kind)
{
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    flash_coordinator_status_t status;
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }
    if (kind == FLASH_REQUEST_LITTLEFS_PROBE && !status.littlefs_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    const flash_request_t request = {
        .kind = kind,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        s_status.rejected_count++;
        s_status.last_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_littlefs_probe(void)
{
    return enqueue_littlefs_request(FLASH_REQUEST_LITTLEFS_PROBE);
}

esp_err_t flash_coordinator_request_littlefs_format(void)
{
    return enqueue_littlefs_request(FLASH_REQUEST_LITTLEFS_FORMAT);
}

esp_err_t flash_coordinator_request_cache_corrupt_newest(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.cache_valid || status.cache_generation < 2U) {
        return ESP_ERR_INVALID_STATE;
    }
    return enqueue_littlefs_request(FLASH_REQUEST_CACHE_CORRUPT_NEWEST);
}

esp_err_t flash_coordinator_request_config_journal_write(void)
{
    if (s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }
    const flash_request_t request = {
        .kind = FLASH_REQUEST_CONFIG_JOURNAL_WRITE,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_config_corrupt_newest(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.config_valid || status.config_generation < 2U) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_request_queue == NULL || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }
    const flash_request_t request = {
        .kind = FLASH_REQUEST_CONFIG_CORRUPT_NEWEST,
        .sequence = status.last_sequence + 1U,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

void flash_coordinator_get_status(flash_coordinator_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    portENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    portEXIT_CRITICAL(&s_status_lock);
}
