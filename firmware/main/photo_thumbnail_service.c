#include "photo_thumbnail_service.h"
#include "np_memory.h"
#include <stdio.h>
#include <string.h>
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

typedef struct {
    photo_thumbnail_page_t page;
    lv_image_dsc_t images[PHOTO_PAGE_SIZE];
    uint8_t *pixels;
    bool leased;
} thumbnail_slot_t;
static portMUX_TYPE s_thumbnail_lock = portMUX_INITIALIZER_UNLOCKED;
static NP2_PSRAM_BSS thumbnail_slot_t s_thumbnails[2];
static NP2_PSRAM_BSS uint8_t s_thumbnail_row[PHOTO_WIDTH * 2U];
static bool s_requested;
static uint8_t s_requested_page;
static int8_t s_thumb_published = -1, s_thumb_loading = -1;
static FILE *s_thumb_file;
static unsigned s_thumb_item, s_thumb_y;
static uint32_t s_thumb_generation;
static TickType_t s_thumb_retry;
static bool s_thumb_failed;

static void close_thumbnail(void)
{
    if (s_thumb_file) fclose(s_thumb_file);
    s_thumb_file = NULL;
}

void photo_thumbnail_request(bool active, uint8_t page)
{
    portENTER_CRITICAL(&s_thumbnail_lock);
    s_requested = active;
    s_requested_page = page < PHOTO_MAX_FILES / PHOTO_PAGE_SIZE ? page : 0U;
    portEXIT_CRITICAL(&s_thumbnail_lock);
}

const photo_thumbnail_page_t *photo_thumbnail_acquire(int8_t *lease)
{
    if (!lease) return NULL;
    const photo_thumbnail_page_t *page = NULL;
    portENTER_CRITICAL(&s_thumbnail_lock);
    if (s_thumb_published >= 0 && !s_thumbnails[s_thumb_published].leased) {
        *lease = s_thumb_published;
        s_thumbnails[*lease].leased = true;
        page = &s_thumbnails[*lease].page;
    }
    portEXIT_CRITICAL(&s_thumbnail_lock);
    return page;
}

void photo_thumbnail_release(int8_t lease)
{
    if (lease < 0 || lease >= 2) return;
    portENTER_CRITICAL(&s_thumbnail_lock);
    s_thumbnails[lease].leased = false;
    portEXIT_CRITICAL(&s_thumbnail_lock);
}

void photo_thumbnail_status(uint32_t *generation, uint8_t *page, bool *ready)
{
    portENTER_CRITICAL(&s_thumbnail_lock);
    *ready = s_thumb_published >= 0;
    *generation = *ready ? s_thumbnails[s_thumb_published].page.generation : 0U;
    *page = *ready ? s_thumbnails[s_thumb_published].page.page : 0U;
    portEXIT_CRITICAL(&s_thumbnail_lock);
}

