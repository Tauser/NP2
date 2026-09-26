/**
 * @file home_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "home_gen.h"
#include "d_Projetos_NP2_firmware_main_ui_lvgl.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * home_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "home_#");
    lv_obj_set_style_bg_color(lv_obj_0, NP_BG, 0);
    lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_0, 0, 0);

    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "≡");
    lv_obj_set_x(lv_label_0, 48);
    lv_obj_set_y(lv_label_0, 22);
    lv_obj_set_width(lv_label_0, 44);
    lv_obj_set_style_text_color(lv_label_0, NP_TEXT_2, 0);
    lv_obj_set_style_text_font(lv_label_0, np_font_24, 0);
    lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "⚙");
    lv_obj_set_x(lv_label_1, 808);
    lv_obj_set_y(lv_label_1, 22);
    lv_obj_set_width(lv_label_1, 44);
    lv_obj_set_style_text_color(lv_label_1, NP_TEXT_2, 0);
    lv_obj_set_style_text_font(lv_label_1, np_font_24, 0);
    lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_2, "Wi‑Fi");
    lv_obj_set_x(lv_label_2, 856);
    lv_obj_set_y(lv_label_2, 27);
    lv_obj_set_width(lv_label_2, 48);
    lv_obj_set_style_text_color(lv_label_2, NP_POSITIVE, 0);
    lv_obj_set_style_text_font(lv_label_2, np_font_16, 0);
    lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_3, "BT");
    lv_obj_set_x(lv_label_3, 904);
    lv_obj_set_y(lv_label_3, 27);
    lv_obj_set_width(lv_label_3, 48);
    lv_obj_set_style_text_color(lv_label_3, NP_TEXT_DISABLED, 0);
    lv_obj_set_style_text_font(lv_label_3, np_font_16, 0);
    lv_obj_set_style_text_align(lv_label_3, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_4 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_4, "!");
    lv_obj_set_x(lv_label_4, 952);
    lv_obj_set_y(lv_label_4, 27);
    lv_obj_set_width(lv_label_4, 48);
    lv_obj_set_style_text_color(lv_label_4, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_4, np_font_16, 0);
    lv_obj_set_style_text_align(lv_label_4, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_1, 0);
    lv_obj_set_y(lv_obj_1, 71);
    lv_obj_set_width(lv_obj_1, 1024);
    lv_obj_set_height(lv_obj_1, 1);
    lv_obj_set_style_bg_color(lv_obj_1, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_1, 0, 0);
    
    lv_obj_t * lv_label_5 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_5, &home_clock, NULL);
    lv_obj_set_x(lv_label_5, 58);
    lv_obj_set_y(lv_label_5, 78);
    lv_obj_set_width(lv_label_5, 440);
    lv_obj_set_style_text_color(lv_label_5, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_5, np_font_48, 0);
    
    lv_obj_t * lv_label_6 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_6, &home_date, NULL);
    lv_obj_set_x(lv_label_6, 64);
    lv_obj_set_y(lv_label_6, 190);
    lv_obj_set_width(lv_label_6, 440);
    lv_obj_set_style_text_color(lv_label_6, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_6, np_font_20, 0);
    
    lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_2, 64);
    lv_obj_set_y(lv_obj_2, 231);
    lv_obj_set_width(lv_obj_2, 440);
    lv_obj_set_height(lv_obj_2, 1);
    lv_obj_set_style_bg_color(lv_obj_2, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_2, 0, 0);
    
    lv_obj_t * lv_label_7 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_7, "CLIMA AGORA");
    lv_obj_set_x(lv_label_7, 64);
    lv_obj_set_y(lv_label_7, 260);
    lv_obj_set_width(lv_label_7, 220);
    lv_obj_set_style_text_color(lv_label_7, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_7, np_font_16, 0);
    
    lv_obj_t * lv_label_8 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_8, &weather_temperature, NULL);
    lv_obj_set_x(lv_label_8, 64);
    lv_obj_set_y(lv_label_8, 298);
    lv_obj_set_width(lv_label_8, 150);
    lv_obj_set_style_text_color(lv_label_8, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_8, np_font_48, 0);
    
    lv_obj_t * lv_label_9 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_9, &weather_place, NULL);
    lv_obj_set_x(lv_label_9, 216);
    lv_obj_set_y(lv_label_9, 304);
    lv_obj_set_width(lv_label_9, 288);
    lv_obj_set_style_text_color(lv_label_9, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_9, np_font_20, 0);
    
    lv_obj_t * lv_label_10 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_10, &weather_summary, NULL);
    lv_obj_set_x(lv_label_10, 216);
    lv_obj_set_y(lv_label_10, 334);
    lv_obj_set_width(lv_label_10, 288);
    lv_obj_set_style_text_color(lv_label_10, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_10, np_font_16, 0);
    
    lv_obj_t * lv_label_11 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_11, "Umidade");
    lv_obj_set_x(lv_label_11, 64);
    lv_obj_set_y(lv_label_11, 382);
    lv_obj_set_width(lv_label_11, 120);
    lv_obj_set_style_text_color(lv_label_11, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_11, np_font_16, 0);
    
    lv_obj_t * lv_label_12 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_12, &weather_humidity, NULL);
    lv_obj_set_x(lv_label_12, 64);
    lv_obj_set_y(lv_label_12, 408);
    lv_obj_set_width(lv_label_12, 120);
    lv_obj_set_style_text_color(lv_label_12, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_12, np_font_20, 0);
    
    lv_obj_t * lv_label_13 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_13, "Vento");
    lv_obj_set_x(lv_label_13, 190);
    lv_obj_set_y(lv_label_13, 382);
    lv_obj_set_width(lv_label_13, 120);
    lv_obj_set_style_text_color(lv_label_13, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_13, np_font_16, 0);
    
    lv_obj_t * lv_label_14 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_14, "--");
    lv_obj_set_x(lv_label_14, 190);
    lv_obj_set_y(lv_label_14, 408);
    lv_obj_set_width(lv_label_14, 120);
    lv_obj_set_style_text_color(lv_label_14, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_14, np_font_20, 0);
    
    lv_obj_t * lv_label_15 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_15, "Sensação");
    lv_obj_set_x(lv_label_15, 316);
    lv_obj_set_y(lv_label_15, 382);
    lv_obj_set_width(lv_label_15, 120);
    lv_obj_set_style_text_color(lv_label_15, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_15, np_font_16, 0);
    
    lv_obj_t * lv_label_16 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_16, "--");
    lv_obj_set_x(lv_label_16, 316);
    lv_obj_set_y(lv_label_16, 408);
    lv_obj_set_width(lv_label_16, 120);
    lv_obj_set_style_text_color(lv_label_16, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_16, np_font_20, 0);
    
    lv_obj_t * lv_label_17 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_17, "UV");
    lv_obj_set_x(lv_label_17, 442);
    lv_obj_set_y(lv_label_17, 382);
    lv_obj_set_width(lv_label_17, 62);
    lv_obj_set_style_text_color(lv_label_17, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_17, np_font_16, 0);
    
    lv_obj_t * lv_label_18 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_18, "--");
    lv_obj_set_x(lv_label_18, 442);
    lv_obj_set_y(lv_label_18, 408);
    lv_obj_set_width(lv_label_18, 62);
    lv_obj_set_style_text_color(lv_label_18, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_18, np_font_20, 0);
    
    lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_3, 64);
    lv_obj_set_y(lv_obj_3, 463);
    lv_obj_set_width(lv_obj_3, 440);
    lv_obj_set_height(lv_obj_3, 1);
    lv_obj_set_style_bg_color(lv_obj_3, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_3, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_3, 0, 0);
    
    lv_obj_t * lv_label_19 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_19, "Dólar");
    lv_obj_set_x(lv_label_19, 64);
    lv_obj_set_y(lv_label_19, 480);
    lv_obj_set_width(lv_label_19, 180);
    lv_obj_set_style_text_color(lv_label_19, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_19, np_font_16, 0);
    
    lv_obj_t * lv_label_20 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_20, "Indisponível");
    lv_obj_set_x(lv_label_20, 64);
    lv_obj_set_y(lv_label_20, 504);
    lv_obj_set_width(lv_label_20, 180);
    lv_obj_set_style_text_color(lv_label_20, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_20, np_font_16, 0);
    
    lv_obj_t * lv_label_21 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_21, "Ibovespa");
    lv_obj_set_x(lv_label_21, 270);
    lv_obj_set_y(lv_label_21, 480);
    lv_obj_set_width(lv_label_21, 210);
    lv_obj_set_style_text_color(lv_label_21, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_21, np_font_16, 0);
    
    lv_obj_t * lv_label_22 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_22, "Indisponível");
    lv_obj_set_x(lv_label_22, 270);
    lv_obj_set_y(lv_label_22, 504);
    lv_obj_set_width(lv_label_22, 210);
    lv_obj_set_style_text_color(lv_label_22, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_22, np_font_16, 0);
    
    lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_4, 566);
    lv_obj_set_y(lv_obj_4, 96);
    lv_obj_set_width(lv_obj_4, 410);
    lv_obj_set_height(lv_obj_4, 442);
    lv_obj_set_style_bg_color(lv_obj_4, NP_SURFACE, 0);
    lv_obj_set_style_bg_opa(lv_obj_4, (255 * 80 / 100), 0);
    lv_obj_set_style_border_color(lv_obj_4, NP_HAIRLINE, 0);
    lv_obj_set_style_border_width(lv_obj_4, 1, 0);
    lv_obj_set_style_radius(lv_obj_4, NP_RADIUS_SURFACE, 0);
    lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_4);
    lv_obj_set_x(lv_obj_5, 0);
    lv_obj_set_y(lv_obj_5, 0);
    lv_obj_set_width(lv_obj_5, 410);
    lv_obj_set_height(lv_obj_5, 112);
    lv_obj_set_style_bg_color(lv_obj_5, NP_POSITIVE, 0);
    lv_obj_set_style_bg_opa(lv_obj_5, (255 * 10 / 100), 0);
    lv_obj_set_style_radius(lv_obj_5, NP_RADIUS_SURFACE, 0);
    lv_obj_set_style_border_width(lv_obj_5, 0, 0);
    
    lv_obj_t * lv_label_23 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_23, "Bitcoin");
    lv_obj_set_x(lv_label_23, 24);
    lv_obj_set_y(lv_label_23, 24);
    lv_obj_set_width(lv_label_23, 150);
    lv_obj_set_style_text_color(lv_label_23, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_23, np_font_20, 0);
    
    lv_obj_t * lv_label_24 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_24, "BTC/USD");
    lv_obj_set_x(lv_label_24, 130);
    lv_obj_set_y(lv_label_24, 28);
    lv_obj_set_width(lv_label_24, 100);
    lv_obj_set_style_text_color(lv_label_24, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_24, np_font_16, 0);
    
    lv_obj_t * lv_label_25 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_25, "US$");
    lv_obj_set_x(lv_label_25, 24);
    lv_obj_set_y(lv_label_25, 96);
    lv_obj_set_width(lv_label_25, 48);
    lv_obj_set_style_text_color(lv_label_25, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_25, np_font_20, 0);
    
    lv_obj_t * lv_label_26 = lv_label_create(lv_obj_4);
    lv_label_bind_text(lv_label_26, &btc_whole, NULL);
    lv_obj_set_x(lv_label_26, 78);
    lv_obj_set_y(lv_label_26, 82);
    lv_obj_set_width(lv_label_26, 242);
    lv_obj_set_style_text_color(lv_label_26, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_26, np_font_32, 0);
    
    lv_obj_t * lv_label_27 = lv_label_create(lv_obj_4);
    lv_label_bind_text(lv_label_27, &btc_fraction, NULL);
    lv_obj_set_x(lv_label_27, 318);
    lv_obj_set_y(lv_label_27, 96);
    lv_obj_set_width(lv_label_27, 68);
    lv_obj_set_style_text_color(lv_label_27, NP_TEXT_2, 0);
    lv_obj_set_style_text_font(lv_label_27, np_font_20, 0);
    
    lv_obj_t * lv_label_28 = lv_label_create(lv_obj_4);
    lv_label_bind_text(lv_label_28, &btc_change, NULL);
    lv_obj_set_x(lv_label_28, 52);
    lv_obj_set_y(lv_label_28, 144);
    lv_obj_set_width(lv_label_28, 310);
    lv_obj_set_style_text_color(lv_label_28, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_28, np_font_24, 0);
    
    lv_obj_t * lv_label_29 = lv_label_create(lv_obj_4);
    lv_label_bind_text(lv_label_29, &btc_age, NULL);
    lv_obj_set_x(lv_label_29, 24);
    lv_obj_set_y(lv_label_29, 184);
    lv_obj_set_width(lv_label_29, 362);
    lv_obj_set_style_text_color(lv_label_29, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_29, np_font_16, 0);
    
    lv_obj_t * lv_label_30 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_30, "Histórico ainda indisponível");
    lv_obj_set_x(lv_label_30, 24);
    lv_obj_set_y(lv_label_30, 254);
    lv_obj_set_width(lv_label_30, 362);
    lv_obj_set_style_text_align(lv_label_30, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lv_label_30, NP_TEXT_DISABLED, 0);
    lv_obj_set_style_text_font(lv_label_30, np_font_16, 0);
    
    lv_obj_t * lv_obj_6 = lv_obj_create(lv_obj_4);
    lv_obj_set_x(lv_obj_6, 24);
    lv_obj_set_y(lv_obj_6, 342);
    lv_obj_set_width(lv_obj_6, 362);
    lv_obj_set_height(lv_obj_6, 1);
    lv_obj_set_style_bg_color(lv_obj_6, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_6, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_6, 0, 0);
    
    lv_obj_t * lv_label_31 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_31, "Máx. 24 h");
    lv_obj_set_x(lv_label_31, 24);
    lv_obj_set_y(lv_label_31, 362);
    lv_obj_set_width(lv_label_31, 105);
    lv_obj_set_style_text_color(lv_label_31, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_31, np_font_16, 0);
    
    lv_obj_t * lv_label_32 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_32, "--");
    lv_obj_set_x(lv_label_32, 24);
    lv_obj_set_y(lv_label_32, 388);
    lv_obj_set_width(lv_label_32, 105);
    lv_obj_set_style_text_color(lv_label_32, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_32, np_font_16, 0);
    
    lv_obj_t * lv_label_33 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_33, "Mín. 24 h");
    lv_obj_set_x(lv_label_33, 149);
    lv_obj_set_y(lv_label_33, 362);
    lv_obj_set_width(lv_label_33, 105);
    lv_obj_set_style_text_color(lv_label_33, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_33, np_font_16, 0);
    
    lv_obj_t * lv_label_34 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_34, "--");
    lv_obj_set_x(lv_label_34, 149);
    lv_obj_set_y(lv_label_34, 388);
    lv_obj_set_width(lv_label_34, 105);
    lv_obj_set_style_text_color(lv_label_34, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_34, np_font_16, 0);
    
    lv_obj_t * lv_label_35 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_35, "Volume");
    lv_obj_set_x(lv_label_35, 274);
    lv_obj_set_y(lv_label_35, 362);
    lv_obj_set_width(lv_label_35, 105);
    lv_obj_set_style_text_color(lv_label_35, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_35, np_font_16, 0);
    
    lv_obj_t * lv_label_36 = lv_label_create(lv_obj_4);
    lv_label_set_text(lv_label_36, "--");
    lv_obj_set_x(lv_label_36, 274);
    lv_obj_set_y(lv_label_36, 388);
    lv_obj_set_width(lv_label_36, 105);
    lv_obj_set_style_text_color(lv_label_36, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_36, np_font_16, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

