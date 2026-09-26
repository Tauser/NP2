#include "update_manifest.h"

#include <stdbool.h>
#include <string.h>

enum {
    WIRE_MAGIC = 0,
    WIRE_FORMAT_VERSION = 4,
    WIRE_LENGTH = 6,
    WIRE_MANIFEST_ID = 8,
    WIRE_TARGET = 24,
    WIRE_OPERATION_FLAGS = 25,
    WIRE_HEADER_RESERVED = 26,
    WIRE_PRODUCT_ID = 28,
    WIRE_BOARD_ID = 32,
    WIRE_REVISION_MIN = 36,
    WIRE_REVISION_MAX = 38,
    WIRE_APP_VERSION = 40,
    WIRE_SECURITY_VERSION = 44,
    WIRE_IMAGE_BYTES = 48,
    WIRE_SCHEMA_MIN = 52,
    WIRE_SCHEMA_MAX = 54,
    WIRE_C6_MAJOR = 56,
    WIRE_C6_MINOR = 58,
    WIRE_C6_PATCH = 60,
    WIRE_C6_RPC = 62,
    WIRE_C6_FLAGS = 63,
    WIRE_IMAGE_SHA256 = 64,
    WIRE_TRAILER_RESERVED = 96,
};

#define UPDATE_MANIFEST_C6_SW_AGGR_FLAG UINT8_C(0x01)

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool all_zero(const uint8_t *bytes, size_t bytes_count)
{
    uint8_t value = 0;
    for (size_t index = 0; index < bytes_count; ++index) {
        value |= bytes[index];
    }
    return value == 0;
}

update_manifest_result_t update_manifest_parse(const uint8_t *wire,
                                               size_t wire_bytes,
                                               update_manifest_t *out_manifest)
{
    if (wire == NULL || out_manifest == NULL) {
        return UPDATE_MANIFEST_INVALID_ARGUMENT;
    }
    if (wire_bytes != UPDATE_MANIFEST_WIRE_BYTES ||
        read_u32_le(&wire[WIRE_MAGIC]) != UPDATE_MANIFEST_MAGIC ||
        read_u16_le(&wire[WIRE_FORMAT_VERSION]) != UPDATE_MANIFEST_FORMAT_VERSION ||
        read_u16_le(&wire[WIRE_LENGTH]) != UPDATE_MANIFEST_WIRE_BYTES ||
        !all_zero(&wire[WIRE_HEADER_RESERVED], 2U) ||
        !all_zero(&wire[WIRE_TRAILER_RESERVED], 8U)) {
        return UPDATE_MANIFEST_MALFORMED;
    }
    if (wire[WIRE_TARGET] != UPDATE_TARGET_ESP32P4) {
        return UPDATE_MANIFEST_UNSUPPORTED;
    }
    /* All update operations outside P4 application A/B are a later format. */
    if (wire[WIRE_OPERATION_FLAGS] != 0U ||
        (wire[WIRE_C6_FLAGS] & ~UPDATE_MANIFEST_C6_SW_AGGR_FLAG) != 0U) {
        return UPDATE_MANIFEST_UNSUPPORTED;
    }
    if (all_zero(&wire[WIRE_MANIFEST_ID], UPDATE_MANIFEST_ID_BYTES) ||
        all_zero(&wire[WIRE_IMAGE_SHA256], UPDATE_MANIFEST_SHA256_BYTES)) {
        return UPDATE_MANIFEST_RANGE;
    }

    update_manifest_t parsed = {0};
    memcpy(parsed.manifest_id, &wire[WIRE_MANIFEST_ID], sizeof(parsed.manifest_id));
    memcpy(parsed.image_sha256, &wire[WIRE_IMAGE_SHA256], sizeof(parsed.image_sha256));
    parsed.app_version = read_u32_le(&wire[WIRE_APP_VERSION]);
    parsed.metadata = (update_metadata_t){
        .target = UPDATE_TARGET_ESP32P4,
        .product_id = read_u32_le(&wire[WIRE_PRODUCT_ID]),
        .board_id = read_u32_le(&wire[WIRE_BOARD_ID]),
        .revision_min = read_u16_le(&wire[WIRE_REVISION_MIN]),
        .revision_max = read_u16_le(&wire[WIRE_REVISION_MAX]),
        .image_bytes = read_u32_le(&wire[WIRE_IMAGE_BYTES]),
        .security_version = read_u32_le(&wire[WIRE_SECURITY_VERSION]),
        .schema_min = read_u16_le(&wire[WIRE_SCHEMA_MIN]),
        .schema_max = read_u16_le(&wire[WIRE_SCHEMA_MAX]),
        .required_c6 = {
            .major = read_u16_le(&wire[WIRE_C6_MAJOR]),
            .minor = read_u16_le(&wire[WIRE_C6_MINOR]),
            .patch = read_u16_le(&wire[WIRE_C6_PATCH]),
            .rpc = wire[WIRE_C6_RPC],
            .sdio_sw_aggr = (wire[WIRE_C6_FLAGS] & UPDATE_MANIFEST_C6_SW_AGGR_FLAG) != 0U,
        },
    };
    if (parsed.app_version == 0U || parsed.metadata.product_id == 0U ||
        parsed.metadata.board_id == 0U || parsed.metadata.image_bytes == 0U ||
        parsed.metadata.schema_min == 0U ||
        parsed.metadata.revision_min > parsed.metadata.revision_max ||
        parsed.metadata.schema_min > parsed.metadata.schema_max) {
        return UPDATE_MANIFEST_RANGE;
    }
    *out_manifest = parsed;
    return UPDATE_MANIFEST_OK;
}

update_manifest_result_t update_manifest_signed_bytes(const uint8_t *wire,
                                                      size_t wire_bytes,
                                                      const uint8_t **out_bytes,
                                                      size_t *out_bytes_len)
{
    if (wire == NULL || out_bytes == NULL || out_bytes_len == NULL) {
        return UPDATE_MANIFEST_INVALID_ARGUMENT;
    }
    update_manifest_t ignored = {0};
    const update_manifest_result_t result = update_manifest_parse(wire, wire_bytes, &ignored);
    if (result != UPDATE_MANIFEST_OK) {
        return result;
    }
    *out_bytes = wire;
    *out_bytes_len = UPDATE_MANIFEST_WIRE_BYTES;
    return UPDATE_MANIFEST_OK;
}
