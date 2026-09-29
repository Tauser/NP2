#pragma once

#include <stdbool.h>
#include <stdint.h>

#define USER_PROFILE_NAME_BYTES 49U
#define USER_PROFILE_COLOR_COUNT 4U

/* One local primary profile. No credential, location or external account. */
typedef struct {
    char name[USER_PROFILE_NAME_BYTES];
    uint8_t avatar_color;
} user_profile_t;

bool user_profile_is_valid(const user_profile_t *profile);
