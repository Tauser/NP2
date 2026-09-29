#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../firmware/main/user_profile.h"

int main(void)
{
    user_profile_t profile = {.name = "Tauser Gonçalves", .avatar_color = 2U};
    assert(user_profile_is_valid(&profile));
    profile.avatar_color = USER_PROFILE_COLOR_COUNT;
    assert(!user_profile_is_valid(&profile));
    profile.avatar_color = 0U;
    strcpy(profile.name, "   ");
    assert(!user_profile_is_valid(&profile));
    strcpy(profile.name, " Nome");
    assert(!user_profile_is_valid(&profile));
    strcpy(profile.name, "Nome ");
    assert(!user_profile_is_valid(&profile));
    memset(profile.name, 'X', sizeof(profile.name));
    assert(!user_profile_is_valid(&profile));
    memset(profile.name, 0, sizeof(profile.name));
    profile.name[0] = (char)0xc3;
    profile.name[1] = '\0';
    assert(!user_profile_is_valid(&profile));
    puts("User profile validation: PASS");
    return 0;
}
