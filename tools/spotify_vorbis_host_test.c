#include "spotify_vorbis_guard.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { const unsigned char *data; size_t size,at,fragment; } input_t;
static size_t read_input(void *out,size_t size,size_t count,void *arg)
{
    input_t *in=arg;
    if(!size || count>SIZE_MAX/size) return 0;
    size_t bytes=size*count;
    if(bytes>in->size-in->at) bytes=in->size-in->at;
    if(bytes>in->fragment) bytes=in->fragment;
    bytes-=bytes%size;memcpy(out,in->data+in->at,bytes);in->at+=bytes;return bytes/size;
}
static int seek_input(void *arg,ogg_int64_t offset,int whence)
{
    input_t *in=arg;int64_t base=0;
    if(whence==1) base=(int64_t)in->at;else if(whence==2) base=(int64_t)in->size;else if(whence!=0) return -1;
    if(offset < -base || offset > (int64_t)in->size-base) return -1;
    in->at=(size_t)(base+offset);return 0;
}
static long tell_input(void *arg){return (long)((input_t*)arg)->at;}
static int close_input(void *arg){(void)arg;return 0;}
static ov_callbacks callbacks={read_input,seek_input,close_input,tell_input};
static unsigned char *load(const char *name,size_t *size)
{
    FILE *file=fopen(name,"rb");assert(file);fseek(file,0,SEEK_END);*size=(size_t)ftell(file);rewind(file);
    unsigned char *data=malloc(*size);assert(data && fread(data,1,*size,file)==*size);fclose(file);return data;
}
static long run(input_t input,size_t fail,size_t limit,FILE *output,spotify_vorbis_stats_t *stats)
{
    spotify_vorbis_guard_t *guard=spotify_vorbis_create(limit);assert(guard);
    spotify_vorbis_fail_at(guard,fail);
    OggVorbis_File file;int result=spotify_vorbis_open(guard,&input,&file,callbacks);
    long bytes=0;
    if(!result){
        char pcm[1024];int section=0;long count;
        do {
            count=spotify_vorbis_read(guard,&file,pcm,sizeof(pcm),&section);
            if(count>0){bytes+=count;if(output)assert(fwrite(pcm,1,count,output)==(size_t)count);}
        }while(count>0);
        if(count<0) bytes=count;
    }else bytes=result;
    spotify_vorbis_close(guard,&file);spotify_vorbis_stats(guard,stats);
    assert(!stats->bytes && !stats->blocks);
    spotify_vorbis_destroy(guard);return bytes;
}
int main(int argc,char **argv)
{
    assert(argc==5);size_t size,wrong_size;unsigned char *data=load(argv[1],&size),*wrong=load(argv[2],&wrong_size);
    input_t input={data,size,0,SIZE_MAX};spotify_vorbis_stats_t stats;
    FILE *output=fopen(argv[3],"wb");assert(output);
    const long bytes=run(input,SIZE_MAX,SPOTIFY_VORBIS_BYTES_MAX,output,&stats);fclose(output);
    assert(bytes==3L*44100*4);
    const size_t attempts=stats.attempts,peak=stats.peak_bytes,blocks=stats.peak_blocks;
    assert(peak<SPOTIFY_VORBIS_BYTES_MAX && blocks<SPOTIFY_VORBIS_BLOCKS_MAX);
    for(size_t n=1;n<=attempts;++n){
        const long result=run(input,n,SPOTIFY_VORBIS_BYTES_MAX,NULL,&stats);
        assert(result==SPOTIFY_VORBIS_RESOURCE && stats.failures==1);
    }
    assert(run(input,SIZE_MAX,1024,NULL,&stats)==SPOTIFY_VORBIS_RESOURCE);
    input.fragment=31;assert(run(input,SIZE_MAX,SPOTIFY_VORBIS_BYTES_MAX,NULL,&stats)==bytes);
    input.fragment=SIZE_MAX;
    input_t bad={wrong,wrong_size,0,SIZE_MAX};assert(run(bad,SIZE_MAX,SPOTIFY_VORBIS_BYTES_MAX,NULL,&stats)<0);
    const size_t lengths[]={1,16,128,512,1024};
    for(size_t i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i){bad=(input_t){data,lengths[i],0,SIZE_MAX};assert(run(bad,SIZE_MAX,SPOTIFY_VORBIS_BYTES_MAX,NULL,&stats)<0);}
    spotify_vorbis_guard_t *guard=spotify_vorbis_create(SPOTIFY_VORBIS_BYTES_MAX);assert(guard);
    OggVorbis_File file;assert(!spotify_vorbis_open(guard,&input,&file,callbacks));
    assert(!spotify_vorbis_seek(guard,&file,1000));
    char pcm[1024];int section;assert(spotify_vorbis_read(guard,&file,pcm,sizeof(pcm),&section)>0);
    assert(spotify_vorbis_seek(guard,&file,-1)==SPOTIFY_VORBIS_RESOURCE);
    spotify_vorbis_close(guard,&file);spotify_vorbis_destroy(guard);
    FILE *metrics=fopen(argv[4],"w");assert(metrics);
    fprintf(metrics,"{\"pcm_bytes\":%ld,\"peak_bytes\":%zu,\"peak_blocks\":%zu,\"allocation_failure_points\":%zu}\n",bytes,peak,blocks,attempts);fclose(metrics);
    printf("Tremor: PCM=%ld bytes peak=%zu blocks=%zu; all %zu allocation failures, fragmented reads, seek, format and truncation PASS\n",bytes,peak,blocks,attempts);
    free(data);free(wrong);
}
