#include "photo_frame_model.h"
#include <string.h>

static uint16_t le16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8U); }
static uint32_t le32(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}
uint32_t photo_crc_update(uint32_t crc, const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return crc;
}
bool photo_header_valid(const uint8_t h[PHOTO_HEADER_BYTES], uint32_t *crc)
{
    if (!h || memcmp(h, "NPFP", 4) || le16(h + 4) != 1 ||
        le16(h + 6) != PHOTO_WIDTH || le16(h + 8) != PHOTO_HEIGHT ||
        le16(h + 10) != 1 || le32(h + 12) != 0 || le32(h + 16) != PHOTO_BYTES)
        return false;
    if (crc) *crc = le32(h + 20);
    return true;
}
