#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef int esp_err_t;
typedef int data_refresh_domain_t;
typedef int update_p4_writer_t;
typedef int update_image_hash_session_t;
#define ESP_ERR_NO_MEM 0x101
#define MALLOC_CAP_INTERNAL 1U
#define MALLOC_CAP_8BIT 2U
#define UPDATE_IMAGE_HASH_CHUNK_MAX_BYTES 4096U
#define NP2_PROVIDER_MAX_BODY_BYTES 4096U
static bool fail_allocation;
static size_t allocations, frees, calls;
static int inner_result;
static void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    assert(count == 1U && size == 4096U);
    if (fail_allocation) return NULL;
    ++allocations;
    return calloc(count, size);
}
static void secure_zero(void *data, size_t size) { memset(data, 0, size); }
static void heap_caps_free(void *data)
{
    for (size_t i = 0; i < 4096U; ++i) assert(((uint8_t *)data)[i] == 0U);
    ++frees;
    free(data);
}
static esp_err_t refresh_product_domain_with_buffer(data_refresh_domain_t domain, int64_t start, uint8_t *body)
{
    assert(domain == 2 && start == 123);
    ++calls;
    memset(body, 0xa5, 4096U);
    return inner_result;
}
static esp_err_t stream_update_image_with_buffer(const char *url, uint32_t bytes,
    update_p4_writer_t *writer, update_image_hash_session_t *hash, uint8_t *chunk)
{
    assert(!strcmp(url, "fixture") && bytes == 123 && writer && hash);
    ++calls;
    memset(chunk, 0xa5, 4096U);
    return inner_result;
}
#include "network_buffers_under_test.inc"
int main(void)
{
    update_p4_writer_t writer = 0;
    update_image_hash_session_t hash = 0;
    const int outcomes[] = {0, -1, 0x107, 0x102};
    for (size_t i = 0; i < sizeof(outcomes) / sizeof(outcomes[0]); ++i) {
        inner_result = outcomes[i];
        assert(refresh_product_domain(2, 123) == inner_result);
        assert(stream_update_image("fixture", 123, &writer, &hash) == inner_result);
        assert(allocations == frees);
    }
    fail_allocation = true;
    size_t before = calls;
    assert(refresh_product_domain(2, 123) == ESP_ERR_NO_MEM);
    assert(stream_update_image("fixture", 123, &writer, &hash) == ESP_ERR_NO_MEM);
    assert(calls == before && allocations == frees);
    puts("Internal transient network buffers: success/error cleanup, zeroization, allocation failure PASS");
}
