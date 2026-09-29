#include "user_profile.h"

#include <stddef.h>

bool user_profile_is_valid(const user_profile_t *profile)
{
    if (profile == NULL || profile->avatar_color >= USER_PROFILE_COLOR_COUNT) return false;
    bool has_letter = false;
    size_t length = 0U;
    for (; length < USER_PROFILE_NAME_BYTES && profile->name[length] != '\0'; ++length) {
        const unsigned char ch = (unsigned char)profile->name[length];
        if (ch < 0x20U || ch == 0x7fU) return false;
        if (ch >= 0x80U) {
            uint8_t continuation = ch >= 0xc2U && ch <= 0xdfU ? 1U :
                                   ch >= 0xe0U && ch <= 0xefU ? 2U :
                                   ch >= 0xf0U && ch <= 0xf4U ? 3U : 0U;
            if (continuation == 0U || length + continuation >= USER_PROFILE_NAME_BYTES)
                return false;
            for (uint8_t j = 1U; j <= continuation; ++j) {
                const unsigned char next = (unsigned char)profile->name[length + j];
                if (next < 0x80U || next > 0xbfU) return false;
            }
            const unsigned char second = (unsigned char)profile->name[length + 1U];
            if ((ch == 0xe0U && second < 0xa0U) ||
                (ch == 0xedU && second >= 0xa0U) ||
                (ch == 0xf0U && second < 0x90U) ||
                (ch == 0xf4U && second >= 0x90U)) return false;
            length += continuation;
            has_letter = true;
            continue;
        }
        if (ch != ' ') has_letter = true;
    }
    return length > 0U && length < USER_PROFILE_NAME_BYTES && has_letter &&
           profile->name[0] != ' ' && profile->name[length - 1U] != ' ';
}
