#include "music_pcm.h"

static const int16_t filter[MUSIC_PCM_FIR_TAPS] = {
#include "music_fir_q15.inc"
};
static int16_t sample(const uint8_t *p)
{
    const uint16_t bits=(uint16_t)((uint16_t)p[0] | (uint16_t)p[1]<<8U);
    return (int16_t)(bits>=32768U ? (int32_t)bits-65536 : bits);
}
size_t music_pcm_convert(music_pcm_converter_t *c,const uint8_t *pcm,size_t bytes,
    int16_t *out,size_t capacity,size_t *produced)
{
    if(produced)*produced=0;
    if(!c || !pcm || !out || !produced || !capacity || bytes%4U ||
        c->next>=MUSIC_PCM_FIR_TAPS || c->phase>1U)return 0;
    if(capacity>MUSIC_PCM_CHUNK)capacity=MUSIC_PCM_CHUNK;
    size_t frames=0,used=0;
    while(frames<bytes/4U && used<capacity) {
        const uint8_t *frame=pcm+frames*4U;
        c->history[c->next]=(int16_t)(((int32_t)sample(frame)+sample(frame+2))/2);
        if(++c->next==MUSIC_PCM_FIR_TAPS)c->next=0;
        c->phase^=1U;++frames;
        if(c->phase)continue;
        int64_t sum=0;
        unsigned index=c->next;
        for(unsigned k=0;k<MUSIC_PCM_FIR_TAPS;++k) {
            index=index==0 ? MUSIC_PCM_FIR_TAPS-1U : index-1U;
            sum+=(int32_t)c->history[index]*(int32_t)filter[k];
        }
        int64_t value=sum>=0 ? (sum+16384)/32768 : (sum-16384)/32768;
        if(value>32767)value=32767;
        if(value< -32768)value=-32768;
        out[used++]=(int16_t)value;
    }
    *produced=used;return frames*4U;
}
static void advance(music_pcm_queue_t *q)
{ if(++q->generation==0U)++q->generation;q->head=0;q->count=0; }
uint32_t music_pcm_begin(music_pcm_queue_t *q)
{ if(!q)return 0;advance(q);q->active=true;q->paused=false;return q->generation; }
void music_pcm_stop(music_pcm_queue_t *q)
{ if(q){advance(q);q->active=false;q->paused=false;} }
static bool ready(const music_pcm_queue_t *q)
{ return q && q->samples && q->capacity && q->capacity<=MUSIC_PCM_QUEUE_SAMPLES && q->head<q->capacity && q->count<=q->capacity; }
size_t music_pcm_push(music_pcm_queue_t *q,uint32_t generation,const int16_t *pcm,size_t count)
{
    if(!ready(q) || !pcm || !q->active || !generation || generation!=q->generation)return 0;
    if(count>MUSIC_PCM_CHUNK)count=MUSIC_PCM_CHUNK;
    if(count>q->capacity-q->count)count=q->capacity-q->count;
    for(size_t i=0;i<count;++i)q->samples[(q->head+q->count+i)%q->capacity]=pcm[i];
    q->count+=count;return count;
}
size_t music_pcm_read(music_pcm_queue_t *q,int16_t *out,size_t capacity)
{
    if(!ready(q) || !out || !q->active || q->paused || q->gain>100)return 0;
    if(capacity>MUSIC_PCM_CHUNK)capacity=MUSIC_PCM_CHUNK;
    if(capacity>q->count)capacity=q->count;
    for(size_t i=0;i<capacity;++i)
        out[i]=(int16_t)((int32_t)q->samples[(q->head+i)%q->capacity]*q->gain/100);
    q->head=(q->head+capacity)%q->capacity;q->count-=capacity;return capacity;
}
