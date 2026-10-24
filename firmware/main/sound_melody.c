#include "sound_melody.h"
#include <string.h>
static const sound_note_t crystal[]={{1047,110,20},{1568,180,0}};
static const sound_note_t drops[]={{1319,100,55},{988,145,0}};
static const sound_note_t duet[]={{784,100,25},{1047,120,25},{1319,170,0}};
static const sound_note_t aurora[]={{523,180,35},{659,180,35},{784,240,90},{659,180,35},{784,180,35},{1047,360,0}};
static const sound_note_t bells[]={{784,250,80},{1047,300,100},{784,250,80},{1319,400,0}};
static const sound_note_t pulse[]={{659,170,65},{659,170,65},{988,260,100},{659,170,65},{988,350,0}};
static const sound_note_t welcome[]={{523,150,15},{659,150,15},{784,200,20},{1047,340,0}};
static const sound_note_t ascending[]={{392,120,15},{523,120,15},{659,150,15},{784,180,15},{1047,300,0}};
static const sound_note_t minimal[]={{659,160,40},{988,300,0}};
#define MELODY(notes,gain) {notes,(uint8_t)(sizeof(notes)/sizeof(notes[0])),gain}
static const sound_melody_t melodies[SOUND_CATEGORY_COUNT][3]={
    {MELODY(crystal,5000),MELODY(drops,4800),MELODY(duet,4700)},
    {MELODY(aurora,6200),MELODY(bells,5800),MELODY(pulse,6000)},
    {MELODY(welcome,3800),MELODY(ascending,3600),MELODY(minimal,3800)}};
static const char *const names[SOUND_CATEGORY_COUNT][SOUND_VARIANT_COUNT]={
    {"Sem som","Cristal","Gotas","Dueto"},{"Sem som","Aurora","Sinos","Pulso"},
    {"Sem som","Boas-vindas","Ascendente","Minimal"}};
bool sound_choice_valid(sound_category_t category,uint8_t variant)
{ return (unsigned)category<SOUND_CATEGORY_COUNT && variant<SOUND_VARIANT_COUNT; }
const char *sound_choice_name(sound_category_t category,uint8_t variant)
{ return sound_choice_valid(category,variant) ? names[category][variant] : "Indisponível"; }
static int32_t sine_point(unsigned index)
{
    static const int16_t quarter[]={0,3212,6393,9512,12539,15446,18204,20787,23170,25329,27245,28898,30273,31356,32137,32609,32767};
    const unsigned quadrant=(index&63U)/16U, offset=index&15U;
    const int32_t value=quarter[quadrant&1U ? 16U-offset : offset];
    return quadrant>=2U ? -value : value;
}
static int32_t wave(uint32_t phase)
{
    const unsigned index=phase>>26U, fraction=(phase>>18U)&255U;
    const int32_t a=sine_point(index),b=sine_point(index+1U);
    return a+(b-a)*(int32_t)fraction/256;
}
bool sound_player_start(sound_player_t *player,sound_category_t category,uint8_t variant)
{
    if (player == NULL) return false;
    memset(player,0,sizeof(*player));
    if (!sound_choice_valid(category,variant) || variant == 0U) return false;
    player->melody=&melodies[category][variant-1U];return true;
}
size_t sound_player_read(sound_player_t *player,int16_t *out,size_t capacity)
{
    if (player == NULL || out == NULL || player->melody == NULL) return 0U;
    size_t used=0U;
    while (used<capacity && player->note<player->melody->note_count) {
        const sound_note_t *note=&player->melody->notes[player->note];
        const uint32_t duration=(uint32_t)note->duration_ms*SOUND_SAMPLE_RATE/1000U;
        const uint32_t total=duration+(uint32_t)note->gap_ms*SOUND_SAMPLE_RATE/1000U;
        if (player->position == 0U) player->phase_step=(uint32_t)(((uint64_t)note->hz<<32U)/SOUND_SAMPLE_RATE);
        int32_t sample=0;
        if (player->position<duration) {
            const uint32_t remaining=duration-1U-player->position;
            uint32_t envelope=player->position<110U ? player->position*32767U/110U : 32767U;
            const uint32_t release=remaining<700U ? remaining*32767U/700U : 32767U;
            if (release<envelope) envelope=release;
            const uint32_t decay=32767U-player->position*20000U/duration;
            sample=(wave(player->phase)+wave(player->phase*2U)/8)*(int32_t)player->melody->amplitude/32767;
            sample=sample*(int32_t)envelope/32767;
            sample=sample*(int32_t)decay/32767;
            player->phase+=player->phase_step;
        }
        out[used++]=(int16_t)sample;
        if (++player->position>=total) { ++player->note;player->position=0U;player->phase=0U; }
    }
    if (player->note>=player->melody->note_count) player->melody=NULL;
    return used;
}
