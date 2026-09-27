/*
 * Controlled Phase 2 persistence probe.
 *
 * Flash access is serialized here because this board's flash cannot suspend
 * erase/write traffic safely while the MIPI-DSI pipeline reads PSRAM. This
 * diagnostic intentionally exercises one small NVS commit; larger writes,
 * filesystem GC, staging and maintenance mode are separate future requests.
 */
#include "flash_coordinator.h"

#include "cache_record.h"
#include "offline_data_codec.h"
#include "update_journal.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_flash_encrypt.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define FLASH_COORDINATOR_QUEUE_LENGTH 1
#define FLASH_COORDINATOR_TASK_STACK_BYTES 10240
#define FLASH_COORDINATOR_TASK_PRIORITY 2
#define FLASH_COORDINATOR_NVS_MIN_INTERVAL_US (60LL * 1000LL * 1000LL)
#define FLASH_COORDINATOR_NVS_COMPACTION_WRITES 64U
#define FLASH_COORDINATOR_NVS_COMPACTION_BLOB_BYTES 512U
#define FLASH_COORDINATOR_LITTLEFS_WRITES 64U
#define FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES 4096U
#define FLASH_COORDINATOR_LITTLEFS_VERIFY_CHUNK_BYTES 256U
#define FLASH_COORDINATOR_FULL_PROBE_CHUNK_BYTES (16U * 1024U)
#define FLASH_COORDINATOR_FULL_PROBE_TAIL_BYTES \
    (sizeof(cache_record_header_t) + OFFLINE_DATA_ENCODED_SIZE)
#define FLASH_COORDINATOR_FULL_PROBE_MAX_PROPOSALS 60U
#define FLASH_COORDINATOR_FULL_PROBE_PATH_BYTES 48U
#define FLASH_COORDINATOR_POWER_CUT_WINDOW_MS 10000U
#define FLASH_COORDINATOR_OFFLINE_DATA_MIN_INTERVAL_US (30LL * 60LL * 1000LL * 1000LL)
#define FLASH_COORDINATOR_OTA_CHUNK_BYTES 4096U
#define FLASH_COORDINATOR_OTA_WAIT_TIMEOUT_MS 20000U
#define FLASH_COORDINATOR_OTA_WAIT_POLL_MS 10U
#define FLASH_COORDINATOR_WIFI_SSID_BYTES 33U
#define FLASH_COORDINATOR_WIFI_PASSWORD_BYTES 64U

static const char *const TAG = "flash_coord";
static const char *const NVS_PARTITION = "nvs";
static const char *const NVS_NAMESPACE = "np2_diag";
static const char *const CONFIG_NAMESPACE = "np2_config";
static const char *const NVS_KEY = "probe_seq";
static const char *const CONFIG_SLOT_ZERO_KEY = "cfg0";
static const char *const CONFIG_SLOT_ONE_KEY = "cfg1";
static const char *const UPDATE_NAMESPACE = "np2_update";
static const char *const ONBOARDING_NAMESPACE = "np2_onboard";
static const char *const ONBOARDING_SLOT_ZERO_KEY = "onb0";
static const char *const ONBOARDING_SLOT_ONE_KEY = "onb1";
/* NVS namespaces are limited to 15 characters. */
static const char *const NOTIFICATION_NAMESPACE = "np2_notif";
static const char *const NOTIFICATION_SLOT_ZERO_KEY = "ntf0";
static const char *const NOTIFICATION_SLOT_ONE_KEY = "ntf1";
static const char *const CREDENTIAL_VAULT_NAMESPACE = "np2_credentials";
static const char *const CREDENTIAL_VAULT_SLOT_ZERO_KEY = "cred0";
static const char *const CREDENTIAL_VAULT_SLOT_ONE_KEY = "cred1";
static const char *const UPDATE_SLOT_ZERO_KEY = "ota0";
static const char *const UPDATE_SLOT_ONE_KEY = "ota1";
static const char *const LITTLEFS_PARTITION = "storage";
static const char *const LITTLEFS_BASE_PATH = "/lfsdiag";
static const char *const LITTLEFS_TMP_PATH = "/lfsdiag/cache.tmp";
static const char *const LITTLEFS_GENERATION_ZERO_PATH = "/lfsdiag/cache.0";
static const char *const LITTLEFS_GENERATION_ONE_PATH = "/lfsdiag/cache.1";
static const char *const LITTLEFS_FULL_PROBE_PATH = "/lfsdiag/full-probe.tmp";
static const char *const LITTLEFS_FULL_PROBE_TAIL_PATH = "/lfsdiag/full-probe.tail";

typedef struct {
    cache_record_header_t header;
    uint32_t value;
} config_record_t;

typedef struct {
    char ssid[FLASH_COORDINATOR_WIFI_SSID_BYTES];
    char password[FLASH_COORDINATOR_WIFI_PASSWORD_BYTES];
} credential_vault_t;

typedef enum {
    FLASH_REQUEST_ONBOARDING_PROFILE_WRITE,
    FLASH_REQUEST_NOTIFICATION_PROFILE_WRITE,
    FLASH_REQUEST_CREDENTIAL_VAULT_WRITE,
    FLASH_REQUEST_CREDENTIAL_VAULT_CLEAR,
    FLASH_REQUEST_NVS_PROBE,
    FLASH_REQUEST_NVS_COMPACTION_PROBE,
    FLASH_REQUEST_LITTLEFS_PROBE,
    FLASH_REQUEST_LITTLEFS_FORMAT,
    FLASH_REQUEST_CACHE_CORRUPT_NEWEST,
    FLASH_REQUEST_CACHE_FULL_PROBE,
    FLASH_REQUEST_CACHE_CUT_BEFORE_RENAME,
    FLASH_REQUEST_CACHE_CUT_AFTER_RENAME,
    FLASH_REQUEST_OFFLINE_DATA_WRITE,
    FLASH_REQUEST_CONFIG_JOURNAL_WRITE,
    FLASH_REQUEST_CONFIG_CORRUPT_NEWEST,
    FLASH_REQUEST_UPDATE_JOURNAL_WRITE,
    FLASH_REQUEST_P4_OTA_BEGIN,
    FLASH_REQUEST_P4_OTA_APPEND,
    FLASH_REQUEST_P4_OTA_FINISH,
    FLASH_REQUEST_P4_OTA_ABORT,
    FLASH_REQUEST_P4_OTA_ACTIVATE,
    FLASH_REQUEST_P4_OTA_CONFIRM,
    FLASH_REQUEST_P4_OTA_ROLLBACK,
} flash_request_kind_t;

typedef struct {
    flash_request_kind_t kind;
    uint32_t sequence;
    offline_data_snapshot_t offline_data;
    onboarding_profile_t onboarding_profile;
    notification_profile_t notification_profile;
    credential_vault_t credential_vault;
    update_journal_record_t update_journal;
    const esp_partition_t *ota_partition;
    uint32_t ota_expected_bytes;
    uint32_t ota_chunk_bytes;
    uint8_t ota_chunk[FLASH_COORDINATOR_OTA_CHUNK_BYTES];
} flash_request_t;

typedef struct {
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    uint32_t expected_bytes;
    uint32_t written_bytes;
    bool active;
    bool finished;
} p4_ota_session_t;

static QueueHandle_t s_request_queue;
static flash_request_t s_p4_ota_request;
static SemaphoreHandle_t s_p4_ota_submission_lock;
static portMUX_TYPE s_status_lock = portMUX_INITIALIZER_UNLOCKED;
static flash_coordinator_status_t s_status;
static int64_t s_last_success_us;
static int64_t s_last_offline_data_write_us;
static int64_t s_power_cut_deadline_us;
static p4_ota_session_t s_p4_ota_session;

static void refresh_cache_status(void);
static void refresh_config_status(void);
static void refresh_update_journal_status(void);
static void refresh_onboarding_profile_status(void);
static void refresh_notification_profile_status(void);
static void refresh_credential_vault_status(void);

static void secure_zero(void *buffer, size_t length)
{
    volatile uint8_t *bytes = buffer;
    while (length-- > 0U) {
        *bytes++ = 0;
    }
}

static size_t bounded_length(const char *value, size_t limit)
{
    size_t length = 0U;
    while (length < limit && value[length] != '\0') {
        ++length;
    }
    return length;
}

static void refresh_p4_ota_status(void)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.p4_ota_active = s_p4_ota_session.active;
    s_status.p4_ota_finished = s_p4_ota_session.finished;
    s_status.p4_ota_expected_bytes = s_p4_ota_session.expected_bytes;
    s_status.p4_ota_written_bytes = s_p4_ota_session.written_bytes;
    s_status.p4_ota_partition = s_p4_ota_session.partition;
    portEXIT_CRITICAL(&s_status_lock);
}

