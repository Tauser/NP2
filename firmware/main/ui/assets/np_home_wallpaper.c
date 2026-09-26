#include "np_home_wallpaper.h"

#include <stdint.h>

extern const uint8_t np_home_wallpaper_bin_start[]
    asm("_binary_np_home_wallpaper_bin_start");

const lv_image_dsc_t np_home_wallpaper = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = 1024,
        .h = 600,
        .stride = 2048,
        .reserved_2 = 0,
    },
    .data_size = 1024U * 600U * 2U,
    .data = np_home_wallpaper_bin_start,
    .reserved = NULL,
    .reserved_2 = NULL,
};
