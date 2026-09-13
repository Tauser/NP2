#include <assert.h>
#include <string.h>

#include "cache_record.h"

static cache_record_header_t make_header(uint32_t generation, const uint8_t *payload,
                                         size_t payload_size)
{
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
    return header;
}

int main(void)
{
    static const uint8_t payload[] = "123456789";
    assert(cache_record_crc32(payload, sizeof(payload) - 1U) == UINT32_C(0xcbf43926));

    cache_record_header_t older = make_header(7U, payload, sizeof(payload) - 1U);
    cache_record_header_t newer = make_header(8U, payload, sizeof(payload) - 1U);
    assert(cache_record_header_is_valid(&older, 4096U));
    assert(cache_record_header_is_valid(&newer, 4096U));

    cache_record_header_t selected = {0};
    assert(cache_record_select_newest(&older, true, &newer, true, &selected));
    assert(selected.generation == 8U);
    assert(cache_record_select_newest(&older, true, &newer, false, &selected));
    assert(selected.generation == 7U);
    assert(!cache_record_select_newest(&older, false, &newer, false, &selected));

    newer.payload_size = 4097U;
    assert(!cache_record_header_is_valid(&newer, 4096U));
    newer = make_header(8U, payload, sizeof(payload) - 1U);
    newer.header_crc32 ^= UINT32_C(1);
    assert(!cache_record_header_is_valid(&newer, 4096U));
    return 0;
}
