#include "weather_asset_service.h"

#include <stdio.h>
#include <string.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#include "ui/assets/np_weather_background_assets.h"
#include "ui/assets/np_weather_icon_assets.h"

#define WEATHER_ASSET_TASK_STACK_BYTES (5U * 1024U)
#define WEATHER_ASSET_TASK_PRIORITY 2U
#define WEATHER_ASSET_WORKER_PERIOD_MS 100U
#define WEATHER_SD_SETTLE_MS 1000U
#define WEATHER_SD_RETRY_DELAY_MS 1200U
#define WEATHER_SD_POWER_OFF_MS 500U
#define WEATHER_SD_BOOT_WINDOW_MS 3500U
#define WEATHER_SD_MOUNT_ATTEMPTS 2U
#define WEATHER_ASSET_SLOT_COUNT 2U
#define WEATHER_ASSET_PATH_MAX 96U
#define WEATHER_ICON_DIRECTORY "/np2/weather/icons"
#define WEATHER_ICON_HEADER_BYTES 24U
#define WEATHER_ICON_MAX_FRAME_BYTES (NP_WEATHER_ICON_SIZE * NP_WEATHER_ICON_SIZE * 4U)
#define WEATHER_ICON_BUFFER_BYTES \
    (WEATHER_ICON_MAX_FRAME_BYTES * NP_WEATHER_ICON_MAX_FRAMES)
typedef struct {
    uint8_t *pixels;
    lv_image_dsc_t frames[NP_WEATHER_ICON_MAX_FRAMES];
    const void *sources[NP_WEATHER_ICON_MAX_FRAMES];
    np_weather_icon_asset_t asset;
} weather_icon_slot_t;

static const char *const TAG = "weather_assets";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static weather_asset_service_status_t s_status = {
    .last_result = ESP_ERR_INVALID_STATE,
};
static weather_icon_slot_t s_icon_slots[WEATHER_ASSET_SLOT_COUNT];
static bool s_icons_allocated;
static bool s_started;
static bool s_request_pending;
static bool s_loading;
static bool s_requested_day;
static weather_condition_t s_requested_condition;
static uint8_t s_next_slot;
static bool s_last_attempt_valid;
static bool s_last_attempt_day;
static weather_condition_t s_last_attempt_condition;
static sd_pwr_ctrl_handle_t s_sd_power;
static TaskHandle_t s_boot_waiter;

static void release_weather_sd_power(void)
{
    if (s_sd_power == NULL) return;
    const esp_err_t result = sd_pwr_ctrl_del_on_chip_ldo(s_sd_power);
    if (result == ESP_OK) {
        s_sd_power = NULL;
    } else {
        ESP_LOGW(TAG, "microSD power-controller release failed: %s",
                 esp_err_to_name(result));
    }
}

static esp_err_t power_cycle_weather_sd(void)
{
    if (s_sd_power == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo = {.ldo_chan_id = 4};
        const esp_err_t result = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &s_sd_power);
        if (result != ESP_OK) return result;
    }
    release_weather_sd_power();
    return s_sd_power == NULL ? ESP_OK : ESP_FAIL;
}

static esp_err_t mount_weather_sd(void)
{
    if (bsp_sdcard != NULL) return ESP_OK;

    /* Match the Waveshare BSP slot and FAT policy. Own channel 4 explicitly:
     * bsp_sdcard_mount() leaks a new LDO handle on each failed attempt. */
    if (s_sd_power == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo = {.ldo_chan_id = 4};
        const esp_err_t power_result = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &s_sd_power);
        if (power_result != ESP_OK) return power_result;
    }
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.pwr_ctrl_handle = s_sd_power;
    const sdmmc_slot_config_t slot = {
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };
    const esp_vfs_fat_sdmmc_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 64U * 1024U,
    };
    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot,
                                    &config, &bsp_sdcard);
}

