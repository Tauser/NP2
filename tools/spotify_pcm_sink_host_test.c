#include "spotify_pcm_sink.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int16_t collected[4096];static size_t count,space;static uint32_t epoch;
static size_t push(void *ctx,uint32_t generation,const int16_t *pcm,size_t n)
{
    (void)ctx;assert(generation==epoch);
    if(n>space)n=space;
    assert(count+n<=4096);
    memcpy(collected+count,pcm,n*sizeof(*pcm));count+=n;space-=n;return n;
}
int main(void)
{
    uint8_t input[4096];for(size_t i=0;i<sizeof(input);++i)input[i]=(uint8_t)(i*17);
    spotify_pcm_sink_t sink;epoch=1;spotify_pcm_sink_reset(&sink,epoch);
    space=0;assert(!spotify_pcm_sink_write(&sink,"abc#1",5,input,1024,push,NULL));
    assert(sink.pending_ack==1024 && !count);
    for(unsigned i=0;i<20;++i)assert(!spotify_pcm_sink_write(&sink,"abc#1",5,input,1024,push,NULL));
    space=31;assert(!spotify_pcm_sink_write(&sink,"abc#1",5,input,1024,push,NULL));assert(count==31);
    space=4096;assert(spotify_pcm_sink_write(&sink,"abc#1",5,input,1024,push,NULL)==1024);assert(count==128);
    music_pcm_converter_t converter={0};int16_t reference[256];size_t produced;
    assert(music_pcm_convert(&converter,input,1024,reference,256,&produced)==1024 && produced==128);
    assert(!memcmp(reference,collected,produced*sizeof(int16_t)));
    char id[65];assert(!spotify_pcm_sink_reached(&sink,0,id));assert(spotify_pcm_sink_reached(&sink,1,id)&&!strcmp(id,"abc#1"));
    assert(!spotify_pcm_sink_reached(&sink,128,id));
    assert(spotify_pcm_sink_write(&sink,"abc#2",5,input,1024,push,NULL)==1024);
    assert(!spotify_pcm_sink_reached(&sink,128,id));assert(spotify_pcm_sink_reached(&sink,129,id)&&!strcmp(id,"abc#2"));
    for(unsigned i=0;i<4;++i){char next[8];snprintf(next,sizeof(next),"id%u",i);assert(spotify_pcm_sink_write(&sink,next,3,input,1024,push,NULL)==1024);}
    const uint32_t queued=sink.queued;assert(!spotify_pcm_sink_write(&sink,"full",4,input,1024,push,NULL));assert(sink.queued==queued);
    assert(spotify_pcm_sink_reached(&sink,sink.queued,id));assert(spotify_pcm_sink_write(&sink,"full",4,input,1024,push,NULL)==1024);
    epoch=2;spotify_pcm_sink_reset(&sink,epoch);assert(!sink.queued && !sink.marker_count && !sink.pending_ack);
    space=0;assert(!spotify_pcm_sink_write(&sink,"x",1,input,1024,push,NULL));
    assert(!spotify_pcm_sink_write(&sink,"x",1,input,512,push,NULL)&&sink.failed);
    puts("Spotify PCM sink: exact retry accounting, partial backpressure, marker timing/limits, generation and malformed retry PASS");
}
