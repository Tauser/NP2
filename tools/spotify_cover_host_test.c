#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spotify_cover.h"
static unsigned random_state=12345;
static uint8_t random_byte(void) {random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return (uint8_t)random_state;}
static size_t read_file(const char *path,uint8_t out[32768])
{FILE *f=fopen(path,"rb");assert(f);size_t n=fread(out,1,32768,f);assert(feof(f));fclose(f);return n;}
int main(int argc,char **argv)
{
    assert(argc==6); uint8_t bytes[32768]; uint32_t generation;
    size_t n=read_file(argv[1],bytes);
    assert(spotify_cover_decode(bytes,n,&generation));
    int8_t lease=-1; const spotify_cover_t *cover=spotify_cover_acquire(generation,&lease);
    assert(cover && cover->width==64 && cover->height==64 && lease>=0);
    assert((cover->pixels[1]&0xf8U)==0xf8U); /* red fixture */
    int8_t second=-1; assert(!spotify_cover_acquire(generation,&second));
    assert(spotify_cover_decode(bytes,n,&generation));
    const spotify_cover_t *other=spotify_cover_acquire(generation,&second); assert(other && second!=lease);
    assert(!spotify_cover_decode(bytes,n,&generation)); /* both leased, never overwrite */
    spotify_cover_release(lease); spotify_cover_release(second);
    assert(!spotify_cover_decode(bytes,n/2,&generation));
    n=read_file(argv[2],bytes); assert(!spotify_cover_decode(bytes,n,&generation)); /* 301x300 */
    n=read_file(argv[3],bytes); assert(!spotify_cover_decode(bytes,n,&generation)); /* progressive */
    n=read_file(argv[4],bytes); assert(spotify_cover_decode(bytes,n,&generation));
    cover=spotify_cover_acquire(generation,&lease);
    assert(cover && cover->width==300 && cover->height==300 && cover->stride==640 && (cover->pixels[1]&0xf8U)==0xf8U);
    assert((uintptr_t)cover->pixels%64==0);
    spotify_cover_release(lease);
    n=read_file(argv[5],bytes); assert(spotify_cover_decode(bytes,n,&generation));
    cover=spotify_cover_acquire(generation,&lease); assert(cover);
    const unsigned points[]={1,150,297,299};
    for(unsigned iy=0;iy<4;++iy) for(unsigned ix=0;ix<4;++ix) {
        unsigned x=points[ix],y=points[iy],at=y*cover->stride+x*2;
        unsigned pixel=cover->pixels[at]|((unsigned)cover->pixels[at+1]<<8);
        assert(abs((int)((pixel>>11)*255/31)-(int)(x*255/299))<12);
        assert(abs((int)(((pixel>>5)&63)*255/63)-(int)(y*255/299))<12);
        assert(abs((int)((pixel&31)*255/31)-40)<12);
    }
    spotify_cover_release(lease);
    for(unsigned sample=0;sample<10000;++sample) {
        size_t size=sample%sizeof(bytes);
        for(size_t i=0;i<size;++i)bytes[i]=random_byte();
        (void)spotify_cover_decode(bytes,size,&generation);
    }
    uint32_t cleared=spotify_cover_clear(); lease=-1; assert(!spotify_cover_acquire(cleared,&lease));
    puts("Cover: fixed 8 KiB decoder pool, <=300px, RGB565, two-slot leases, oversized/progressive/truncated JPEG rejection and 10000 malformed inputs PASS");
}
