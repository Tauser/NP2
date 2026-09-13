#include "cache_record.h"

uint32_t cache_record_crc32(const uint8_t *data, size_t length)
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

bool cache_record_header_is_valid(const cache_record_header_t *header, size_t max_payload_bytes)
{
    if (header == NULL || header->magic != NP2_CACHE_RECORD_MAGIC ||
        header->schema_version != NP2_CACHE_RECORD_SCHEMA_VERSION ||
        header->header_size != sizeof(*header) || header->payload_size > max_payload_bytes) {
        return false;
    }
    return header->header_crc32 ==
           cache_record_crc32((const uint8_t *)header, offsetof(cache_record_header_t, header_crc32));
}

bool cache_record_select_newest(const cache_record_header_t *slot_zero, bool slot_zero_valid,
                                const cache_record_header_t *slot_one, bool slot_one_valid,
                                cache_record_header_t *out_header)
{
    if (!slot_zero_valid && !slot_one_valid) {
        return false;
    }
    const cache_record_header_t *const selected =
        slot_one_valid && (!slot_zero_valid || slot_one->generation > slot_zero->generation)
            ? slot_one
            : slot_zero;
    if (out_header != NULL) {
        *out_header = *selected;
    }
    return true;
}
