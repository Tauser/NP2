#include <assert.h>
#include <stdio.h>
#include "../firmware/main/night_mode_policy.h"
#include "../firmware/main/device_control_profile.h"

int main(void)
{
    device_control_profile_t restored = {0};
    const uint8_t legacy[] = {60, 65};
    assert(device_control_profile_decode(legacy, sizeof(legacy), &restored));
    assert(restored.brightness_percent == 60 && restored.volume_percent == 65);
    assert(restored.night_mode_enabled == 0);
    const uint8_t current[] = {80, 40, 1};
    assert(device_control_profile_decode(current, sizeof(current), &restored));
    assert(restored.night_mode_enabled == 1);
    const uint8_t corrupt[] = {60, 65, 2};
    assert(!device_control_profile_decode(corrupt, sizeof(corrupt), &restored));
    assert(restored.brightness_percent == 80 && restored.night_mode_enabled == 1);
    assert(!device_control_profile_decode(legacy, 1, &restored));
    assert(!device_control_profile_decode(current, 4, &restored));
    const uint8_t invalid[] = {101, 65};
    assert(!device_control_profile_decode(invalid, sizeof(invalid), &restored));
    assert(night_mode_active(true, true, 22));
    assert(night_mode_active(true, true, 0));
    assert(night_mode_active(true, true, 5));
    assert(!night_mode_active(true, true, 6));
    assert(!night_mode_active(true, true, 21));
    assert(!night_mode_active(true, false, 23));
    assert(!night_mode_active(false, true, 23));
    assert(!night_mode_active(true, true, 24));
    for (uint8_t brightness = 0; brightness <= 100; ++brightness) {
        const uint8_t effective = night_mode_brightness(brightness, true);
        assert(effective <= brightness && effective <= 15);
        assert(night_mode_brightness(brightness, false) == brightness);
    }
    puts("Settings policy and legacy profile: PASS");
    return 0;
}
