/* Portable integrity contract for the two-generation offline cache. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NP2_CACHE_RECORD_MAGIC UINT32_C(0x4e503243)
#define NP2_CACHE_RECORD_SCHEMA_VERSION UINT16_C(1)

typedef struct {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t header_size;
    uint32_t generation;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t header_crc32;
} cache_record_header_t;

uint32_t cache_record_crc32(const uint8_t *data, size_t length);
bool cache_record_header_is_valid(const cache_record_header_t *header, size_t max_payload_bytes);

/* Copies the newest valid slot to out_header. False means both slots are invalid. */
bool cache_record_select_newest(const cache_record_header_t *slot_zero, bool slot_zero_valid,
                                const cache_record_header_t *slot_one, bool slot_one_valid,
                                cache_record_header_t *out_header);
