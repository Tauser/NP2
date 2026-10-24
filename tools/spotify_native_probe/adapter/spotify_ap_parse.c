#include "spotify_ap_verify.h"
#include "spotify_packet_policy.h"
#include <string.h>
typedef struct { const uint8_t *data; size_t size; } bytes_t;
static bool varint(bytes_t *in, uint32_t *out)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < 5; ++i) {
        if (!in->size) return false;
        const uint8_t byte = *in->data++; --in->size;
        if (i == 4 && (byte & 0xf0U)) return false;
        value |= (uint32_t)(byte & 0x7fU) << (7U * i);
        if (!(byte & 0x80U)) { *out = value; return true; }
    }
    return false;
}
/* Find one exact length-delimited field; skip only bounded standard wire
 * types. Duplicate fields and groups are rejected, including unknown ones. */
static bool field(bytes_t in, unsigned wanted, bytes_t *out)
{
    bool found = false;
    uint32_t seen[64]; size_t fields = 0;
    while (in.size) {
        uint32_t tag, length = 0;
        if (fields == 64 || !varint(&in, &tag) || !(tag >> 3U)) return false;
        for (size_t i = 0; i < fields; ++i) if (seen[i] == tag >> 3U) return false;
        seen[fields++] = tag >> 3U;
        switch (tag & 7U) {
        case 0: if (!varint(&in, &length) || tag >> 3U == wanted) return false; continue;
        case 1: length = 8; break;
        case 2: if (!varint(&in, &length)) return false; break;
        case 5: length = 4; break;
        default: return false;
        }
        if (length > in.size) return false;
        if (tag >> 3U == wanted) {
            if ((tag & 7U) != 2U) return false;
            *out = (bytes_t){in.data, length}; found = true;
        }
        in.data += length; in.size -= length;
    }
    return found;
}
bool spotify_ap_parse(const uint8_t *packet, size_t size, spotify_ap_challenge_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!packet || size < 4U || size > SPOTIFY_PACKET_MAX) return false;
    const uint32_t wire = (uint32_t)packet[0] << 24U | (uint32_t)packet[1] << 16U |
                          (uint32_t)packet[2] << 8U | packet[3];
    if (wire != size) return false;
    bytes_t challenge = {packet + 4, size - 4}, key = {0}, signature = {0};
    /* APResponseMessage -> APChallenge -> crypto union -> DH challenge. */
    for (unsigned i = 0; i < 3; ++i) if (!field(challenge, 10, &challenge)) return false;
    if (!field(challenge, 10, &key) || key.size != 96 ||
        !field(challenge, 30, &signature) || signature.size != 256) return false;
    /* server_signature_key is a required scalar. Only key id zero exists in
     * this integration; inspect it independently, without accepting duplicates. */
    bool key_id_found = false;
    bytes_t scan = challenge;
    while (scan.size) {
        uint32_t tag, value;
        if (!varint(&scan, &tag)) return false;
        if ((tag & 7U) == 0) {
            if (!varint(&scan, &value)) return false;
            if (tag >> 3U == 20) { if (key_id_found || value) return false; key_id_found = true; }
        } else if ((tag & 7U) == 2) {
            if (!varint(&scan, &value) || value > scan.size) return false;
            scan.data += value; scan.size -= value;
        } else return false;
    }
    if (!key_id_found) return false;
    *out = (spotify_ap_challenge_t){key.data, signature.data}; return true;
}