bool photo_thumbnail_step(bool mounted, uint64_t catalog)
{
    portENTER_CRITICAL(&s_thumbnail_lock);
    const bool active = s_requested;
    const uint8_t requested_page = s_requested_page;
    portEXIT_CRITICAL(&s_thumbnail_lock);
    if (!mounted || !active || !catalog) {
        close_thumbnail();
        s_thumb_loading = -1;
        return false;
    }
    if (s_thumb_loading >= 0 &&
        (s_thumbnails[s_thumb_loading].page.page != requested_page ||
         s_thumbnails[s_thumb_loading].page.catalog != catalog)) {
        close_thumbnail();
        s_thumb_loading = -1;
    }
    if (s_thumb_loading < 0) {
        if (s_thumb_failed && xTaskGetTickCount() - s_thumb_retry < pdMS_TO_TICKS(5000U)) return false;
        int8_t slot = -1;
        portENTER_CRITICAL(&s_thumbnail_lock);
        if (s_thumb_published >= 0 && s_thumbnails[s_thumb_published].page.page == requested_page &&
            s_thumbnails[s_thumb_published].page.catalog == catalog) {
            portEXIT_CRITICAL(&s_thumbnail_lock);
            return false;
        }
        for (int8_t i = 0; i < 2; ++i)
            if (i != s_thumb_published && !s_thumbnails[i].leased) { slot = i; break; }
        portEXIT_CRITICAL(&s_thumbnail_lock);
        if (slot < 0) return false;
        thumbnail_slot_t *target = &s_thumbnails[slot];
        if (!target->pixels) target->pixels = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN,
            PHOTO_PAGE_SIZE * PHOTO_THUMB_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!target->pixels) {
            s_thumb_failed = true;
            s_thumb_retry = xTaskGetTickCount();
            return false;
        }
        s_thumb_failed = false;
        target->page = (photo_thumbnail_page_t){.catalog = catalog, .page = requested_page};
        photo_page_ids(catalog, requested_page, target->page.ids);
        s_thumb_loading = slot;
        s_thumb_item = s_thumb_y = 0;
    }
    thumbnail_slot_t *target = &s_thumbnails[s_thumb_loading];
    if (s_thumb_item >= PHOTO_PAGE_SIZE) {
        portENTER_CRITICAL(&s_thumbnail_lock);
        target->page.generation = ++s_thumb_generation;
        s_thumb_published = s_thumb_loading;
        s_thumb_loading = -1;
        portEXIT_CRITICAL(&s_thumbnail_lock);
        ESP_LOGI("photo_frame", "Thumbnails ready page=%u count=%u internal=%u psram=%u",
            target->page.page, photo_mask_count(target->page.catalog),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        return false;
    }
    const unsigned index = s_thumb_item;
    if (!target->page.ids[index]) { ++s_thumb_item; return false; }
    if (!s_thumb_file) {
        char path[96];
        snprintf(path, sizeof(path), BSP_SD_MOUNT_POINT "/np2/photos/photo%03u.npf", target->page.ids[index]);
        s_thumb_file = fopen(path, "rb");
        uint8_t header[PHOTO_HEADER_BYTES];
        if (!s_thumb_file || fread(header, 1, sizeof(header), s_thumb_file) != sizeof(header) ||
            !photo_header_valid(header, NULL) || fseek(s_thumb_file, 0, SEEK_END) ||
            ftell(s_thumb_file) != (long)(PHOTO_HEADER_BYTES + PHOTO_BYTES)) {
            close_thumbnail();
            ++s_thumb_item;
            return true;
        }
        s_thumb_y = 0;
    }
    uint8_t *pixels = target->pixels + index * PHOTO_THUMB_BYTES;
    for (unsigned row = 0; row < 16U && s_thumb_y < PHOTO_THUMB_HEIGHT; ++row, ++s_thumb_y) {
        const unsigned source_y = s_thumb_y * PHOTO_HEIGHT / PHOTO_THUMB_HEIGHT;
        if (fseek(s_thumb_file, (long)(PHOTO_HEADER_BYTES + source_y * PHOTO_WIDTH * 2U), SEEK_SET) ||
            fread(s_thumbnail_row, 1, sizeof(s_thumbnail_row), s_thumb_file) != sizeof(s_thumbnail_row)) {
            close_thumbnail();
            ++s_thumb_item;
            return true;
        }
        for (unsigned x = 0; x < PHOTO_THUMB_WIDTH; ++x) {
            const unsigned source_x = x * PHOTO_WIDTH / PHOTO_THUMB_WIDTH;
            memcpy(pixels + (s_thumb_y * PHOTO_THUMB_WIDTH + x) * 2U,
                   s_thumbnail_row + source_x * 2U, 2U);
        }
    }
    if (s_thumb_y == PHOTO_THUMB_HEIGHT) {
        close_thumbnail();
        target->images[index] = (lv_image_dsc_t){
            .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                .w = PHOTO_THUMB_WIDTH, .h = PHOTO_THUMB_HEIGHT, .stride = PHOTO_THUMB_WIDTH * 2U},
            .data_size = PHOTO_THUMB_BYTES, .data = pixels,
        };
        target->page.images[index] = &target->images[index];
        ++s_thumb_item;
    }
    return true;
}
