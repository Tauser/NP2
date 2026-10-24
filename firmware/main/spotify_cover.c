#include "spotify_cover.h"
#include "np_memory.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "np2_tjpgd.h"

_Static_assert(offsetof(spotify_cover_t, pixels) % SPOTIFY_COVER_ALIGNMENT == 0, "pixel alignment");
_Static_assert(sizeof(spotify_cover_t) % SPOTIFY_COVER_ALIGNMENT == 0, "slot alignment");
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static NP2_PSRAM_BSS spotify_cover_t s_slots[2];
static bool s_leased[2];
static int8_t s_published = -1;
static uint32_t s_generation;
/* TJpgDec allocates only from this pool; no malloc and no dynamic fonts. */
static NP2_PSRAM_BSS uint32_t s_pool[2048];
typedef struct { const uint8_t *data; size_t size, offset; spotify_cover_t *out; } decoder_io_t;
bool spotify_cover_url_allowed(const char *url)
{
    static const char prefix[] = "https://i.scdn.co/image/";
    if (!url || strncmp(url,prefix,sizeof(prefix)-1U)) return false;
    const char *id=url+sizeof(prefix)-1U;
    return strlen(id)==40U && strspn(id,"0123456789abcdef")==40U;
}
static size_t input(JDEC *decoder, uint8_t *buffer, size_t size)
{
    decoder_io_t *io=decoder->device;
    size_t remaining=io->size-io->offset;
    if (size>remaining) size=remaining;
    if(buffer) memcpy(buffer,io->data+io->offset,size);
    io->offset+=size; return size;
}
static int output(JDEC *decoder, void *bitmap, JRECT *rect)
{
    decoder_io_t *io=decoder->device;
    if(rect->right>=io->out->width || rect->bottom>=io->out->height || rect->left>rect->right || rect->top>rect->bottom) return 0;
    const uint8_t *rgb=bitmap;
    for(unsigned y=rect->top;y<=rect->bottom;++y) for(unsigned x=rect->left;x<=rect->right;++x) {
        /* LVGL's pinned TJpgDec port emits BGR888, unlike upstream RGB888. */
        uint16_t pixel=(uint16_t)(((rgb[2]&0xf8U)<<8U)|((rgb[1]&0xfcU)<<3U)|(rgb[0]>>3U)); rgb+=3;
        size_t at=y*io->out->stride+x*2U;
        io->out->pixels[at]=(uint8_t)pixel; io->out->pixels[at+1]=(uint8_t)(pixel>>8U);
    }
    return 1;
}
bool spotify_cover_decode(const uint8_t *jpeg, size_t size, uint32_t *generation)
{
    if(!jpeg || !generation || size<4 || size>SPOTIFY_COVER_MAX_JPEG) return false;
    int8_t slot=-1;
    portENTER_CRITICAL(&s_lock);
    for(int8_t i=0;i<2;++i) if(i!=s_published && !s_leased[i]) {slot=i;break;}
    portEXIT_CRITICAL(&s_lock);
    if(slot<0) return false;
    JDEC decoder={0};
    decoder_io_t io={.data=jpeg,.size=size,.out=&s_slots[slot]};
    if(jd_prepare(&decoder,input,s_pool,sizeof(s_pool),&io)!=JDR_OK || decoder.width==0 || decoder.width>SPOTIFY_COVER_MAX_EDGE || decoder.height==0 || decoder.height>SPOTIFY_COVER_MAX_EDGE) return false;
    io.out->width=decoder.width; io.out->height=decoder.height;
    io.out->stride=(uint16_t)((decoder.width*2U+63U)&~63U);
    memset(io.out->pixels,0,io.out->stride*io.out->height);
    if(jd_decomp(&decoder,output,0)!=JDR_OK) return false;
    portENTER_CRITICAL(&s_lock);
    io.out->generation=++s_generation; s_published=slot; *generation=s_generation;
    portEXIT_CRITICAL(&s_lock);
    return true;
}
uint32_t spotify_cover_clear(void)
{ portENTER_CRITICAL(&s_lock); s_published=-1; uint32_t gen=++s_generation; portEXIT_CRITICAL(&s_lock); return gen; }
const spotify_cover_t *spotify_cover_acquire(uint32_t generation,int8_t *lease)
{
    const spotify_cover_t *out=NULL;
    portENTER_CRITICAL(&s_lock);
    if(s_published>=0 && !s_leased[s_published] && s_slots[s_published].generation==generation) {
        *lease=s_published; s_leased[s_published]=true; out=&s_slots[s_published];
    }
    portEXIT_CRITICAL(&s_lock); return out;
}
void spotify_cover_release(int8_t lease)
{ if(lease<0 || lease>1)return; portENTER_CRITICAL(&s_lock); s_leased[lease]=false; portEXIT_CRITICAL(&s_lock); }