static bool make_full_probe_proposal_path(size_t index, char *out_path, size_t out_size)
{
    const int written = snprintf(out_path, out_size, "/lfsdiag/full-probe.proposal.%u",
                                 (unsigned int)index);
    return written > 0 && (size_t)written < out_size;
}

static esp_err_t remove_full_probe_files(void)
{
    esp_err_t result = ESP_OK;
    const char *const paths[] = {
        LITTLEFS_FULL_PROBE_PATH,
        LITTLEFS_FULL_PROBE_TAIL_PATH,
    };
    for (size_t index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        if (unlink(paths[index]) != 0 && errno != ENOENT) {
            ESP_LOGE(TAG, "cannot remove full-filesystem probe file %s: errno=%d", paths[index],
                     errno);
            result = ESP_FAIL;
        }
    }
    for (size_t index = 0; index < FLASH_COORDINATOR_FULL_PROBE_MAX_PROPOSALS; ++index) {
        char path[FLASH_COORDINATOR_FULL_PROBE_PATH_BYTES] = {0};
        if (!make_full_probe_proposal_path(index, path, sizeof(path))) {
            result = ESP_ERR_INVALID_SIZE;
            continue;
        }
        if (unlink(path) != 0 && errno != ENOENT) {
            ESP_LOGE(TAG, "cannot remove full-filesystem probe proposal %s: errno=%d",
                     path, errno);
            result = ESP_FAIL;
        }
    }
    return result;
}

static esp_err_t read_selected_offline_data(const cache_record_header_t *header,
                                            offline_data_snapshot_t *out_snapshot)
{
    if (header == NULL || out_snapshot == NULL ||
        (header->payload_size != OFFLINE_DATA_V1_ENCODED_SIZE &&
         header->payload_size != OFFLINE_DATA_V3_ENCODED_SIZE &&
         header->payload_size != OFFLINE_DATA_ENCODED_SIZE)) {
        return ESP_ERR_INVALID_SIZE;
    }
    const char *const path = (header->generation & 1U) == 0U
                                 ? LITTLEFS_GENERATION_ZERO_PATH
                                 : LITTLEFS_GENERATION_ONE_PATH;
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }
    uint8_t payload[OFFLINE_DATA_ENCODED_SIZE] = {0};
    esp_err_t result = ESP_OK;
    if (lseek(fd, (off_t)sizeof(cache_record_header_t), SEEK_SET) < 0 ||
        read(fd, payload, header->payload_size) != (ssize_t)header->payload_size ||
        !offline_data_snapshot_decode(payload, header->payload_size, out_snapshot)) {
        result = ESP_ERR_INVALID_RESPONSE;
    }
    if (close(fd) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    return result;
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
    const esp_err_t result = esp_vfs_littlefs_register(&config);
    if (result != ESP_OK) {
        return result;
    }

    /* A reset during the deliberately full G4 probe must not strand storage
     * full. This path is probe-owned only and never touches cache generations. */
    if (remove_full_probe_files() != ESP_OK) {
        ESP_LOGE(TAG, "cannot remove interrupted full-filesystem probe files");
        return ESP_FAIL;
    }
    return ESP_OK;
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

static void set_full_probe_progress(bool active, bool syncing, uint32_t written_bytes,
                                    uint32_t target_bytes)
{
    portENTER_CRITICAL(&s_status_lock);
    s_status.full_probe_active = active;
    s_status.full_probe_syncing = syncing;
    s_status.full_probe_written_bytes = written_bytes;
    s_status.full_probe_target_bytes = target_bytes;
    portEXIT_CRITICAL(&s_status_lock);
}

static void set_power_cut_window(bool active, bool after_rename)
{
    const int64_t deadline_us = active
                                    ? esp_timer_get_time() +
                                          (int64_t)FLASH_COORDINATOR_POWER_CUT_WINDOW_MS * 1000LL
                                    : 0LL;
    portENTER_CRITICAL(&s_status_lock);
    s_status.power_cut_window_active = active;
    s_status.power_cut_after_rename = active && after_rename;
    s_power_cut_deadline_us = deadline_us;
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
            const int failure_errno = written < 0 ? errno : EIO;
            ESP_LOGE(TAG, "LittleFS write failed: errno=%d", failure_errno);
            errno = failure_errno;
            return ESP_FAIL;
        }
        offset += (size_t)written;
    }
    return ESP_OK;
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
        !cache_record_header_is_valid(&header, FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES)) {
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
        (void)cache_record_select_newest(&generation_zero, zero_result == ESP_OK,
                                         &generation_one, one_result == ESP_OK, out_header);
    }
    return ESP_OK;
}

