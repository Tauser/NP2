#pragma once
#include "photo_frame_model.h"

#define PHOTO_THUMB_WIDTH 192U
#define PHOTO_THUMB_HEIGHT 112U
#define PHOTO_THUMB_BYTES (PHOTO_THUMB_WIDTH * PHOTO_THUMB_HEIGHT * 2U)

typedef struct {
    uint64_t catalog;
    uint32_t generation;
    uint8_t page;
    uint8_t ids[PHOTO_PAGE_SIZE];
    const void *images[PHOTO_PAGE_SIZE];
} photo_thumbnail_page_t;

/* Mailbox and immutable leases. No caller performs filesystem I/O. */
void photo_thumbnail_request(bool active, uint8_t page);
const photo_thumbnail_page_t *photo_thumbnail_acquire(int8_t *lease);
void photo_thumbnail_release(int8_t lease);
void photo_thumbnail_status(uint32_t *generation, uint8_t *page, bool *ready);
/* Existing SD owner only; <=16 source rows / 32 KiB per call. */
bool photo_thumbnail_step(bool mounted, uint64_t catalog);
