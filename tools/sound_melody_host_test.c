#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound_melody.h"
#include "notification_profile.h"
static int16_t large[50000],small[50000];
static size_t render(sound_category_t category,uint8_t variant,int16_t *out,size_t chunk)
{
    sound_player_t player;assert(sound_player_start(&player,category,variant));
    size_t used=0U,count;
    while ((count=sound_player_read(&player,out+used,chunk)) != 0U) { used+=count;assert(used+chunk<50000U); }
    assert(player.melody == NULL);return used;
}
int main(int argc,char **argv)
{
    assert(argc == 2);
    uint8_t legacy[]={1U,0U,1U},current[]={1U,1U,1U,0U,2U,3U};notification_profile_t profile;
    assert(notification_profile_decode(legacy,3U,&profile) && !profile.sound_enabled);
    for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) assert(profile.melodies[i] == 1U);
    assert(notification_profile_decode(current,6U,&profile) && profile.melodies[2] == 3U);
    current[5]=4U;assert(!notification_profile_decode(current,6U,&profile));
    legacy[0]=2U;assert(!notification_profile_decode(legacy,3U,&profile));
    assert(!notification_profile_decode(current,5U,&profile));
    for (unsigned category=0U;category<SOUND_CATEGORY_COUNT;++category) {
        sound_player_t silent;assert(!sound_player_start(&silent,(sound_category_t)category,0U));
        assert(sound_player_read(&silent,small,256U) == 0U);
        for (uint8_t variant=1U;variant<SOUND_VARIANT_COUNT;++variant) {
            const size_t count=render((sound_category_t)category,variant,large,1024U);
            assert(render((sound_category_t)category,variant,small,1U) == count && !memcmp(small,large,count*sizeof(large[0])));
            assert(count>SOUND_SAMPLE_RATE/5U && count<2U*SOUND_SAMPLE_RATE && large[0] == 0 && large[count-1U] == 0);
            int64_t sum=0;int peak=0;
            for (size_t i=0U;i<count;++i) { int amplitude=abs(large[i]);if (amplitude>peak) peak=amplitude;sum+=large[i]; }
            assert(peak>1000 && peak<7400 && llabs(sum/(int64_t)count)<50);
            char path[1024];snprintf(path,sizeof(path),"%s/sound-%u-%u.pcm",argv[1],category,variant);
            FILE *file=fopen(path,"wb");assert(file);assert(fwrite(large,sizeof(large[0]),count,file) == count);fclose(file);
            printf("%s: %u ms, peak %d PASS\n",sound_choice_name((sound_category_t)category,variant),(unsigned)(count*1000U/SOUND_SAMPLE_RATE),peak);
        }
    }
    assert(!sound_choice_valid(SOUND_CATEGORY_COUNT,1U));
    puts("Sound synthesis: nine original melodies, chunk invariance, bounded duration/gain, silent and legacy profile PASS");
}
