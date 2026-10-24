#include "spotify_login_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    uint8_t data[2048]={0,0,0,1,0,3,'a','b','c'};spotify_login_auth_t fields;
    assert(spotify_login_auth(data,16,&fields) && fields.type==1 && fields.offset==6 && fields.size==3);
    for(size_t n=0;n<16;++n)assert(!spotify_login_auth(data,n,&fields));
    assert(!spotify_login_auth(data,17,&fields));
    data[1]=127;assert(!spotify_login_auth(data,16,&fields));
    data[1]=0;data[5]=127;assert(!spotify_login_auth(data,16,&fields));
    data[5]=0;assert(!spotify_login_auth(data,16,&fields));
    uint8_t a[20]={0},b[20]={0};assert(spotify_login_mac(a,20,b,20));
    for(unsigned i=0;i<20;++i){b[i]=1;assert(!spotify_login_mac(a,20,b,20));b[i]=0;}
    assert(!spotify_login_mac(a,19,b,20));assert(!spotify_login_mac(NULL,20,b,20));
    assert(spotify_login_username("user",4));assert(!spotify_login_username("",0));
    assert(!spotify_login_username("ab\0cd",5));assert(!spotify_login_username("bad\n",4));
    /* Deterministic malformed-data corpus: bounds/type/length invariants must
     * hold for every accepted parse. Does not prove cryptographic validity. */
    uint32_t random=1;
    for(unsigned n=0;n<10000;++n){
        size_t size=(n%128+1)*16;
        for(size_t i=0;i<size;++i){random=random*1664525U+1013904223U;data[i]=(uint8_t)(random>>24);}
        if(spotify_login_auth(data,size,&fields))assert(fields.type<=255 && fields.size>0 && fields.size<=512 && fields.offset+fields.size<=size);
    }
    puts("Spotify Connect decrypted-blob policy: length/varint/auth bounds, checksum rejection, private username controls and malformed corpus PASS (no crypto/Connect proof)");
}
