#pragma once
#include "sound_melody.h"
typedef struct { bool general_enabled, sound_enabled, system_alerts_enabled; uint8_t melodies[SOUND_CATEGORY_COUNT]; } notification_profile_t;
static inline bool notification_profile_valid(const notification_profile_t *profile)
{
    if (profile == NULL) return false;
    for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i)
        if (!sound_choice_valid((sound_category_t)i,profile->melodies[i])) return false;
    return true;
}
static inline bool notification_profile_decode(const uint8_t *bytes,size_t size,notification_profile_t *out)
{
    if (bytes == NULL || out == NULL || (size != 3U && size != 6U) || bytes[0]>1U || bytes[1]>1U || bytes[2]>1U) return false;
    notification_profile_t profile={.general_enabled=bytes[0],.sound_enabled=bytes[1],.system_alerts_enabled=bytes[2],.melodies={1U,1U,1U}};
    if (size == 6U) for (unsigned i=0U;i<SOUND_CATEGORY_COUNT;++i) profile.melodies[i]=bytes[3U+i];
    if (!notification_profile_valid(&profile)) return false;
    *out=profile;return true;
}
_Static_assert(sizeof(notification_profile_t) == 6U,"notification v2 profile layout changed");
