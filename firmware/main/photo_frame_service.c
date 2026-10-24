#include "photo_frame_service.h"
#include "photo_thumbnail_service.h"
#include "np_memory.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define PHOTO_DIRECTORY BSP_SD_MOUNT_POINT "/np2/photos"
#define PHOTO_READ_BYTES 32768U

typedef struct {
    uint8_t *pixels;
    lv_image_dsc_t image;
    bool leased;
} photo_slot_t;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static NP2_PSRAM_BSS photo_slot_t s_slots[2];
static photo_frame_status_t s_status;
static uint8_t s_config = PHOTO_CONFIG_DEFAULT;
static bool s_visible;
static uint8_t s_interval_choice = PHOTO_INTERVAL_CHOICE_DEFAULT;
static uint64_t s_enabled_mask = UINT64_MAX;
static int8_t s_published = -1;
/* Everything below belongs exclusively to the existing SD worker. */
static uint64_t s_catalog;
static FILE *s_file;
static uint32_t s_offset, s_crc, s_expected_crc;
static int8_t s_loading = -1;
static uint8_t s_index;
static TickType_t s_scan_tick, s_swap_tick, s_retry_tick;
static bool s_scanned, s_attempted;

static void scan_catalog(void)
{
    uint64_t catalog = 0;
    DIR *dir = opendir(PHOTO_DIRECTORY);
    if (dir) {
        struct dirent *entry;
        /* Bound work even if the card contains an unrelated large directory. */
        for (unsigned i = 0; i < 256 && (entry = readdir(dir)) != NULL; ++i) {
            const char *n = entry->d_name;
            if (strlen(n) != 12 || strncmp(n, "photo", 5) || strcmp(n + 8, ".npf") ||
                n[5] < '0' || n[5] > '9' || n[6] < '0' || n[6] > '9' ||
                n[7] < '0' || n[7] > '9') continue;
            unsigned id = (unsigned)(n[5]-'0')*100U + (unsigned)(n[6]-'0')*10U + (unsigned)(n[7]-'0');
            if (id >= 1 && id <= PHOTO_MAX_FILES) catalog |= UINT64_C(1) << (id-1U);
        }
        closedir(dir);
    }
    s_catalog = catalog;
    unsigned count = 0;
    for (unsigned i = 0; i < PHOTO_MAX_FILES; ++i) count += (catalog >> i) & 1U;
    portENTER_CRITICAL(&s_lock);
    s_status.count = count;
    s_status.catalog = catalog;
    if (!count) s_status.result = ESP_ERR_NOT_FOUND;
    portEXIT_CRITICAL(&s_lock);
    s_scan_tick = xTaskGetTickCount();
    s_scanned = true;
    ESP_LOGI("photo_frame", "Catalog count=%u", count);
}

static void finish_load(esp_err_t result)
{
    if (s_file) { fclose(s_file); s_file = NULL; }
    const TickType_t now = xTaskGetTickCount();
    portENTER_CRITICAL(&s_lock);
    s_status.pending = false;
    s_status.result = result;
    if (result == ESP_OK) {
        photo_slot_t *slot = &s_slots[s_loading];
        slot->image = (lv_image_dsc_t){
            .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                       .w = PHOTO_WIDTH, .h = PHOTO_HEIGHT, .stride = PHOTO_WIDTH * 2U},
            .data_size = PHOTO_BYTES, .data = slot->pixels,
        };
        s_published = s_loading;
        s_status.available = true;
        s_status.current = s_index + 1U;
        ++s_status.generation;
        s_swap_tick = now;
    }
    portEXIT_CRITICAL(&s_lock);
    if (result == ESP_OK)
        ESP_LOGI("photo_frame", "Photo ready index=%u bytes=%u internal=%u psram=%u", s_index+1U, PHOTO_BYTES,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    else
        ESP_LOGW("photo_frame", "Photo rejected index=%u result=%ld", s_index+1U, (long)result);
    s_loading = -1;
    s_retry_tick = now;
    s_attempted = true;
}

