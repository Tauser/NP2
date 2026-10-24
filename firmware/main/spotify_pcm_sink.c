#include "spotify_pcm_sink.h"
#include <string.h>
void spotify_pcm_sink_reset(spotify_pcm_sink_t *s,uint32_t generation)
{ if(s){memset(s,0,sizeof(*s));s->generation=generation;} }
static size_t flush(spotify_pcm_sink_t *s,size_t available,spotify_pcm_push_t push,void *ctx)
{
    if(available<s->pending_ack){s->failed=true;return 0;}
    const size_t left=s->pending_count-s->pending_offset;
    const size_t used=push(ctx,s->generation,s->pending+s->pending_offset,left);
    if(used>left || used>UINT32_MAX-s->queued){s->failed=true;return 0;}
    s->pending_offset+=used;s->queued+=(uint32_t)used;
    if(s->pending_offset<s->pending_count) return 0;
    const size_t ack=s->pending_ack;
    s->pending_ack=0;s->pending_count=0;s->pending_offset=0;return ack;
}
size_t spotify_pcm_sink_write(spotify_pcm_sink_t *s,const char *id,size_t id_size,
    const uint8_t *pcm,size_t bytes,spotify_pcm_push_t push,void *ctx)
{
    if(!s || !s->generation || s->failed || !id || !id_size || id_size>=sizeof(s->id) ||
        memchr(id,0,id_size) || !pcm || !bytes || bytes%4 || !push) return 0;
    const bool same=strlen(s->id)==id_size && !memcmp(s->id,id,id_size);
    if(s->pending_ack){if(!same){s->failed=true;return 0;}return flush(s,bytes,push,ctx);}
    if(!same){
        if(s->marker_count==SPOTIFY_PCM_MARKERS) return 0;
        memcpy(s->id,id,id_size);s->id[id_size]=0;memset(&s->converter,0,sizeof(s->converter));
        spotify_pcm_marker_t *marker=&s->markers[s->marker_count++];
        memcpy(marker->id,s->id,id_size+1);marker->sample=s->queued;
    }
    s->pending_ack=music_pcm_convert(&s->converter,pcm,bytes,s->pending,MUSIC_PCM_CHUNK,&s->pending_count);
    if(!s->pending_ack){s->failed=true;return 0;}
    if(!s->pending_count){const size_t ack=s->pending_ack;s->pending_ack=0;return ack;}
    return flush(s,bytes,push,ctx);
}
bool spotify_pcm_sink_reached(spotify_pcm_sink_t *s,uint32_t consumed,char id[SPOTIFY_PCM_ID_BYTES])
{
    if(!s || !id || !s->marker_count || consumed<=s->markers[0].sample || consumed>s->queued) return false;
    memcpy(id,s->markers[0].id,SPOTIFY_PCM_ID_BYTES);--s->marker_count;
    memmove(s->markers,s->markers+1,s->marker_count*sizeof(*s->markers));return true;
}
