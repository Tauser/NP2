#include "spotify_login_policy.h"
static bool integer(const uint8_t *data,size_t size,size_t *at,uint32_t *out)
{
    if(*at>=size)return false;
    unsigned low=data[(*at)++];*out=low;
    if(!(low&128U))return true;
    if(*at>=size)return false;
    *out=(low&127U)|((uint32_t)data[(*at)++]<<7U);return true;
}
bool spotify_login_auth(const uint8_t *data,size_t size,spotify_login_auth_t *out)
{
    if(!data || !out || size<16 || size>2048 || size%16)return false;
    size_t at=1;uint32_t skip,type,count;
    if(!integer(data,size,&at,&skip) || skip>size-at)return false;
    at+=skip;
    if(at>=size)return false;
    ++at;
    if(!integer(data,size,&at,&type) || type>255 || at>=size)return false;
    ++at;
    if(!integer(data,size,&at,&count) || !count || count>512 || count>size-at)return false;
    *out=(spotify_login_auth_t){.type=type,.offset=at,.size=count};return true;
}
bool spotify_login_mac(const uint8_t *expected,size_t expected_size,const uint8_t *received,size_t received_size)
{
    if(!expected || !received || expected_size!=20 || received_size!=20)return false;
    volatile unsigned different=0;
    /* Fixed-length comparison, no early exit at the first different byte. */
    for(unsigned i=0;i<20;++i)different|=expected[i]^received[i];
    return different==0;
}
bool spotify_login_username(const char *text,size_t size)
{
    if(!text || !size || size>128)return false;
    for(size_t i=0;i<size;++i)if((unsigned char)text[i]<32 || (unsigned char)text[i]==127)return false;
    return true;
}
