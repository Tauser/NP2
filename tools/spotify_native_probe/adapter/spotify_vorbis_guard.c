#include "spotify_vorbis_guard.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#define AUDIO_ALLOC(n) heap_caps_malloc((n), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define AUDIO_REALLOC(p,n) heap_caps_realloc((p),(n), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define AUDIO_FREE(p) heap_caps_free(p)
#else
#define AUDIO_ALLOC(n) malloc(n)
#define AUDIO_REALLOC(p,n) realloc((p),(n))
#define AUDIO_FREE(p) free(p)
#endif
struct spotify_vorbis_guard {
    jmp_buf escape;
    void *pointers[SPOTIFY_VORBIS_BLOCKS_MAX];
    size_t sizes[SPOTIFY_VORBIS_BLOCKS_MAX];
    size_t limit, fail_at;
    spotify_vorbis_stats_t stats;
    bool opened;
};
static atomic_flag busy = ATOMIC_FLAG_INIT;
/* Private codec only; acquisition serializes the pointer. Callbacks must
 * never invoke another decoder or these allocation hooks themselves. */
static spotify_vorbis_guard_t *active;
static void release_all(spotify_vorbis_guard_t *g)
{
    for (size_t i=0;i<SPOTIFY_VORBIS_BLOCKS_MAX;++i) {
        if (g->pointers[i]) AUDIO_FREE(g->pointers[i]);
        g->pointers[i]=NULL;g->sizes[i]=0;
    }
    g->stats.bytes=0;g->stats.blocks=0;g->opened=false;
}
static void resource_failure(void)
{
    /* No other library/global malloc is redirected here. */
    ++active->stats.failures;
    longjmp(active->escape,1);
}
static size_t slot_for(void *p)
{
    for(size_t i=0;i<SPOTIFY_VORBIS_BLOCKS_MAX;++i) if(active->pointers[i]==p) return i;
    resource_failure();return 0;
}
static void admission(size_t old,size_t size)
{
    ++active->stats.attempts;
    if(active->stats.attempts==active->fail_at || size>active->limit ||
        active->stats.bytes-old>active->limit-size) resource_failure();
}
static void account(size_t old,size_t size)
{
    active->stats.bytes=active->stats.bytes-old+size;
    if(active->stats.bytes>active->stats.peak_bytes) active->stats.peak_bytes=active->stats.bytes;
    if(active->stats.blocks>active->stats.peak_blocks) active->stats.peak_blocks=active->stats.blocks;
}
void *spotify_ogg_malloc(size_t size)
{
    if(!active) return NULL;
    if(!size) size=1;
    admission(0,size);
    const size_t slot=slot_for(NULL);
    void *p=AUDIO_ALLOC(size);
    if(!p) resource_failure();
    active->pointers[slot]=p;active->sizes[slot]=size;++active->stats.blocks;
    account(0,size);return p;
}
void *spotify_ogg_calloc(size_t count,size_t size)
{
    if(!active) return NULL;
    if(size && count>SIZE_MAX/size) resource_failure();
    size_t total=count*size;if(!total) total=1;
    void *p=spotify_ogg_malloc(total);memset(p,0,total);return p;
}
void spotify_ogg_free(void *p)
{
    if(!p || !active) return;
    const size_t slot=slot_for(p);
    active->stats.bytes-=active->sizes[slot];--active->stats.blocks;
    active->pointers[slot]=NULL;active->sizes[slot]=0;AUDIO_FREE(p);
}
void *spotify_ogg_realloc(void *p,size_t size)
{
    if(!active) return NULL;
    if(!p) return spotify_ogg_malloc(size);
    if(!size) size=1;
    const size_t slot=slot_for(p),old=active->sizes[slot];
    admission(old,size);
    void *replacement=AUDIO_REALLOC(p,size);
    if(!replacement) resource_failure();
    active->pointers[slot]=replacement;active->sizes[slot]=size;account(old,size);return replacement;
}
static bool enter(spotify_vorbis_guard_t *g)
{
    if(!g || atomic_flag_test_and_set_explicit(&busy,memory_order_acquire)) return false;
    active=g;return true;
}
static void leave(void) { active=NULL;atomic_flag_clear_explicit(&busy,memory_order_release); }
static void invalid(spotify_vorbis_guard_t *g,OggVorbis_File *file)
{ release_all(g);memset(file,0,sizeof(*file)); }
spotify_vorbis_guard_t *spotify_vorbis_create(size_t limit)
{
    if(!limit || limit>SPOTIFY_VORBIS_BYTES_MAX) return NULL;
    spotify_vorbis_guard_t *g=AUDIO_ALLOC(sizeof(*g));
    if(g){memset(g,0,sizeof(*g));g->limit=limit;g->fail_at=SIZE_MAX;}
    return g;
}
void spotify_vorbis_destroy(spotify_vorbis_guard_t *g)
{ if(g){release_all(g);AUDIO_FREE(g);} }
int spotify_vorbis_open(spotify_vorbis_guard_t *g,void *source,OggVorbis_File *file,ov_callbacks callbacks)
{
    if(!file || !source || !callbacks.read_func || !enter(g)) return SPOTIFY_VORBIS_RESOURCE;
    release_all(g);memset(file,0,sizeof(*file));
    volatile int result=SPOTIFY_VORBIS_RESOURCE;
    if(!setjmp(g->escape)) {
        result=ov_open_callbacks(source,file,NULL,0,callbacks);
        if(!result) {
            const vorbis_info *format=ov_info(file,-1);
            if(!format || format->channels!=2 || format->rate!=44100) result=OV_EBADHEADER;
        }
    }
    if(result) invalid(g,file);else g->opened=true;
    leave();return result;
}
long spotify_vorbis_read(spotify_vorbis_guard_t *g,OggVorbis_File *file,char *pcm,int bytes,int *section)
{
    if(!file || !pcm || !section || bytes<=0 || bytes>4096 || !g || !g->opened || !enter(g))
        return SPOTIFY_VORBIS_RESOURCE;
    volatile long result=SPOTIFY_VORBIS_RESOURCE;
    if(!setjmp(g->escape)) {
        result=ov_read(file,pcm,bytes,section);
        const vorbis_info *format=ov_info(file,-1);
        if(result>bytes || (result>0 && (result%4 || !format || format->rate!=44100 || format->channels!=2)))
            result=OV_EBADHEADER;
    }
    if(result<0 && result!=OV_HOLE) invalid(g,file);
    leave();return result;
}
int spotify_vorbis_seek(spotify_vorbis_guard_t *g,OggVorbis_File *file,int64_t ms)
{
    if(!file || !g || !g->opened || ms<0 || ms>86400000LL || !enter(g)) return SPOTIFY_VORBIS_RESOURCE;
    volatile int result=SPOTIFY_VORBIS_RESOURCE;
    if(!setjmp(g->escape)) result=ov_time_seek(file,ms);
    if(result==SPOTIFY_VORBIS_RESOURCE) invalid(g,file);
    leave();return result;
}
void spotify_vorbis_close(spotify_vorbis_guard_t *g,OggVorbis_File *file)
{
    if(!file || !enter(g)) return;
    if(!setjmp(g->escape) && g->opened) ov_clear(file);
    invalid(g,file);leave();
}
void spotify_vorbis_stats(const spotify_vorbis_guard_t *g,spotify_vorbis_stats_t *out)
{ if(g && out) *out=g->stats; }
#ifdef NP_SPOTIFY_VORBIS_TEST
void spotify_vorbis_fail_at(spotify_vorbis_guard_t *g,size_t attempt) {if(g)g->fail_at=attempt;}
#endif
