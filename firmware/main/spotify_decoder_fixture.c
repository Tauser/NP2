#include "spotify_decoder_fixture.h"
#include "spotify_vorbis_guard.h"
#include "music_pcm.h"
#include <string.h>
#include "../../tools/spotify_native_probe/fixtures/spotify_tone.inc"
typedef struct { size_t at; } fixture_input_t;
static size_t read_fixture(void *out,size_t size,size_t count,void *arg)
{
    fixture_input_t *input=arg;
    if(!size || count>SIZE_MAX/size) return 0;
    size_t bytes=size*count;
    if(bytes>sizeof(spotify_fixture)-input->at) bytes=sizeof(spotify_fixture)-input->at;
    bytes-=bytes%size;
    memcpy(out,spotify_fixture+input->at,bytes);input->at+=bytes;return bytes/size;
}
static int seek_fixture(void *arg,ogg_int64_t offset,int whence)
{
    fixture_input_t *input=arg;int64_t base=0;
    if(whence==1) base=(int64_t)input->at;else if(whence==2) base=sizeof(spotify_fixture);else if(whence!=0) return -1;
    if(offset < -base || offset > (int64_t)sizeof(spotify_fixture)-base) return -1;
    input->at=(size_t)(base+offset);return 0;
}
static long tell_fixture(void *arg){return (long)((fixture_input_t*)arg)->at;}
static int close_fixture(void *arg){(void)arg;return 0;}
bool spotify_decoder_fixture(spotify_decoder_proof_t *out)
{
    if(!out) return false;
    memset(out,0,sizeof(*out));
    spotify_vorbis_guard_t *guard=spotify_vorbis_create(SPOTIFY_VORBIS_BYTES_MAX);
    if(!guard) return false;
    OggVorbis_File file;fixture_input_t input={0};
    const ov_callbacks callbacks={read_fixture,seek_fixture,close_fixture,tell_fixture};
    bool valid=spotify_vorbis_open(guard,&input,&file,callbacks)==0;
    music_pcm_converter_t converter={0};uint8_t pcm[1024];int16_t mono[256],previous=0;
    int section=0;long bytes=0;
    while(valid && (bytes=spotify_vorbis_read(guard,&file,(char*)pcm,sizeof(pcm),&section))>0){
        out->pcm_bytes+=(uint32_t)bytes;size_t produced=0;
        const size_t used=music_pcm_convert(&converter,pcm,(size_t)bytes,mono,256,&produced);
        if(used!=(size_t)bytes){valid=false;break;}
        out->mono_samples+=(uint32_t)produced;
        for(size_t i=0;i<produced;++i){
            const uint32_t magnitude=mono[i]<0 ? (uint32_t)-(int32_t)mono[i] : (uint32_t)mono[i];
            if(magnitude>out->peak_sample) out->peak_sample=magnitude;
            if((previous<0 && mono[i]>=0) || (previous>=0 && mono[i]<0)) ++out->zero_crossings;
            previous=mono[i];
        }
    }
    spotify_vorbis_close(guard,&file);spotify_vorbis_stats_t stats;
    spotify_vorbis_stats(guard,&stats);spotify_vorbis_destroy(guard);
    out->peak_codec_bytes=(uint32_t)stats.peak_bytes;out->peak_blocks=(uint32_t)stats.peak_blocks;
    return valid && bytes==0 && !stats.bytes && !stats.blocks && !stats.failures &&
        out->pcm_bytes==529200U && out->mono_samples==66150U &&
        out->peak_sample>400 && out->peak_sample<1000 &&
        out->zero_crossings>5700 && out->zero_crossings<6200;
}
