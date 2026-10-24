#include "spotify_packet_policy.h"
#include <string.h>
static uint16_t be16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8U | p[1]); }
static bool var32(const uint8_t *data,size_t size,size_t *at,uint32_t *out)
{
    uint32_t value=0;
    for(unsigned shift=0;shift<35;shift+=7){
        if(*at>=size) return false;
        const uint8_t byte=data[(*at)++];
        if(shift==28 && (byte&0xf0)) return false;
        value|=(uint32_t)(byte&127U)<<shift;
        if(!(byte&128U)){*out=value;return true;}
    }
    return false;
}
bool spotify_mercury_status(const uint8_t *header,size_t size,int32_t *status)
{
    if(!header || !status || size>SPOTIFY_PACKET_MAX) return false;
    size_t at=0;bool found=false;int32_t result=0;
    while(at<size){
        uint32_t tag=0,value=0;
        if(!var32(header,size,&at,&tag) || !(tag>>3)) return false;
        const unsigned wire=tag&7U;
        if((tag>>3)==4){
            if(found || wire!=0 || !var32(header,size,&at,&value)) return false;
            result=(value&1U) ? -(int32_t)(value>>1)-1 : (int32_t)(value>>1);
            found=true;
        }else if(wire==0){if(!var32(header,size,&at,&value)) return false;}
        else{
            size_t skip=0;
            if(wire==2){if(!var32(header,size,&at,&value)) return false;skip=value;}
            else if(wire==1) skip=8;
            else if(wire==5) skip=4;
            else return false;
            if(skip>size-at) return false;
            at+=skip;
        }
    }
    if(found) *status=result;
    return found;
}
bool spotify_packet_size(uint32_t total) { return total >= 4U && total <= SPOTIFY_PACKET_MAX; }
bool spotify_packet_mac(const uint8_t *a, const uint8_t *b, size_t size)
{
    if (!a || !b || size != 4U) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < 4U; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}
bool spotify_mercury_layout(const uint8_t *data, size_t size, spotify_mercury_layout_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!data || size < 15U || size > SPOTIFY_PACKET_MAX || be16(data) != 8U || data[10] != 1U)
        return false; /* Fragmented responses are deliberately unsupported. */
    const size_t count = be16(data + 11);
    if (!count || count > SPOTIFY_MERCURY_PARTS_MAX + 1U) return false;
    spotify_mercury_layout_t result = {0};
    for (size_t i = 2U; i < 10U; ++i) result.sequence = result.sequence << 8U | data[i];
    size_t pos = 13U;
    for (size_t i = 0; i < count; ++i) {
        if (size - pos < 2U) return false;
        const size_t length = be16(data + pos);
        pos += 2U;
        if (length > size - pos) return false;
        const spotify_packet_slice_t slice = {pos, length};
        if (i == 0U) result.header = slice;
        else result.parts[result.count++] = slice;
        pos += length;
    }
    if (pos != size) return false;
    *out = result;
    return true;
}