static void refresh_cache_status(void)
{
    cache_record_header_t selected = {0};
    const esp_err_t result = select_latest_cache(&selected);
    offline_data_snapshot_t offline_data = {0};
    const esp_err_t offline_data_result =
        result == ESP_OK ? read_selected_offline_data(&selected, &offline_data) : result;
    portENTER_CRITICAL(&s_status_lock);
    s_status.cache_result = result;
    s_status.cache_valid = result == ESP_OK;
    s_status.cache_generation = result == ESP_OK ? selected.generation : 0U;
    s_status.cache_schema_version = result == ESP_OK ? selected.schema_version : 0U;
    s_status.offline_data_valid = offline_data_result == ESP_OK;
    s_status.offline_data = offline_data_result == ESP_OK ? offline_data : (offline_data_snapshot_t){0};
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_cache_record_at_path(const char *path, uint32_t generation,
                                            const uint8_t *payload, size_t payload_size,
                                            int *out_failure_errno)
{
    if (out_failure_errno != NULL) {
        *out_failure_errno = 0;
    }
    if (path == NULL || payload == NULL || payload_size > FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    cache_record_header_t header = {
        .magic = NP2_CACHE_RECORD_MAGIC,
        .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
        .header_size = sizeof(cache_record_header_t),
        .generation = generation,
        .payload_size = payload_size,
        .payload_crc32 = cache_record_crc32(payload, payload_size),
    };
    header.header_crc32 =
        cache_record_crc32((const uint8_t *)&header, offsetof(cache_record_header_t, header_crc32));

    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        if (out_failure_errno != NULL) {
            *out_failure_errno = errno;
        }
        ESP_LOGE(TAG, "LittleFS cache temp open failed: errno=%d", errno);
        return ESP_FAIL;
    }
    errno = 0;
    esp_err_t result = write_all(fd, (const uint8_t *)&header, sizeof(header));
    if (result == ESP_OK) {
        result = write_all(fd, payload, payload_size);
    }
    if (result != ESP_OK && out_failure_errno != NULL) {
        *out_failure_errno = errno;
    }
    if (result == ESP_OK && fsync(fd) != 0) {
        if (out_failure_errno != NULL) {
            *out_failure_errno = errno;
        }
        result = ESP_FAIL;
    }
    if (close(fd) != 0) {
        const int close_errno = errno;
        if (result == ESP_OK || (out_failure_errno != NULL &&
                                 *out_failure_errno == ENOSPC && close_errno != ENOSPC)) {
            if (out_failure_errno != NULL) {
                *out_failure_errno = close_errno;
            }
            result = ESP_FAIL;
        }
    }
    if (result != ESP_OK) {
        return result;
    }

    cache_record_header_t verified = {0};
    result = validate_cache_generation(path, &verified);
    return result == ESP_OK && verified.generation == generation ? ESP_OK :
           (result == ESP_OK ? ESP_ERR_INVALID_RESPONSE : result);
}

static esp_err_t write_cache_temp_record(uint32_t generation, const uint8_t *payload,
                                         size_t payload_size)
{
    return write_cache_record_at_path(LITTLEFS_TMP_PATH, generation, payload, payload_size, NULL);
}

static esp_err_t write_cache_generation(uint32_t generation, const uint8_t *payload,
                                        size_t payload_size)
{
    esp_err_t result = write_cache_temp_record(generation, payload, payload_size);
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

/* Checkpoint every successful append. LittleFS marks an errored file with
 * LFS_F_ERRED: sync/close may then return success without committing its data.
 * Reopen after ENOSPC to discard only the last uncommitted append, never the
 * entire bulk filler. Smaller appends consume the remaining durable slack. */
static esp_err_t fill_full_probe_file(const char *path, uint8_t *buffer, size_t buffer_size,
                                      uint32_t *written_bytes, uint32_t target_bytes,
                                      int64_t deadline_us)
{
    size_t attempt_bytes = buffer_size;
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        return errno == ENOSPC ? ESP_OK : ESP_FAIL;
    }
    for (;;) {
        if (esp_timer_get_time() >= deadline_us || *written_bytes > target_bytes) {
            const esp_err_t result = esp_timer_get_time() >= deadline_us
                                         ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_SIZE;
            if (close(fd) != 0) {
                ESP_LOGE(TAG, "full probe close failed on budget exit: errno=%d", errno);
                return ESP_FAIL;
            }
            return result;
        }
        errno = 0;
        const ssize_t written = write(fd, buffer, attempt_bytes);
        int failure_errno = written < 0 ? errno : 0;
        if (written == 0) {
            failure_errno = EIO;
        }
        if (written > 0) {
            if (fsync(fd) != 0) {
                failure_errno = errno;
            } else {
                *written_bytes += (uint32_t)written;
                set_full_probe_progress(true, false, *written_bytes, target_bytes);
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
        }
        /* Do not sync after a failed write: that can report a false commit. */
        const int close_result = close(fd);
        const int close_errno = errno;
        if ((close_result != 0 && close_errno != ENOSPC) || failure_errno != ENOSPC) {
            ESP_LOGE(TAG, "full probe filler failed: io_errno=%d close_errno=%d",
                     failure_errno, close_result != 0 ? close_errno : 0);
            return ESP_FAIL;
        }
        if (attempt_bytes == FLASH_COORDINATOR_FULL_PROBE_TAIL_BYTES) {
            return ESP_OK;
        }
        attempt_bytes /= 4U;
        if (attempt_bytes < FLASH_COORDINATOR_FULL_PROBE_TAIL_BYTES) {
            attempt_bytes = FLASH_COORDINATOR_FULL_PROBE_TAIL_BYTES;
        }
        fd = open(path, O_WRONLY | O_APPEND);
        if (fd < 0) {
            ESP_LOGE(TAG, "full probe cannot reopen committed filler: errno=%d", errno);
            return ESP_FAIL;
        }
    }
}

static esp_err_t run_cache_full_probe(uint32_t sequence)
{
    const int64_t deadline_us = esp_timer_get_time() + 180LL * 1000LL * 1000LL;
    set_full_probe_progress(true, false, 0U, 0U);
    cache_record_header_t before = {0};
    esp_err_t result = select_latest_cache(&before);
    if (result != ESP_OK) {
        set_full_probe_progress(false, false, 0U, 0U);
        return result;
    }

    uint32_t written_bytes = 0U;
    uint32_t target_bytes = 0U;
    size_t total_bytes = 0U, used_bytes = 0U;
    bool baseline_ready = false;
    bool proposal_rejected = false;
    size_t accepted_proposals = 0U;
    static uint8_t fill_chunk[FLASH_COORDINATOR_FULL_PROBE_CHUNK_BYTES];
    uint8_t payload[OFFLINE_DATA_ENCODED_SIZE] = {0};
    const offline_data_snapshot_t proposal = {
        .schema_version = OFFLINE_DATA_SCHEMA_VERSION,
        .origin = OFFLINE_DATA_ORIGIN_LIVE,
        .weather = {.available = true, .temperature_deci_c = 215,
                    .relative_humidity_percent = 50U, .weather_code = 1U,
                    .observed_at_unix_s = UINT32_C(1760000000)},
    };

    result = remove_full_probe_files();
    if (result != ESP_OK) {
        goto cleanup;
    }
    if (unlink(LITTLEFS_TMP_PATH) != 0 && errno != ENOENT) {
        result = ESP_FAIL;
        goto cleanup;
    }
    result = esp_littlefs_info(LITTLEFS_PARTITION, &total_bytes, &used_bytes);
    if (result != ESP_OK || total_bytes > UINT32_MAX || used_bytes > total_bytes) {
        result = ESP_FAIL;
        goto cleanup;
    }
    baseline_ready = true;
    target_bytes = (uint32_t)(total_bytes - used_bytes);
    memset(fill_chunk, (int)(sequence & UINT32_C(0xff)), sizeof(fill_chunk));
    result = fill_full_probe_file(LITTLEFS_FULL_PROBE_PATH, fill_chunk, sizeof(fill_chunk),
                                  &written_bytes, target_bytes, deadline_us);
    if (result != ESP_OK) {
        goto cleanup;
    }
    set_full_probe_progress(true, true, written_bytes, target_bytes);
    if (!offline_data_snapshot_encode(&proposal, payload, sizeof(payload))) {
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    for (size_t index = 0; index < FLASH_COORDINATOR_FULL_PROBE_MAX_PROPOSALS; ++index) {
        if (esp_timer_get_time() >= deadline_us) {
            result = ESP_ERR_TIMEOUT;
            goto cleanup;
        }
        char proposal_path[FLASH_COORDINATOR_FULL_PROBE_PATH_BYTES] = {0};
        if (!make_full_probe_proposal_path(index, proposal_path, sizeof(proposal_path))) {
            result = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }
        int failure_errno = 0;
        result = write_cache_record_at_path(proposal_path, before.generation + 1U, payload,
                                            sizeof(payload), &failure_errno);
        if (result != ESP_OK) {
            proposal_rejected = failure_errno == ENOSPC;
            ESP_LOGI(TAG, "full probe proposal: accepted=%u errno=%d result=%s",
                     (unsigned int)accepted_proposals, failure_errno, esp_err_to_name(result));
            if (proposal_rejected) {
                result = ESP_OK;
            }
            goto cleanup;
        }
        ++accepted_proposals;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGE(TAG, "full probe inconclusive: all %u proposals fit after durable fill",
             (unsigned int)accepted_proposals);
    result = ESP_ERR_INVALID_STATE;

cleanup:;
    /* Every exit verifies cleanup. Never turn an I/O error into a passed test. */
    const esp_err_t cleanup_result = remove_full_probe_files();
    const int tmp_result = unlink(LITTLEFS_TMP_PATH);
    const int tmp_errno = errno;
    set_full_probe_progress(false, false, written_bytes, target_bytes);
    refresh_cache_status();
    if (cleanup_result != ESP_OK || (tmp_result != 0 && tmp_errno != ENOENT)) {
        ESP_LOGE(TAG, "full probe failed cleanup; result=%s tmp_errno=%d",
                 esp_err_to_name(cleanup_result), tmp_result != 0 ? tmp_errno : 0);
        return ESP_FAIL;
    }
    cache_record_header_t after = {0};
    const esp_err_t selected_result = select_latest_cache(&after);
    if (selected_result != ESP_OK || memcmp(&before, &after, sizeof(before)) != 0) {
        ESP_LOGE(TAG, "full probe cache preservation failed");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (baseline_ready) {
        size_t recovered_total = 0U, recovered_used = 0U;
        const esp_err_t info_result =
            esp_littlefs_info(LITTLEFS_PARTITION, &recovered_total, &recovered_used);
        ESP_LOGI(TAG, "full probe cleanup: used=%lu -> %lu bytes; committed filler=%lu",
                 (unsigned long)used_bytes, (unsigned long)recovered_used,
                 (unsigned long)written_bytes);
        if (info_result != ESP_OK || recovered_total != total_bytes || recovered_used > used_bytes) {
            ESP_LOGE(TAG, "full probe space recovery failed");
            return ESP_ERR_INVALID_STATE;
        }
    }
    return result == ESP_OK && !proposal_rejected ? ESP_ERR_INVALID_STATE : result;
}
static esp_err_t run_cache_power_cut_probe(uint32_t sequence, bool after_rename)
{
    cache_record_header_t latest = {0};
    ESP_RETURN_ON_ERROR(select_latest_cache(&latest), TAG,
                        "No valid cache generation before power-cut probe");

    const uint32_t generation = latest.generation + 1U;
    static uint8_t payload[FLASH_COORDINATOR_LITTLEFS_PAYLOAD_BYTES];
    memset(payload, (int)(sequence & UINT32_C(0xff)), sizeof(payload));
    cache_record_header_t header = {
        .magic = NP2_CACHE_RECORD_MAGIC,
        .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
        .header_size = sizeof(cache_record_header_t),
        .generation = generation,
        .payload_size = sizeof(payload),
        .payload_crc32 = cache_record_crc32(payload, sizeof(payload)),
    };
    header.header_crc32 =
        cache_record_crc32((const uint8_t *)&header, offsetof(cache_record_header_t, header_crc32));

    const int fd = open(LITTLEFS_TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return ESP_FAIL;
    }
    esp_err_t result = write_all(fd, (const uint8_t *)&header, sizeof(header));
    if (result == ESP_OK) {
        result = write_all(fd, payload, sizeof(payload));
    }
    if (result == ESP_OK && fsync(fd) != 0) {
        result = ESP_FAIL;
    }
    if (close(fd) != 0 && result == ESP_OK) {
        result = ESP_FAIL;
    }
    if (result != ESP_OK) {
        (void)unlink(LITTLEFS_TMP_PATH);
        return result;
    }

    if (!after_rename) {
        set_power_cut_window(true, false);
        ESP_LOGW(TAG, "POWER_CUT_NOW before rename; window=%ums", FLASH_COORDINATOR_POWER_CUT_WINDOW_MS);
        vTaskDelay(pdMS_TO_TICKS(FLASH_COORDINATOR_POWER_CUT_WINDOW_MS));
        set_power_cut_window(false, false);
        (void)unlink(LITTLEFS_TMP_PATH);
        return ESP_ERR_TIMEOUT;
    }

    const char *const destination = (generation & 1U) == 0U
                                        ? LITTLEFS_GENERATION_ZERO_PATH
                                        : LITTLEFS_GENERATION_ONE_PATH;
    if (rename(LITTLEFS_TMP_PATH, destination) != 0) {
        return ESP_FAIL;
    }
    set_power_cut_window(true, true);
    ESP_LOGW(TAG, "POWER_CUT_NOW after rename; window=%ums", FLASH_COORDINATOR_POWER_CUT_WINDOW_MS);
    vTaskDelay(pdMS_TO_TICKS(FLASH_COORDINATOR_POWER_CUT_WINDOW_MS));
    set_power_cut_window(false, false);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t write_offline_data_snapshot(const offline_data_snapshot_t *snapshot)
{
    if (!offline_data_snapshot_is_valid(snapshot)) {
        return ESP_ERR_INVALID_ARG;
    }
    cache_record_header_t latest = {0};
    const esp_err_t latest_result = select_latest_cache(&latest);
    const uint32_t generation = latest_result == ESP_OK ? latest.generation + 1U : 1U;
    uint8_t payload[OFFLINE_DATA_ENCODED_SIZE] = {0};
    if (!offline_data_snapshot_encode(snapshot, payload, sizeof(payload))) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t result = write_cache_generation(generation, payload, sizeof(payload));
    if (result == ESP_OK) {
        refresh_cache_status();
    }
    return result;
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
    if (record_size != sizeof(record) ||
        !cache_record_header_is_valid(&record.header, sizeof(record.value)) ||
        record.header.payload_size != sizeof(record.value) ||
        record.header.payload_crc32 !=
            cache_record_crc32((const uint8_t *)&record.value, sizeof(record.value))) {
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
            .magic = NP2_CACHE_RECORD_MAGIC,
            .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
            .header_size = sizeof(cache_record_header_t),
            .generation = generation,
            .payload_size = sizeof(uint32_t),
        },
        .value = generation,
    };
    record.header.payload_crc32 =
        cache_record_crc32((const uint8_t *)&record.value, sizeof(record.value));
    record.header.header_crc32 = cache_record_crc32((const uint8_t *)&record.header,
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

static bool onboarding_profile_is_valid(const onboarding_profile_t *profile)
{
    return profile != NULL && profile->timezone_index <= 1U;
}

typedef struct {
    cache_record_header_t header;
    onboarding_profile_t profile;
} onboarding_profile_record_t;

static esp_err_t read_onboarding_profile_slot(const char *key,
                                              onboarding_profile_record_t *out_record)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, ONBOARDING_NAMESPACE,
                                               NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    onboarding_profile_record_t record = {0};
    size_t record_size = sizeof(record);
    result = nvs_get_blob(handle, key, &record, &record_size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    if (record_size != sizeof(record) ||
        !cache_record_header_is_valid(&record.header, sizeof(record.profile)) ||
        record.header.payload_size != sizeof(record.profile) ||
        record.header.payload_crc32 != cache_record_crc32((const uint8_t *)&record.profile,
                                                            sizeof(record.profile)) ||
        !onboarding_profile_is_valid(&record.profile)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) *out_record = record;
    return ESP_OK;
}

static esp_err_t select_latest_onboarding_profile(onboarding_profile_record_t *out_record)
{
    onboarding_profile_record_t slot_zero = {0};
    onboarding_profile_record_t slot_one = {0};
    const esp_err_t zero_result = read_onboarding_profile_slot(ONBOARDING_SLOT_ZERO_KEY, &slot_zero);
    const esp_err_t one_result = read_onboarding_profile_slot(ONBOARDING_SLOT_ONE_KEY, &slot_one);
    if (zero_result != ESP_OK && one_result != ESP_OK) {
        return zero_result == ESP_ERR_NVS_NOT_FOUND && one_result == ESP_ERR_NVS_NOT_FOUND
                   ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) {
        *out_record = (one_result == ESP_OK &&
                       (zero_result != ESP_OK || slot_one.header.generation > slot_zero.header.generation))
                          ? slot_one : slot_zero;
    }
    return ESP_OK;
}

static void refresh_onboarding_profile_status(void)
{
    onboarding_profile_record_t selected = {0};
    const esp_err_t result = select_latest_onboarding_profile(&selected);
    portENTER_CRITICAL(&s_status_lock);
    s_status.onboarding_profile_result = result;
    s_status.onboarding_profile_valid = result == ESP_OK;
    s_status.onboarding_profile_generation = result == ESP_OK ? selected.header.generation : 0U;
    s_status.onboarding_profile = result == ESP_OK ? selected.profile : (onboarding_profile_t){0};
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_onboarding_profile(const onboarding_profile_t *profile)
{
    if (!onboarding_profile_is_valid(profile)) return ESP_ERR_INVALID_ARG;
    onboarding_profile_record_t latest = {0};
    const esp_err_t latest_result = select_latest_onboarding_profile(&latest);
    const uint32_t generation = latest_result == ESP_OK ? latest.header.generation + 1U : 1U;
    onboarding_profile_record_t record = {
        .header = {.magic = NP2_CACHE_RECORD_MAGIC,
                   .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
                   .header_size = sizeof(cache_record_header_t),
                   .generation = generation,
                   .payload_size = sizeof(profile[0])},
        .profile = *profile,
    };
    record.header.payload_crc32 = cache_record_crc32((const uint8_t *)&record.profile,
                                                      sizeof(record.profile));
    record.header.header_crc32 = cache_record_crc32((const uint8_t *)&record.header,
                                                     offsetof(cache_record_header_t, header_crc32));
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, ONBOARDING_NAMESPACE,
                                                NVS_READWRITE, &handle), TAG,
                        "Onboarding profile open failed");
    const char *target = (generation & 1U) == 0U ? ONBOARDING_SLOT_ZERO_KEY : ONBOARDING_SLOT_ONE_KEY;
    esp_err_t result = nvs_set_blob(handle, target, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) {
        refresh_onboarding_profile_status();
        onboarding_profile_record_t selected = {0};
        result = select_latest_onboarding_profile(&selected);
        if (result == ESP_OK && selected.header.generation != generation) result = ESP_FAIL;
    }
    return result;
}

typedef struct {
    cache_record_header_t header;
    notification_profile_t profile;
} notification_profile_record_t;

static bool notification_profile_is_valid(const notification_profile_t *profile)
{
    return profile != NULL;
}

static esp_err_t read_notification_profile_slot(const char *key,
                                                notification_profile_record_t *out_record)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, NOTIFICATION_NAMESPACE,
                                               NVS_READONLY, &handle);
    if (result != ESP_OK) return result;

    notification_profile_record_t record = {0};
    size_t record_size = sizeof(record);
    result = nvs_get_blob(handle, key, &record, &record_size);
    nvs_close(handle);
    if (result != ESP_OK) return result;

    if (record_size != sizeof(record) ||
        !cache_record_header_is_valid(&record.header, sizeof(record.profile)) ||
        record.header.payload_size != sizeof(record.profile) ||
        record.header.payload_crc32 != cache_record_crc32(
                                        (const uint8_t *)&record.profile,
                                        sizeof(record.profile)) ||
        !notification_profile_is_valid(&record.profile)) {
        return ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) *out_record = record;
    return ESP_OK;
}

static esp_err_t select_latest_notification_profile(notification_profile_record_t *out_record)
{
    notification_profile_record_t slot_zero = {0};
    notification_profile_record_t slot_one = {0};
    const esp_err_t zero_result = read_notification_profile_slot(
        NOTIFICATION_SLOT_ZERO_KEY, &slot_zero);
    const esp_err_t one_result = read_notification_profile_slot(
        NOTIFICATION_SLOT_ONE_KEY, &slot_one);
    if (zero_result != ESP_OK && one_result != ESP_OK) {
        return zero_result == ESP_ERR_NVS_NOT_FOUND && one_result == ESP_ERR_NVS_NOT_FOUND
                   ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) {
        *out_record = one_result == ESP_OK &&
                              (zero_result != ESP_OK ||
                               slot_one.header.generation > slot_zero.header.generation)
                          ? slot_one
                          : slot_zero;
    }
    return ESP_OK;
}

static void refresh_notification_profile_status(void)
{
    notification_profile_record_t selected = {0};
    const esp_err_t result = select_latest_notification_profile(&selected);
    portENTER_CRITICAL(&s_status_lock);
    s_status.notification_profile_result = result;
    s_status.notification_profile_valid = result == ESP_OK;
    s_status.notification_profile_generation =
        result == ESP_OK ? selected.header.generation : 0U;
    s_status.notification_profile =
        result == ESP_OK ? selected.profile : (notification_profile_t){0};
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_notification_profile(const notification_profile_t *profile)
{
    if (!notification_profile_is_valid(profile)) return ESP_ERR_INVALID_ARG;

    notification_profile_record_t latest = {0};
    const esp_err_t latest_result = select_latest_notification_profile(&latest);
    if (latest_result == ESP_OK && latest.header.generation == UINT32_MAX) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint32_t generation = latest_result == ESP_OK ? latest.header.generation + 1U : 1U;
    notification_profile_record_t record = {
        .header = {.magic = NP2_CACHE_RECORD_MAGIC,
                   .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
                   .header_size = sizeof(cache_record_header_t),
                   .generation = generation,
                   .payload_size = sizeof(profile[0])},
        .profile = *profile,
    };
    record.header.payload_crc32 = cache_record_crc32((const uint8_t *)&record.profile,
                                                      sizeof(record.profile));
    record.header.header_crc32 = cache_record_crc32((const uint8_t *)&record.header,
                                                     offsetof(cache_record_header_t, header_crc32));

    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, NOTIFICATION_NAMESPACE,
                                                NVS_READWRITE, &handle), TAG,
                        "Notification profile open failed");
    const char *const target = (generation & 1U) == 0U
                                   ? NOTIFICATION_SLOT_ZERO_KEY
                                   : NOTIFICATION_SLOT_ONE_KEY;
    esp_err_t result = nvs_set_blob(handle, target, &record, sizeof(record));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    if (result == ESP_OK) {
        refresh_notification_profile_status();
        notification_profile_record_t selected = {0};
        result = select_latest_notification_profile(&selected);
        if (result == ESP_OK && selected.header.generation != generation) result = ESP_FAIL;
    }
    return result;
}

typedef struct {
    cache_record_header_t header;
    credential_vault_t credentials;
} credential_vault_record_t;

static credential_vault_t s_credential_vault;

static bool credential_vault_is_valid(const credential_vault_t *credentials)
{
    if (credentials == NULL) return false;
    const size_t ssid_length = bounded_length(credentials->ssid, sizeof(credentials->ssid));
    const size_t password_length = bounded_length(credentials->password, sizeof(credentials->password));
    return ssid_length > 0U && ssid_length < sizeof(credentials->ssid) &&
           password_length >= 8U && password_length < sizeof(credentials->password);
}

static esp_err_t read_credential_vault_slot(
    const char *key, credential_vault_record_t *out_record)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, CREDENTIAL_VAULT_NAMESPACE,
                                               NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    credential_vault_record_t record = {0};
    size_t record_size = sizeof(record);
    result = nvs_get_blob(handle, key, &record, &record_size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    if (record_size != sizeof(record) ||
        !cache_record_header_is_valid(&record.header, sizeof(record.credentials)) ||
        record.header.payload_size != sizeof(record.credentials) ||
        record.header.payload_crc32 != cache_record_crc32(
                                        (const uint8_t *)&record.credentials,
                                        sizeof(record.credentials)) ||
        !credential_vault_is_valid(&record.credentials)) {
        secure_zero(&record, sizeof(record));
        return ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) *out_record = record;
    secure_zero(&record, sizeof(record));
    return ESP_OK;
}

static esp_err_t select_latest_credential_vault(
    credential_vault_record_t *out_record)
{
    credential_vault_record_t slot_zero = {0};
    credential_vault_record_t slot_one = {0};
    const esp_err_t zero_result = read_credential_vault_slot(
        CREDENTIAL_VAULT_SLOT_ZERO_KEY, &slot_zero);
    const esp_err_t one_result = read_credential_vault_slot(
        CREDENTIAL_VAULT_SLOT_ONE_KEY, &slot_one);
    if (zero_result != ESP_OK && one_result != ESP_OK) {
        secure_zero(&slot_zero, sizeof(slot_zero));
        secure_zero(&slot_one, sizeof(slot_one));
        /* Present a missing two-slot vault like the other record stores.
         * The first credential write accepts ESP_ERR_NOT_FOUND to create
         * generation 1. Returning the NVS-specific code here made that
         * initial write fail before it reached nvs_set_blob(). */
        return zero_result == ESP_ERR_NVS_NOT_FOUND && one_result == ESP_ERR_NVS_NOT_FOUND
                   ? ESP_ERR_NOT_FOUND
                   : zero_result == ESP_ERR_NOT_SUPPORTED ? zero_result : ESP_ERR_INVALID_CRC;
    }
    if (out_record != NULL) {
        *out_record = one_result == ESP_OK &&
                              (zero_result != ESP_OK ||
                               slot_one.header.generation > slot_zero.header.generation)
                          ? slot_one
                          : slot_zero;
    }
    secure_zero(&slot_zero, sizeof(slot_zero));
    secure_zero(&slot_one, sizeof(slot_one));
    return ESP_OK;
}

static void refresh_credential_vault_status(void)
{
    credential_vault_record_t selected = {0};
    const esp_err_t result = select_latest_credential_vault(&selected);
    portENTER_CRITICAL(&s_status_lock);
    secure_zero(&s_credential_vault, sizeof(s_credential_vault));
    if (result == ESP_OK) {
        s_credential_vault = selected.credentials;
    }
    s_status.credential_vault_result = result;
    s_status.credential_vault_valid = result == ESP_OK;
    s_status.credential_vault_generation = result == ESP_OK
                                                            ? selected.header.generation
                                                            : 0U;
    portEXIT_CRITICAL(&s_status_lock);
    secure_zero(&selected, sizeof(selected));
}

static esp_err_t write_credential_vault(const credential_vault_t *credentials)
{
    if (!flash_coordinator_credential_vault_ready()) return ESP_ERR_NOT_SUPPORTED;
    if (!credential_vault_is_valid(credentials)) return ESP_ERR_INVALID_ARG;
    credential_vault_record_t latest = {0};
    const esp_err_t latest_result = select_latest_credential_vault(&latest);
    if (latest_result != ESP_OK && latest_result != ESP_ERR_NOT_FOUND) {
        secure_zero(&latest, sizeof(latest));
        return latest_result;
    }
    const uint32_t generation = latest_result == ESP_OK ? latest.header.generation + 1U : 1U;
    secure_zero(&latest, sizeof(latest));
    if (generation == 0U) return ESP_ERR_INVALID_STATE;
    credential_vault_record_t record = {
        .header = {.magic = NP2_CACHE_RECORD_MAGIC,
                   .schema_version = NP2_CACHE_RECORD_SCHEMA_VERSION,
                   .header_size = sizeof(cache_record_header_t),
                   .generation = generation,
                   .payload_size = sizeof(credentials[0])},
        .credentials = *credentials,
    };
    record.header.payload_crc32 = cache_record_crc32((const uint8_t *)&record.credentials,
                                                      sizeof(record.credentials));
    record.header.header_crc32 = cache_record_crc32((const uint8_t *)&record.header,
                                                     offsetof(cache_record_header_t, header_crc32));
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, CREDENTIAL_VAULT_NAMESPACE,
                                               NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        const char *const target = (generation & 1U) == 0U
                                       ? CREDENTIAL_VAULT_SLOT_ZERO_KEY
                                       : CREDENTIAL_VAULT_SLOT_ONE_KEY;
        result = nvs_set_blob(handle, target, &record, sizeof(record));
        if (result == ESP_OK) result = nvs_commit(handle);
        nvs_close(handle);
    }
    secure_zero(&record, sizeof(record));
    refresh_credential_vault_status();
    return result;
}

static esp_err_t clear_credential_vault(void)
{
    if (!flash_coordinator_credential_vault_ready()) return ESP_ERR_NOT_SUPPORTED;
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, CREDENTIAL_VAULT_NAMESPACE,
                                               NVS_READWRITE, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        secure_zero(&s_credential_vault, sizeof(s_credential_vault));
        refresh_credential_vault_status();
        return ESP_OK;
    }
    if (result != ESP_OK) return result;
    const esp_err_t zero_result = nvs_erase_key(handle, CREDENTIAL_VAULT_SLOT_ZERO_KEY);
    const esp_err_t one_result = nvs_erase_key(handle, CREDENTIAL_VAULT_SLOT_ONE_KEY);
    if ((zero_result != ESP_OK && zero_result != ESP_ERR_NVS_NOT_FOUND) ||
        (one_result != ESP_OK && one_result != ESP_ERR_NVS_NOT_FOUND)) {
        result = zero_result != ESP_OK && zero_result != ESP_ERR_NVS_NOT_FOUND
                     ? zero_result
                     : one_result;
    } else {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    refresh_credential_vault_status();
    return result;
}

static esp_err_t initialize_nvs(void)
{
#if defined(CONFIG_NVS_ENCRYPTION)
    /* Do not generate or use an NVS key before its key partition is protected
     * by Flash Encryption. The production provisioning sequence enables this
     * eFuse before this application is allowed to initialize NVS. */
    if (!esp_flash_encryption_enabled()) return ESP_ERR_INVALID_STATE;
#endif
    return nvs_flash_init();
}

static esp_err_t read_update_journal_slot(const char *key, update_journal_record_t *out_record)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open_from_partition(NVS_PARTITION, UPDATE_NAMESPACE, NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    update_journal_record_t record = {0};
    size_t size = sizeof(record);
    result = nvs_get_blob(handle, key, &record, &size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    if (size != sizeof(record) || !update_journal_is_valid(&record)) return ESP_ERR_INVALID_CRC;
    if (out_record != NULL) *out_record = record;
    return ESP_OK;
}

static esp_err_t select_latest_update_journal(update_journal_record_t *out_record)
{
    update_journal_record_t zero = {0}, one = {0};
    const esp_err_t zero_result = read_update_journal_slot(UPDATE_SLOT_ZERO_KEY, &zero);
    const esp_err_t one_result = read_update_journal_slot(UPDATE_SLOT_ONE_KEY, &one);
    if (zero_result != ESP_OK && one_result != ESP_OK)
        return zero_result == ESP_ERR_NVS_NOT_FOUND && one_result == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_CRC;
    if (zero_result == ESP_OK && one_result == ESP_OK && zero.generation == one.generation &&
        memcmp(&zero, &one, sizeof(zero)) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_record != NULL)
        *out_record = one_result == ESP_OK && (zero_result != ESP_OK || one.generation > zero.generation) ? one : zero;
    return ESP_OK;
}

static void refresh_update_journal_status(void)
{
    update_journal_record_t selected = {0};
    const esp_err_t result = select_latest_update_journal(&selected);
    portENTER_CRITICAL(&s_status_lock);
    s_status.update_journal_result = result;
    s_status.update_journal_valid = result == ESP_OK;
    s_status.update_journal_generation = result == ESP_OK ? selected.generation : 0U;
    s_status.update_journal_state = result == ESP_OK ? selected.state : UPDATE_JOURNAL_IDLE;
    portEXIT_CRITICAL(&s_status_lock);
}

static esp_err_t write_update_journal(const update_journal_record_t *input)
{
    if (input == NULL || !update_journal_is_valid(input) || input->generation != 0U) return ESP_ERR_INVALID_ARG;
    update_journal_record_t latest = {0};
    const esp_err_t latest_result = select_latest_update_journal(&latest);
    if (latest_result != ESP_OK && latest_result != ESP_ERR_NOT_FOUND) return latest_result;
    if (latest_result == ESP_OK && latest.generation == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    if (latest_result == ESP_ERR_NOT_FOUND && input->state != UPDATE_JOURNAL_P4_STAGED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (latest_result == ESP_OK && !update_journal_can_follow(&latest, input)) {
        return ESP_ERR_INVALID_STATE;
    }
    update_journal_record_t candidate = *input;
    const uint32_t generation = latest_result == ESP_OK ? latest.generation + 1U : 1U;
    if (!update_journal_set_generation(&candidate, generation)) return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open_from_partition(NVS_PARTITION, UPDATE_NAMESPACE, NVS_READWRITE, &handle), TAG, "OTA journal open failed");
    const char *target = (generation & 1U) == 0U ? UPDATE_SLOT_ZERO_KEY : UPDATE_SLOT_ONE_KEY;
    esp_err_t result = nvs_set_blob(handle, target, &candidate, sizeof(candidate));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    update_journal_record_t verified = {0};
    const esp_err_t verified_result = select_latest_update_journal(&verified);
    refresh_update_journal_status();
    return result == ESP_OK && verified_result == ESP_OK && verified.generation == generation ?
               ESP_OK : ESP_FAIL;
}

static esp_err_t activate_p4_ota_partition(const esp_partition_t *partition,
                                           const update_journal_record_t *journal)
{
    if (partition == NULL || journal == NULL || !update_journal_is_valid(journal) ||
        journal->state != UPDATE_JOURNAL_P4_PENDING || journal->generation == 0U ||
        partition->type != ESP_PARTITION_TYPE_APP ||
        (partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
         partition->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1)) {
        return ESP_ERR_INVALID_ARG;
    }

    update_journal_record_t persisted = {0};
    const esp_err_t journal_result = select_latest_update_journal(&persisted);
    if (journal_result != ESP_OK || memcmp(&persisted, journal, sizeof(persisted)) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_ota_set_boot_partition(partition);
}

static esp_err_t begin_p4_ota(uint32_t expected_bytes)
{
    if (expected_bytes == 0U || s_p4_ota_session.active || s_p4_ota_session.finished) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_partition_t *const partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL || expected_bytes > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_ota_handle_t handle = 0;
    /* Incremental erase; the signed size remains enforced by this session. */
    const esp_err_t result = esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (result != ESP_OK) {
        return result;
    }
    s_p4_ota_session = (p4_ota_session_t){
        .partition = partition,
        .handle = handle,
        .expected_bytes = expected_bytes,
        .active = true,
    };
    refresh_p4_ota_status();
    return ESP_OK;
}

static esp_err_t append_p4_ota(const uint8_t *bytes, uint32_t bytes_count)
{
    if (bytes == NULL || bytes_count == 0U || bytes_count > FLASH_COORDINATOR_OTA_CHUNK_BYTES ||
        !s_p4_ota_session.active ||
        s_p4_ota_session.written_bytes > s_p4_ota_session.expected_bytes ||
        bytes_count > s_p4_ota_session.expected_bytes - s_p4_ota_session.written_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t result = esp_ota_write(s_p4_ota_session.handle, bytes, bytes_count);
    if (result == ESP_OK) {
        s_p4_ota_session.written_bytes += bytes_count;
        refresh_p4_ota_status();
    }
    return result;
}

static esp_err_t finish_p4_ota(void)
{
    if (!s_p4_ota_session.active ||
        s_p4_ota_session.written_bytes != s_p4_ota_session.expected_bytes) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result = esp_ota_end(s_p4_ota_session.handle);
    s_p4_ota_session.active = false;
    if (result == ESP_OK) {
        s_p4_ota_session.finished = true;
    } else {
        s_p4_ota_session = (p4_ota_session_t){0};
    }
    refresh_p4_ota_status();
    return result;
}

static esp_err_t abort_p4_ota(void)
{
    esp_err_t result = ESP_OK;
    if (s_p4_ota_session.active) {
        result = esp_ota_abort(s_p4_ota_session.handle);
    }
    s_p4_ota_session = (p4_ota_session_t){0};
    refresh_p4_ota_status();
    return result;
}

static esp_err_t activate_finished_p4_ota(const update_journal_record_t *journal)
{
    if (!s_p4_ota_session.finished || s_p4_ota_session.partition == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t result = activate_p4_ota_partition(s_p4_ota_session.partition, journal);
    if (result == ESP_OK) {
        s_p4_ota_session = (p4_ota_session_t){0};
        refresh_p4_ota_status();
    }
    return result;
}

static esp_err_t confirm_running_p4_ota(void)
{
    const esp_partition_t *const running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == NULL || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_ERR_INVALID_STATE;
    }
    update_journal_record_t persisted = {0};
    const esp_err_t journal_result = select_latest_update_journal(&persisted);
    if (journal_result != ESP_OK) {
        return journal_result;
    }
    if (persisted.state == UPDATE_JOURNAL_P4_PENDING) {
        update_journal_record_t accepted = {0};
        if (!update_journal_next(&persisted, UPDATE_JOURNAL_ACCEPTED, &accepted)) {
            return ESP_ERR_INVALID_STATE;
        }
        const esp_err_t persist_result = write_update_journal(&accepted);
        if (persist_result != ESP_OK) {
            return persist_result;
        }
    } else if (persisted.state != UPDATE_JOURNAL_ACCEPTED) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_err_t confirm_result = esp_ota_mark_app_valid_cancel_rollback();
    if (confirm_result != ESP_OK) {
        return confirm_result;
    }
    update_journal_record_t accepted = {0};
    const esp_err_t accepted_result = select_latest_update_journal(&accepted);
    update_journal_record_t idle = {0};
    if (accepted_result != ESP_OK || accepted.state != UPDATE_JOURNAL_ACCEPTED ||
        !update_journal_next(&accepted, UPDATE_JOURNAL_IDLE, &idle)) {
        return ESP_ERR_INVALID_STATE;
    }
    return write_update_journal(&idle);
}

static esp_err_t rollback_running_p4_ota(void)
{
    const esp_partition_t *const running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == NULL || esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_ERR_INVALID_STATE;
    }
    const esp_partition_t *const fallback = esp_ota_get_next_update_partition(NULL);
    esp_ota_img_states_t fallback_state = ESP_OTA_IMG_UNDEFINED;
    if (fallback == NULL || esp_ota_get_state_partition(fallback, &fallback_state) != ESP_OK ||
        fallback_state != ESP_OTA_IMG_VALID || !esp_ota_check_rollback_is_possible()) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_ota_mark_app_invalid_rollback_and_reboot();
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
    refresh_update_journal_status();
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

    const esp_err_t init_result = initialize_nvs();
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
    refresh_onboarding_profile_status();
    refresh_notification_profile_status();
    refresh_credential_vault_status();
    refresh_update_journal_status();
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
        case FLASH_REQUEST_ONBOARDING_PROFILE_WRITE:
            result = write_onboarding_profile(&request.onboarding_profile);
            break;
        case FLASH_REQUEST_NOTIFICATION_PROFILE_WRITE:
            result = write_notification_profile(&request.notification_profile);
            break;
        case FLASH_REQUEST_CREDENTIAL_VAULT_WRITE:
            result = write_credential_vault(&request.credential_vault);
            break;
        case FLASH_REQUEST_CREDENTIAL_VAULT_CLEAR:
            result = clear_credential_vault();
            break;
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
        case FLASH_REQUEST_CACHE_FULL_PROBE:
            result = run_cache_full_probe(request.sequence);
            break;
        case FLASH_REQUEST_CACHE_CUT_BEFORE_RENAME:
            result = run_cache_power_cut_probe(request.sequence, false);
            break;
        case FLASH_REQUEST_CACHE_CUT_AFTER_RENAME:
            result = run_cache_power_cut_probe(request.sequence, true);
            break;
        case FLASH_REQUEST_OFFLINE_DATA_WRITE:
            result = write_offline_data_snapshot(&request.offline_data);
            break;
        case FLASH_REQUEST_CONFIG_JOURNAL_WRITE:
            result = write_config_journal();
            break;
        case FLASH_REQUEST_CONFIG_CORRUPT_NEWEST:
            result = corrupt_newest_config_generation();
            break;
        case FLASH_REQUEST_UPDATE_JOURNAL_WRITE:
            result = write_update_journal(&request.update_journal);
            break;
        case FLASH_REQUEST_P4_OTA_BEGIN:
            result = begin_p4_ota(request.ota_expected_bytes);
            break;
        case FLASH_REQUEST_P4_OTA_APPEND:
            result = append_p4_ota(request.ota_chunk, request.ota_chunk_bytes);
            break;
        case FLASH_REQUEST_P4_OTA_FINISH:
            result = finish_p4_ota();
            break;
        case FLASH_REQUEST_P4_OTA_ABORT:
            result = abort_p4_ota();
            break;
        case FLASH_REQUEST_P4_OTA_ACTIVATE:
            result = request.ota_partition == s_p4_ota_session.partition
                         ? activate_finished_p4_ota(&request.update_journal)
                         : ESP_ERR_INVALID_STATE;
            break;
        case FLASH_REQUEST_P4_OTA_CONFIRM:
            result = confirm_running_p4_ota();
            break;
        case FLASH_REQUEST_P4_OTA_ROLLBACK:
            result = rollback_running_p4_ota();
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
            if (request.kind == FLASH_REQUEST_ONBOARDING_PROFILE_WRITE) {
                ESP_LOGI(TAG, "onboarding profile saved in %lums", (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_NOTIFICATION_PROFILE_WRITE) {
                ESP_LOGI(TAG, "notification preferences saved in %lums",
                         (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CREDENTIAL_VAULT_WRITE) {
                ESP_LOGI(TAG, "credential vault credentials saved in %lums",
                         (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CREDENTIAL_VAULT_CLEAR) {
                ESP_LOGI(TAG, "credential vault credentials cleared in %lums",
                         (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_OFFLINE_DATA_WRITE) {
                s_last_offline_data_write_us = s_last_success_us;
            }
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
            } else if (request.kind == FLASH_REQUEST_CACHE_FULL_PROBE) {
                ESP_LOGW(TAG, "cache full-filesystem probe %lu preserved generation in %lums",
                         (unsigned long)request.sequence, (unsigned long)duration_ms);
            } else if (request.kind == FLASH_REQUEST_CACHE_CUT_BEFORE_RENAME ||
                       request.kind == FLASH_REQUEST_CACHE_CUT_AFTER_RENAME) {
                ESP_LOGE(TAG, "cache power-cut probe was not cut within its window");
            } else if (request.kind == FLASH_REQUEST_OFFLINE_DATA_WRITE) {
                flash_coordinator_status_t data_status = {0};
                flash_coordinator_get_status(&data_status);
                ESP_LOGI(TAG, "offline snapshot cache generation %lu completed in %lums",
                         (unsigned long)data_status.cache_generation,
                         (unsigned long)duration_ms);
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
            ESP_LOGE(TAG, "flash request kind=%u sequence=%lu failed: %s",
                     (unsigned int)request.kind, (unsigned long)request.sequence,
                     esp_err_to_name(result));
        }
        if (request.kind == FLASH_REQUEST_NOTIFICATION_PROFILE_WRITE) {
            portENTER_CRITICAL(&s_status_lock);
            s_status.notification_profile_completed_sequence = request.sequence;
            s_status.notification_profile_last_write_result = result;
            portEXIT_CRITICAL(&s_status_lock);
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

    s_p4_ota_submission_lock = xSemaphoreCreateMutex();
    if (s_p4_ota_submission_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const BaseType_t task_created = xTaskCreate(flash_worker_task, "flash_worker",
                                                 FLASH_COORDINATOR_TASK_STACK_BYTES, NULL,
                                                 FLASH_COORDINATOR_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        vSemaphoreDelete(s_p4_ota_submission_lock);
        s_p4_ota_submission_lock = NULL;
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

esp_err_t flash_coordinator_request_cache_full_probe(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.cache_valid || !status.littlefs_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return enqueue_littlefs_request(FLASH_REQUEST_CACHE_FULL_PROBE);
}

esp_err_t flash_coordinator_request_cache_cut_before_rename(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.cache_valid || !status.littlefs_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return enqueue_littlefs_request(FLASH_REQUEST_CACHE_CUT_BEFORE_RENAME);
}

esp_err_t flash_coordinator_request_cache_cut_after_rename(void)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.cache_valid || !status.littlefs_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return enqueue_littlefs_request(FLASH_REQUEST_CACHE_CUT_AFTER_RENAME);
}

esp_err_t flash_coordinator_request_offline_data_write(const offline_data_snapshot_t *snapshot)
{
    if (!offline_data_snapshot_is_valid(snapshot) || s_request_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || !status.littlefs_ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }
    const int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_status_lock);
    const bool too_soon = s_last_offline_data_write_us != 0LL &&
                          now_us - s_last_offline_data_write_us <
                              FLASH_COORDINATOR_OFFLINE_DATA_MIN_INTERVAL_US;
    portEXIT_CRITICAL(&s_status_lock);
    if (too_soon) {
        return ESP_ERR_TIMEOUT;
    }
    const flash_request_t request = {
        .kind = FLASH_REQUEST_OFFLINE_DATA_WRITE,
        .sequence = status.last_sequence + 1U,
        .offline_data = *snapshot,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) {
        portENTER_CRITICAL(&s_status_lock);
        ++s_status.rejected_count;
        s_status.last_result = ESP_ERR_TIMEOUT;
        portEXIT_CRITICAL(&s_status_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_onboarding_profile_write(const onboarding_profile_t *profile)
{
    if (!onboarding_profile_is_valid(profile) || s_request_queue == NULL) return ESP_ERR_INVALID_ARG;
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) return ESP_ERR_INVALID_STATE;
    const flash_request_t request = {.kind = FLASH_REQUEST_ONBOARDING_PROFILE_WRITE,
                                     .sequence = status.last_sequence + 1U,
                                     .onboarding_profile = *profile};
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) return ESP_ERR_TIMEOUT;
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_notification_profile_write(
    const notification_profile_t *profile, uint32_t *out_sequence)
{
    if (!notification_profile_is_valid(profile) || s_request_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) return ESP_ERR_INVALID_STATE;

    const flash_request_t request = {
        .kind = FLASH_REQUEST_NOTIFICATION_PROFILE_WRITE,
        .sequence = status.last_sequence + 1U,
        .notification_profile = *profile,
    };
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) return ESP_ERR_TIMEOUT;
    if (out_sequence != NULL) *out_sequence = request.sequence;
    set_busy(false, true);
    return ESP_OK;
}

bool flash_coordinator_credential_vault_ready(void)
{
#if defined(CONFIG_NVS_ENCRYPTION)
    return esp_flash_encryption_enabled();
#elif defined(NP2_DEVELOPMENT_WIFI_CREDENTIAL_RETENTION)
    /* Development-only opt-in. Production builds never define this path. */
    return true;
#else
    return false;
#endif
}

esp_err_t flash_coordinator_request_credential_vault_write(const char *ssid,
                                                                        const char *password)
{
    if (!flash_coordinator_credential_vault_ready() || ssid == NULL ||
        password == NULL || s_request_queue == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    flash_request_t request = {.kind = FLASH_REQUEST_CREDENTIAL_VAULT_WRITE};
    const size_t ssid_length = bounded_length(ssid, sizeof(request.credential_vault.ssid));
    const size_t password_length = bounded_length(password,
                                                  sizeof(request.credential_vault.password));
    if (ssid_length == 0U || ssid_length >= sizeof(request.credential_vault.ssid) ||
        password_length < 8U ||
        password_length >= sizeof(request.credential_vault.password)) {
        secure_zero(&request, sizeof(request));
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(request.credential_vault.ssid, ssid, ssid_length);
    memcpy(request.credential_vault.password, password, password_length);
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) {
        secure_zero(&request, sizeof(request));
        return ESP_ERR_INVALID_STATE;
    }
    request.sequence = status.last_sequence + 1U;
    const BaseType_t sent = xQueueSend(s_request_queue, &request, 0);
    secure_zero(&request, sizeof(request));
    if (sent != pdPASS) return ESP_ERR_TIMEOUT;
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_credential_vault_clear(void)
{
    if (!flash_coordinator_credential_vault_ready() || s_request_queue == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) return ESP_ERR_INVALID_STATE;
    const flash_request_t request = {.kind = FLASH_REQUEST_CREDENTIAL_VAULT_CLEAR,
                                     .sequence = status.last_sequence + 1U};
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) return ESP_ERR_TIMEOUT;
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_copy_credential_vault(char *out_ssid, size_t ssid_size,
                                                               char *out_password, size_t password_size)
{
    if (!flash_coordinator_credential_vault_ready() || out_ssid == NULL ||
        out_password == NULL || ssid_size < FLASH_COORDINATOR_WIFI_SSID_BYTES ||
        password_size < FLASH_COORDINATOR_WIFI_PASSWORD_BYTES) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t result = ESP_OK;
    portENTER_CRITICAL(&s_status_lock);
    if (!s_status.credential_vault_valid) {
        result = s_status.credential_vault_result;
    } else {
        memcpy(out_ssid, s_credential_vault.ssid,
               sizeof(s_credential_vault.ssid));
        memcpy(out_password, s_credential_vault.password,
               sizeof(s_credential_vault.password));
    }
    portEXIT_CRITICAL(&s_status_lock);
    return result;
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

esp_err_t flash_coordinator_request_update_journal(const update_journal_record_t *record)
{
    if (record == NULL || !update_journal_is_valid(record) || record->generation != 0U ||
        s_request_queue == NULL) return ESP_ERR_INVALID_ARG;
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (!status.ready || status.busy || status.pending) return ESP_ERR_INVALID_STATE;
    const flash_request_t request = {.kind = FLASH_REQUEST_UPDATE_JOURNAL_WRITE,
                                     .sequence = status.last_sequence + 1U,
                                     .update_journal = *record};
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) return ESP_ERR_TIMEOUT;
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_get_update_journal(update_journal_record_t *out_record)
{
    if (out_record == NULL || s_request_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return select_latest_update_journal(out_record);
}

static esp_err_t enqueue_p4_ota_and_wait(flash_request_kind_t kind,
                                          uint32_t expected_bytes,
                                          const uint8_t *bytes,
                                          uint32_t bytes_count,
                                          const update_journal_record_t *journal,
                                          const esp_partition_t **out_partition)
{
    if (s_request_queue == NULL || s_p4_ota_submission_lock == NULL ||
        (bytes_count > 0U && bytes == NULL) ||
        bytes_count > FLASH_COORDINATOR_OTA_CHUNK_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_p4_ota_submission_lock, 0) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    flash_coordinator_status_t before = {0};
    flash_coordinator_get_status(&before);
    if (!before.ready || before.busy || before.pending) {
        xSemaphoreGive(s_p4_ota_submission_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (journal != NULL && !update_journal_is_valid(journal)) {
        xSemaphoreGive(s_p4_ota_submission_lock);
        return ESP_ERR_INVALID_ARG;
    }
    const uint32_t sequence = before.last_sequence + 1U;
    s_p4_ota_request = (flash_request_t){
        .kind = kind,
        .sequence = sequence,
        .ota_expected_bytes = expected_bytes,
        .ota_chunk_bytes = bytes_count,
    };
    if (journal != NULL) {
        s_p4_ota_request.update_journal = *journal;
    }
    if (before.p4_ota_partition != NULL) {
        s_p4_ota_request.ota_partition = before.p4_ota_partition;
    }
    if (bytes_count > 0U) {
        memcpy(s_p4_ota_request.ota_chunk, bytes, bytes_count);
    }
    if (xQueueSend(s_request_queue, &s_p4_ota_request, 0) != pdPASS) {
        xSemaphoreGive(s_p4_ota_submission_lock);
        return ESP_ERR_TIMEOUT;
    }
    set_busy(false, true);
    const int64_t deadline_us = esp_timer_get_time() +
                                (int64_t)FLASH_COORDINATOR_OTA_WAIT_TIMEOUT_MS * 1000LL;
    for (;;) {
        flash_coordinator_status_t after = {0};
        flash_coordinator_get_status(&after);
        if (!after.busy && !after.pending && after.last_sequence == sequence) {
            if (after.last_result == ESP_OK && out_partition != NULL) {
                *out_partition = after.p4_ota_partition;
            }
            const esp_err_t result = after.last_result;
            xSemaphoreGive(s_p4_ota_submission_lock);
            return result;
        }
        if (esp_timer_get_time() >= deadline_us || after.last_sequence > sequence) {
            xSemaphoreGive(s_p4_ota_submission_lock);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(FLASH_COORDINATOR_OTA_WAIT_POLL_MS));
    }
}

esp_err_t flash_coordinator_p4_ota_begin(uint32_t expected_bytes,
                                         const esp_partition_t **out_partition)
{
    if (out_partition == NULL || expected_bytes == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_partition = NULL;
    return enqueue_p4_ota_and_wait(FLASH_REQUEST_P4_OTA_BEGIN, expected_bytes,
                                   NULL, 0U, NULL, out_partition);
}

esp_err_t flash_coordinator_p4_ota_append(const uint8_t *bytes, uint32_t bytes_count)
{
    return enqueue_p4_ota_and_wait(FLASH_REQUEST_P4_OTA_APPEND, 0U, bytes,
                                   bytes_count, NULL, NULL);
}

esp_err_t flash_coordinator_p4_ota_finish(void)
{
    return enqueue_p4_ota_and_wait(FLASH_REQUEST_P4_OTA_FINISH, 0U, NULL, 0U, NULL, NULL);
}

esp_err_t flash_coordinator_p4_ota_abort(void)
{
    return enqueue_p4_ota_and_wait(FLASH_REQUEST_P4_OTA_ABORT, 0U, NULL, 0U, NULL, NULL);
}

esp_err_t flash_coordinator_p4_ota_activate(const update_journal_record_t *journal)
{
    return enqueue_p4_ota_and_wait(FLASH_REQUEST_P4_OTA_ACTIVATE, 0U, NULL, 0U,
                                   journal, NULL);
}

static esp_err_t enqueue_p4_ota_state_request(flash_request_kind_t kind)
{
    flash_coordinator_status_t status = {0};
    flash_coordinator_get_status(&status);
    if (s_request_queue == NULL || !status.ready || status.busy || status.pending) {
        return ESP_ERR_INVALID_STATE;
    }
    const flash_request_t request = {.kind = kind, .sequence = status.last_sequence + 1U};
    if (xQueueSend(s_request_queue, &request, 0) != pdPASS) return ESP_ERR_TIMEOUT;
    set_busy(false, true);
    return ESP_OK;
}

esp_err_t flash_coordinator_request_p4_ota_confirm(void)
{
    return enqueue_p4_ota_state_request(FLASH_REQUEST_P4_OTA_CONFIRM);
}

esp_err_t flash_coordinator_request_p4_ota_rollback(void)
{
    return enqueue_p4_ota_state_request(FLASH_REQUEST_P4_OTA_ROLLBACK);
}

void flash_coordinator_get_status(flash_coordinator_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    const int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL(&s_status_lock);
    *out_status = s_status;
    if (s_status.power_cut_window_active) {
        const int64_t remaining_us = s_power_cut_deadline_us - now_us;
        out_status->power_cut_remaining_ms = remaining_us <= 0
                                                  ? 0U
                                                  : (uint32_t)((remaining_us + 999LL) / 1000LL);
    }
    portEXIT_CRITICAL(&s_status_lock);
}
