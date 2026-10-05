#pragma once

#include <stdbool.h>
#include <stdint.h>

#define USER_PROFILE_NAME_BYTES 49U
#define USER_PROFILE_COLOR_COUNT 4U
#define USER_PROFILE_START_SCREEN_COUNT 5U

typedef enum {
    USER_PROFILE_START_HOME = 0,
    USER_PROFILE_START_WEATHER,
    USER_PROFILE_START_MARKET,
    USER_PROFILE_START_DEVICES,
    USER_PROFILE_START_POMODORO,
} user_profile_start_screen_t;

/* One local profile and its startup-page preference. No credential, location
 * or external account. The zero/default startup page remains Home. */
typedef struct {
    char name[USER_PROFILE_NAME_BYTES];
    uint8_t avatar_color;
    uint8_t initial_screen;
} user_profile_t;

bool user_profile_is_valid(const user_profile_t *profile);