void photo_frame_service_step(bool mounted)
{
    portENTER_CRITICAL(&s_lock);
    s_status.ready = true;
    s_status.mounted = mounted;
    const uint8_t config = s_config;
    const bool visible = s_visible;
    const uint64_t enabled_mask = s_enabled_mask;
    const uint8_t interval_choice = s_interval_choice;
    const bool available = s_status.available && s_status.current &&
        (enabled_mask & (UINT64_C(1) << (s_status.current-1U)));
    s_status.available = available;
    s_status.enabled_count = photo_mask_count(s_catalog & enabled_mask);
    const uint8_t current = s_status.current;
    portEXIT_CRITICAL(&s_lock);
    if (!mounted) {
        (void)photo_thumbnail_step(false, 0);
        if (s_loading >= 0) finish_load(ESP_ERR_INVALID_STATE);
        return; /* Mount/recovery remains owned by weather_asset_service. */
    }
    if (s_loading >= 0) {
        if (!(enabled_mask & (UINT64_C(1) << s_index))) { finish_load(ESP_ERR_INVALID_STATE); return; }
        const size_t remaining = PHOTO_BYTES - s_offset;
        const size_t size = remaining < PHOTO_READ_BYTES ? remaining : PHOTO_READ_BYTES;
        uint8_t *dest = s_slots[s_loading].pixels + s_offset;
        if (fread(dest, 1, size, s_file) != size) { finish_load(ESP_FAIL); return; }
        s_crc = photo_crc_update(s_crc, dest, size);
        s_offset += size;
        if (s_offset == PHOTO_BYTES)
            finish_load(fgetc(s_file) == EOF && !ferror(s_file) && ~s_crc == s_expected_crc
                        ? ESP_OK : ESP_ERR_INVALID_CRC);
        return;
    }
    const TickType_t now = xTaskGetTickCount();
    if (!s_scanned || now-s_scan_tick >= pdMS_TO_TICKS(60000U)) scan_catalog();
    if (photo_thumbnail_step(true, s_catalog)) return;
    const uint64_t selected = s_catalog & enabled_mask;
    if ((!(config & PHOTO_FRAME_ENABLED) && !visible) || !selected ||
        (s_attempted && now-s_retry_tick < pdMS_TO_TICKS(5000U)) ||
        (available && (!visible || now-s_swap_tick < pdMS_TO_TICKS(photo_interval_choice_ms(interval_choice))))) return;
    /* One picture is static: do not reread 1.2 MiB or allocate a second slot
     * every interval when there is nothing to rotate to. */
    if (available && current && selected == (UINT64_C(1) << (current-1U))) return;
    int8_t target = -1;
    portENTER_CRITICAL(&s_lock);
    for (int8_t i = 0; i < 2; ++i)
        if (i != s_published && !s_slots[i].leased) { target = i; break; }
    portEXIT_CRITICAL(&s_lock);
    if (target < 0) return;
    /* Lazy bounded allocation; never borrow internal SRAM on PSRAM failure. */
    if (!s_slots[target].pixels)
        s_slots[target].pixels = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, PHOTO_BYTES,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_loading = target;
    if (!s_slots[target].pixels) { finish_load(ESP_ERR_NO_MEM); return; }
    for (unsigned n = 0; n < PHOTO_MAX_FILES; ++n) {
        s_index = s_attempted ? (s_index+1U) % PHOTO_MAX_FILES : 0U;
        s_attempted = true;
        if ((selected >> s_index) & 1U) break;
    }
    char path[96];
    snprintf(path, sizeof(path), PHOTO_DIRECTORY "/photo%03u.npf", s_index+1U);
    s_file = fopen(path, "rb");
    uint8_t header[PHOTO_HEADER_BYTES];
    if (!s_file || fread(header, 1, sizeof(header), s_file) != sizeof(header) ||
        !photo_header_valid(header, &s_expected_crc)) { finish_load(ESP_ERR_INVALID_RESPONSE); return; }
    s_offset = 0;
    s_crc = UINT32_MAX;
    portENTER_CRITICAL(&s_lock);
    s_status.pending = true;
    portEXIT_CRITICAL(&s_lock);
}

void photo_frame_service_configure(uint8_t config)
{
    if (!photo_config_valid(config)) return;
    portENTER_CRITICAL(&s_lock);
    s_config = config;
    portEXIT_CRITICAL(&s_lock);
}
void photo_frame_service_set_options(uint8_t interval_choice, uint64_t enabled_mask)
{
    if (interval_choice >= PHOTO_INTERVAL_CHOICE_COUNT) return;
    portENTER_CRITICAL(&s_lock);
    s_interval_choice = interval_choice;
    s_enabled_mask = enabled_mask;
    portEXIT_CRITICAL(&s_lock);
}
void photo_frame_service_set_visible(bool visible)
{
    portENTER_CRITICAL(&s_lock); s_visible = visible; portEXIT_CRITICAL(&s_lock);
}
void photo_frame_service_get_status(photo_frame_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_lock); *out = s_status; portEXIT_CRITICAL(&s_lock);
    photo_thumbnail_status(&out->thumbnail_generation, &out->thumbnail_page, &out->thumbnail_ready);
}
const void *photo_frame_service_acquire(uint32_t *generation, int8_t *lease)
{
    if (!generation || !lease) return NULL;
    const void *image = NULL;
    portENTER_CRITICAL(&s_lock);
    if (s_published >= 0 && !s_slots[s_published].leased) {
        *lease = s_published;
        *generation = s_status.generation;
        s_slots[s_published].leased = true;
        image = &s_slots[s_published].image;
    }
    portEXIT_CRITICAL(&s_lock);
    return image;
}
void photo_frame_service_release(int8_t lease)
{
    if (lease < 0 || lease >= 2) return;
    portENTER_CRITICAL(&s_lock); s_slots[lease].leased = false; portEXIT_CRITICAL(&s_lock);
}
