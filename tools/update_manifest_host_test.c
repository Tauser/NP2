#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "update_manifest.h"

enum { WIRE_BYTES = UPDATE_MANIFEST_WIRE_BYTES };

static void put_u16(uint8_t *wire, size_t offset, uint16_t value)
{
    wire[offset] = (uint8_t)value;
    wire[offset + 1U] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *wire, size_t offset, uint32_t value)
{
    wire[offset] = (uint8_t)value;
    wire[offset + 1U] = (uint8_t)(value >> 8);
    wire[offset + 2U] = (uint8_t)(value >> 16);
    wire[offset + 3U] = (uint8_t)(value >> 24);
}

static void make_valid(uint8_t wire[WIRE_BYTES])
{
    memset(wire, 0, WIRE_BYTES);
    put_u32(wire, 0, UPDATE_MANIFEST_MAGIC);
    put_u16(wire, 4, UPDATE_MANIFEST_FORMAT_VERSION);
    put_u16(wire, 6, WIRE_BYTES);
    wire[8] = 1;
    wire[24] = UPDATE_TARGET_ESP32P4;
    put_u32(wire, 28, 1);
    put_u32(wire, 32, 2);
    put_u16(wire, 36, 100);
    put_u16(wire, 38, 199);
    put_u32(wire, 40, 7);
    put_u32(wire, 44, 0);
    put_u32(wire, 48, 0x800000);
    put_u16(wire, 52, 1);
    put_u16(wire, 54, 1);
    put_u16(wire, 56, 3);
    put_u16(wire, 58, 0);
    put_u16(wire, 60, 6);
    wire[62] = 2;
    wire[63] = 1;
    wire[64] = 0xa5;
}

static void test_valid_and_exact_bytes(void)
{
    uint8_t wire[WIRE_BYTES];
    make_valid(wire);
    update_manifest_t parsed = {0};
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_OK);
    assert(parsed.app_version == 7 && parsed.metadata.product_id == 1 &&
           parsed.metadata.required_c6.major == 3 && parsed.metadata.required_c6.sdio_sw_aggr);
    const uint8_t *signed_bytes = NULL;
    size_t signed_length = 0;
    assert(update_manifest_signed_bytes(wire, sizeof(wire), &signed_bytes, &signed_length) == UPDATE_MANIFEST_OK);
    assert(signed_bytes == wire && signed_length == sizeof(wire));
}

static void test_rejections(void)
{
    uint8_t wire[WIRE_BYTES];
    update_manifest_t parsed = {0};
    make_valid(wire);
    assert(update_manifest_parse(NULL, sizeof(wire), &parsed) == UPDATE_MANIFEST_INVALID_ARGUMENT);
    assert(update_manifest_parse(wire, sizeof(wire) - 1U, &parsed) == UPDATE_MANIFEST_MALFORMED);
    wire[0] ^= 1U;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_MALFORMED);
    make_valid(wire); wire[24] = UPDATE_TARGET_ESP32C6;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_UNSUPPORTED);
    make_valid(wire); wire[25] = 1U;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_UNSUPPORTED);
    make_valid(wire); wire[63] = 3U;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_UNSUPPORTED);
    make_valid(wire); wire[26] = 1U;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_MALFORMED);
    make_valid(wire); wire[96] = 1U;
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_MALFORMED);
    make_valid(wire); memset(&wire[8], 0, 16U);
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_RANGE);
    make_valid(wire); memset(&wire[64], 0, 32U);
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_RANGE);
    make_valid(wire); put_u16(wire, 36, 200); put_u16(wire, 38, 100);
    assert(update_manifest_parse(wire, sizeof(wire), &parsed) == UPDATE_MANIFEST_RANGE);
}

int main(void)
{
    test_valid_and_exact_bytes();
    test_rejections();
    puts("update_manifest_host_test: all checks passed");
    return 0;
}