static void allocate_icon_slots(void)
{
    for (uint8_t index = 0U; index < WEATHER_ASSET_SLOT_COUNT; ++index) {
        weather_icon_slot_t *const slot = &s_icon_slots[index];
        slot->pixels = heap_caps_aligned_alloc(
            LV_DRAW_BUF_ALIGN, WEATHER_ICON_BUFFER_BYTES,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (slot->pixels == NULL) {
            for (uint8_t prior = 0U; prior < index; ++prior) {
                heap_caps_free(s_icon_slots[prior].pixels);
                s_icon_slots[prior].pixels = NULL;
            }
            ESP_LOGW(TAG, "PSRAM unavailable for animated weather icons; using fallback");
            return;
        }
        slot->asset.frames = slot->sources;
        slot->asset.frame_ms = 125U;
    }
    s_icons_allocated = true;
}

static uint16_t read_le16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint32_t icon_crc32(const uint8_t *bytes, size_t size)
{
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0U; index < size; ++index) {
        crc ^= bytes[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

static esp_err_t load_requested_icon(bool is_day, weather_condition_t condition,
                                     uint8_t slot_index)
{
    if (!s_icons_allocated || slot_index >= WEATHER_ASSET_SLOT_COUNT) {
        return ESP_ERR_NO_MEM;
    }
    const char *const background_name = np_weather_background_asset_file_name(is_day, condition);
    if (background_name == NULL || strncmp(background_name, "np_bg_", 6U) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char path[WEATHER_ASSET_PATH_MAX] = {0};
    const int length = snprintf(path, sizeof(path), "%s%s/np_icon_%s",
                                BSP_SD_MOUNT_POINT, WEATHER_ICON_DIRECTORY,
                                background_name + 6U);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        return ESP_ERR_INVALID_SIZE;
    }
    FILE *const file = fopen(path, "rb");
    if (file == NULL) {
        ESP_LOGW(TAG, "weather icon unavailable: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t header[WEATHER_ICON_HEADER_BYTES] = {0};
    esp_err_t result = ESP_OK;
    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "NPWI", 4U) != 0 || read_le16(header + 4U) != 1U ||
        read_le16(header + 12U) != 125U || read_le16(header + 14U) != 0U) {
        result = ESP_ERR_INVALID_RESPONSE;
    }
    const uint16_t icon_width = read_le16(header + 6U);
    const uint16_t icon_height = read_le16(header + 8U);
    if (icon_width != icon_height ||
        (icon_width != NP_WEATHER_ICON_LEGACY_SIZE &&
         icon_width != NP_WEATHER_ICON_SIZE)) {
        result = ESP_ERR_INVALID_RESPONSE;
    }
    const uint16_t count = read_le16(header + 10U);
    const uint32_t size = read_le32(header + 16U);
    const uint32_t frame_bytes = (uint32_t)icon_width * icon_height * 4U;
    if (result == ESP_OK &&
        (count == 0U || count > NP_WEATHER_ICON_MAX_FRAMES ||
         size != (uint32_t)count * frame_bytes ||
         fseek(file, 0L, SEEK_END) != 0 ||
         ftell(file) != (long)(WEATHER_ICON_HEADER_BYTES + size) ||
         fseek(file, (long)WEATHER_ICON_HEADER_BYTES, SEEK_SET) != 0)) {
        result = ESP_ERR_INVALID_SIZE;
    }
    weather_icon_slot_t *const slot = &s_icon_slots[slot_index];
    if (result == ESP_OK && fread(slot->pixels, 1U, size, file) != size) {
        result = ESP_FAIL;
    }
    (void)fclose(file);
    if (result == ESP_OK && icon_crc32(slot->pixels, size) != read_le32(header + 20U)) {
        result = ESP_ERR_INVALID_CRC;
    }
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "weather icon rejected: %s (%s)", path, esp_err_to_name(result));
        return result;
    }
    slot->asset.frame_count = count;
    for (uint16_t index = 0U; index < count; ++index) {
        slot->frames[index] = (lv_image_dsc_t){
            .header = {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_ARGB8888,
                .w = icon_width,
                .h = icon_height,
                .stride = icon_width * 4U,
            },
            .data_size = frame_bytes,
            .data = slot->pixels + (size_t)index * frame_bytes,
        };
        slot->sources[index] = &slot->frames[index];
    }
    return ESP_OK;
}

static void weather_asset_task(void *arg)
{
    (void)arg;
    /* A warm reset can leave the card powered in an indeterminate state.
     * Cycle its dedicated LDO before the first mount; app_main starts this
     * service before ESP-Hosted, so the C6 is never disturbed. */
    const esp_err_t power_cycle_result = power_cycle_weather_sd();
    if (power_cycle_result != ESP_OK) {
        ESP_LOGW(TAG, "microSD power reset unavailable: %s",
                 esp_err_to_name(power_cycle_result));
    }
    vTaskDelay(pdMS_TO_TICKS(WEATHER_SD_POWER_OFF_MS));
    vTaskDelay(pdMS_TO_TICKS(WEATHER_SD_SETTLE_MS));
    esp_err_t result = ESP_ERR_INVALID_STATE;
    for (uint8_t attempt = 0U; attempt < WEATHER_SD_MOUNT_ATTEMPTS; ++attempt) {
        result = mount_weather_sd();
        if (result == ESP_OK) break;
        if ((result != ESP_ERR_TIMEOUT && result != ESP_ERR_INVALID_RESPONSE) ||
            attempt + 1U == WEATHER_SD_MOUNT_ATTEMPTS) break;
        ESP_LOGW(TAG, "microSD init attempt %u failed: %s; retrying",
                 (unsigned int)(attempt + 1U), esp_err_to_name(result));
        release_weather_sd_power();
        if (s_sd_power != NULL) break; /* Never reacquire an occupied channel. */
        vTaskDelay(pdMS_TO_TICKS(WEATHER_SD_POWER_OFF_MS));
        vTaskDelay(pdMS_TO_TICKS(WEATHER_SD_RETRY_DELAY_MS));
    }
    const bool mounted = result == ESP_OK;
    if (!mounted) release_weather_sd_power();
    taskENTER_CRITICAL(&s_lock);
    s_status.ready = true;
    s_status.mounted = mounted;
    s_status.last_result = mounted ? ESP_OK : result;
    taskEXIT_CRITICAL(&s_lock);
    if (!mounted) {
        ESP_LOGW(TAG, "microSD unavailable; Home will use the static weather icon: %s",
                 esp_err_to_name(result));
    } else {
        /* Reserve the two icon buffers only when the optional pack is on SD. */
        FILE *const icon_probe = fopen(BSP_SD_MOUNT_POINT WEATHER_ICON_DIRECTORY
                                       "/np_icon_day_clear.bin", "rb");
        if (icon_probe != NULL) {
            (void)fclose(icon_probe);
            allocate_icon_slots();
        }
        ESP_LOGI(TAG, "microSD mounted at %s; animated weather icons %s",
                 BSP_SD_MOUNT_POINT, s_icons_allocated ? "ready" : "unavailable");
    }

    const TaskHandle_t boot_waiter = s_boot_waiter;
    if (boot_waiter != NULL) xTaskNotifyGive(boot_waiter);

    for (;;) {
        bool pending = false;
        bool is_day = false;
        weather_condition_t condition = WEATHER_CONDITION_VARIABLE;
        taskENTER_CRITICAL(&s_lock);
        if (mounted && s_request_pending) {
            pending = true;
            is_day = s_requested_day;
            condition = s_requested_condition;
            s_request_pending = false;
            s_loading = true;
        }
        taskEXIT_CRITICAL(&s_lock);

        if (pending) {
            const uint8_t slot = s_next_slot;
            const esp_err_t icon_result = load_requested_icon(is_day, condition, slot);
            if (icon_result == ESP_OK) {
                ESP_LOGI(TAG, "loaded animated weather icon: %u frames, %ux%u",
                         (unsigned int)s_icon_slots[slot].asset.frame_count,
                         (unsigned int)s_icon_slots[slot].frames[0].header.w,
                         (unsigned int)s_icon_slots[slot].frames[0].header.h);
            }
            taskENTER_CRITICAL(&s_lock);
            s_loading = false;
            s_status.pending = s_request_pending;
            s_status.last_result = icon_result;
            s_status.icon_result = icon_result;
            s_status.icon_available = icon_result == ESP_OK;
            s_status.icon_is_day = is_day;
            s_status.icon_condition = condition;
            s_status.icon_source = s_status.icon_available
                                       ? &s_icon_slots[slot].asset : NULL;
            s_last_attempt_valid = true;
            s_last_attempt_day = is_day;
            s_last_attempt_condition = condition;
            if (icon_result == ESP_OK) {
                ++s_status.generation;
                ++s_status.load_count;
                s_next_slot = (uint8_t)((slot + 1U) % WEATHER_ASSET_SLOT_COUNT);
            }
            taskEXIT_CRITICAL(&s_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(WEATHER_ASSET_WORKER_PERIOD_MS));
    }
}

esp_err_t weather_asset_service_start(void)
{
    taskENTER_CRITICAL(&s_lock);
    if (s_started) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    s_started = true;
    taskEXIT_CRITICAL(&s_lock);

    s_boot_waiter = xTaskGetCurrentTaskHandle();
    if (xTaskCreate(weather_asset_task, "np2_weather_sd", WEATHER_ASSET_TASK_STACK_BYTES, NULL,
                    WEATHER_ASSET_TASK_PRIORITY, NULL) != pdPASS) {
        s_boot_waiter = NULL;
        taskENTER_CRITICAL(&s_lock);
        s_started = false;
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    /* app_main waits only for this bounded pre-Hosted initialization window.
     * LVGL is already active, so visible UI keeps running normally. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(WEATHER_SD_BOOT_WINDOW_MS)) == 0U) {
        ESP_LOGW(TAG, "microSD boot window elapsed; continuing with Hosted startup");
    }
    return ESP_OK;
}

esp_err_t weather_asset_service_request(uint8_t hour, uint16_t weather_code)
{
    if (hour > 23U) {
        return ESP_ERR_INVALID_ARG;
    }
    const bool is_day = weather_condition_is_day(hour);
    const weather_condition_t condition = weather_condition_from_code(weather_code);

    taskENTER_CRITICAL(&s_lock);
    if (!s_started) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_INVALID_STATE;
    }
    if (s_status.icon_available && s_status.icon_is_day == is_day &&
        s_status.icon_condition == condition &&
        !s_request_pending) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_OK;
    }
    if (s_loading && s_requested_day == is_day && s_requested_condition == condition) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_OK;
    }
    if (s_last_attempt_valid && s_last_attempt_day == is_day &&
        s_last_attempt_condition == condition && !s_request_pending) {
        const esp_err_t result = s_status.last_result;
        taskEXIT_CRITICAL(&s_lock);
        return result;
    }
    if (!s_request_pending || s_requested_day != is_day || s_requested_condition != condition) {
        s_requested_day = is_day;
        s_requested_condition = condition;
        s_request_pending = true;
    }
    s_status.pending = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

void weather_asset_service_get_status(weather_asset_service_status_t *out_status)
{
    if (out_status == NULL) {
        return;
    }
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
