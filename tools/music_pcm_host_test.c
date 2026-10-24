#include "music_pcm.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(uint8_t *p,int16_t value)
{ p[0]=(uint8_t)value;p[1]=(uint8_t)((uint16_t)value>>8U); }
static size_t convert(const uint8_t *in,size_t bytes,int16_t *out,size_t fragment,size_t capacity)
{
    music_pcm_converter_t state={0};size_t read=0,written=0;
    while(read<bytes) {
        size_t n=bytes-read;if(n>fragment)n=fragment;
        size_t produced=0,consumed=music_pcm_convert(&state,in+read,n,out+written,capacity,&produced);
        assert(consumed && consumed<=n && produced<=capacity && produced<=256);
        read+=consumed;written+=produced;
    }
    return written;
}
static void dsp_tests(void)
{
    uint8_t source[4096*4];int16_t whole[2048],fragmented[2048];
    for(unsigned i=0;i<4096;++i) { put(source+i*4,(int16_t)(i*17));put(source+i*4+2,(int16_t)(i*7)); }
    assert(convert(source,sizeof(source),whole,sizeof(source),256)==2048);
    assert(convert(source,sizeof(source),fragmented,4,1)==2048);
    assert(!memcmp(whole,fragmented,sizeof(whole)));
    assert(convert(source,sizeof(source),fragmented,28,3)==2048);
    assert(!memcmp(whole,fragmented,sizeof(whole)));
    for(unsigned i=0;i<4096;++i){put(source+i*4,32767);put(source+i*4+2,-32767);}
    convert(source,sizeof(source),whole,1024,256);
    for(unsigned i=0;i<2048;++i)assert(whole[i]==0);
    for(unsigned i=0;i<4096;++i){put(source+i*4,-32768);put(source+i*4+2,-32768);}
    convert(source,sizeof(source),whole,1024,256);
    for(unsigned i=200;i<2048;++i)assert(whole[i]==-32768);
    music_pcm_converter_t state={0};size_t produced=9;
    assert(!music_pcm_convert(&state,source,3,whole,256,&produced) && !produced);
    assert(!music_pcm_convert(&state,source,4,whole,0,&produced));
    assert(!music_pcm_convert(NULL,source,4,whole,1,&produced));
    puts("Music DSP: stereo downmix, full scale, odd-block state and capacity invariance PASS");
}
static void frequency_tests(void)
{
    const unsigned frames=44100;
    uint8_t *source=malloc(frames*4);int16_t *out=malloc(frames*sizeof(int16_t));assert(source&&out);
    const double frequencies[]={1000,8000,12000,16000,20000};
    for(unsigned f=0;f<5;++f) {
        for(unsigned i=0;i<frames;++i){int16_t v=(int16_t)(10000*sin(2*3.141592653589793*frequencies[f]*i/44100));put(source+4*i,v);put(source+4*i+2,v);}
        size_t n=convert(source,frames*4,out,1024,256);double energy=0;
        for(size_t i=256;i<n;++i)energy+=(double)out[i]*out[i];
        double ratio=sqrt(energy/(n-256))/7071.067811865;
        printf("Music FIR %.0f Hz: amplitude ratio %.6f\n",frequencies[f],ratio);
        if(f<2)assert(ratio>0.96 && ratio<1.02);else assert(ratio<0.01);
    }
    free(source);free(out);
}
static void queue_tests(void)
{
    int16_t storage[512],pcm[300],out[300];for(unsigned i=0;i<300;++i)pcm[i]=(int16_t)(i*97-10000);
    music_pcm_queue_t q={.samples=storage,.capacity=512,.gain=100};
    uint32_t generation=music_pcm_begin(&q);assert(generation);
    assert(music_pcm_push(&q,generation,pcm,300)==256);
    assert(music_pcm_push(&q,generation,pcm,256)==256);
    assert(!music_pcm_push(&q,generation,pcm,1) && q.count==512);
    assert(music_pcm_read(&q,out,300)==256 && !memcmp(pcm,out,512));
    assert(music_pcm_push(&q,generation,pcm,3)==3);
    assert(music_pcm_read(&q,out,256)==256 && !memcmp(pcm,out,512));
    q.paused=true;assert(!music_pcm_read(&q,out,256) && q.count==3);
    q.paused=false;q.gain=50;assert(music_pcm_read(&q,out,256)==3);
    for(unsigned i=0;i<3;++i)assert(out[i]==pcm[i]/2);
    uint32_t old=generation;generation=music_pcm_begin(&q);assert(generation!=old && !q.count);
    assert(!music_pcm_push(&q,old,pcm,3));assert(music_pcm_push(&q,generation,pcm,3)==3);
    q.gain=0;assert(music_pcm_read(&q,out,3)==3 && out[0]==0 && out[1]==0 && out[2]==0);
    music_pcm_stop(&q);assert(!q.active && !q.count && !music_pcm_push(&q,generation,pcm,3));
    q.generation=UINT32_MAX;assert(music_pcm_begin(&q)==1);
    q.samples=NULL;assert(!music_pcm_push(&q,q.generation,pcm,3));
    assert(!music_pcm_read(&q,out,3));
    puts("Music queue: bounded copies, wrap, backpressure, pause/gain, stop/track epoch, generation wrap PASS");
}
int main(void) { dsp_tests();frequency_tests();queue_tests(); }
