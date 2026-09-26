/**
 * @file d_Projetos_NP2_firmware_main_ui_lvgl_gen.h
 */

#ifndef D_PROJETOS_NP2_FIRMWARE_MAIN_UI_LVGL_GEN_H
#define D_PROJETOS_NP2_FIRMWARE_MAIN_UI_LVGL_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/

#define NP_BG lv_color_hex(0x0D0F18)

#define NP_SURFACE lv_color_hex(0x141721)

#define NP_HAIRLINE lv_color_hex(0x1E2235)

#define NP_TEXT lv_color_hex(0xE8EAF2)

#define NP_TEXT_2 lv_color_hex(0xBCC0CE)

#define NP_TEXT_3 lv_color_hex(0x7A8298)

#define NP_TEXT_DISABLED lv_color_hex(0x464E64)

#define NP_ACCENT lv_color_hex(0xE8A83C)

#define NP_POSITIVE lv_color_hex(0x4ABB78)

#define NP_NEGATIVE lv_color_hex(0xD05252)

#define NP_RADIUS_SURFACE 20

#define NP_RADIUS_CONTROL 8

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t np_screen;
extern lv_style_t np_card;
extern lv_style_t np_caption;
extern lv_style_t np_body;
extern lv_style_t np_metric;

/*----------------
 * Fonts
 *----------------*/

extern lv_font_t * np_font_16;
extern lv_font_t * np_font_20;
extern lv_font_t * np_font_24;
extern lv_font_t * np_font_32;
extern lv_font_t * np_font_48;

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

extern lv_subject_t boot_status;
extern lv_subject_t boot_detail;
extern lv_subject_t home_clock;
extern lv_subject_t home_date;
extern lv_subject_t weather_temperature;
extern lv_subject_t weather_place;
extern lv_subject_t weather_summary;
extern lv_subject_t weather_humidity;
extern lv_subject_t btc_whole;
extern lv_subject_t btc_fraction;
extern lv_subject_t btc_change;
extern lv_subject_t btc_age;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void d_Projetos_NP2_firmware_main_ui_lvgl_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widget and components of this library*/
#include "screens/boot_gen.h"
#include "screens/home_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*D_PROJETOS_NP2_FIRMWARE_MAIN_UI_LVGL_GEN_H*/