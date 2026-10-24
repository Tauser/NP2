#pragma once
#include <cstring>
#include <exception>
#include "esp_log.h"

/* Compare fixed adapter errors only. Never print foreign exception text: it
 * may contain account identifiers, response bodies or signed URLs. */
inline void spotify_lab_failure(unsigned stage,const std::exception& error)
{
    const char *const known[]={"Spotify socket failed","Spotify response invalid",
        "Spotify response memory unavailable","Spotify server authentication failed",
        "Spotify authentication invalid","Spotify protobuf invalid",
        "Spotify packet invalid","Spotify country unavailable",
        "Spotify workers unavailable","Spotify account rejected"};
    unsigned reason=0;
    for(unsigned i=0;i<sizeof(known)/sizeof(known[0]);++i)
        if(!std::strcmp(error.what(),known[i])){reason=i+1;break;}
    ESP_LOGI("np2_spotify","failure stage=%u reason=%u",stage,reason);
}
