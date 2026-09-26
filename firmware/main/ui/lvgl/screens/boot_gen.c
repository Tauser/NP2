/**
 * @file boot_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "boot_gen.h"
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

lv_obj_t * boot_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "boot_#");
    lv_obj_set_style_bg_color(lv_obj_0, NP_BG, 0);
    lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_0, 0, 0);

    lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_1, 100);
    lv_obj_set_y(lv_obj_1, 232);
    lv_obj_set_width(lv_obj_1, 11);
    lv_obj_set_height(lv_obj_1, 11);
    lv_obj_set_style_bg_color(lv_obj_1, NP_ACCENT, 0);
    lv_obj_set_style_bg_opa(lv_obj_1, (255 * 100 / 100), 0);
    lv_obj_set_style_radius(lv_obj_1, 2, 0);
    lv_obj_set_style_border_width(lv_obj_1, 0, 0);
    
    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "Nova");
    lv_obj_set_x(lv_label_0, 100);
    lv_obj_set_y(lv_label_0, 258);
    lv_obj_set_width(lv_label_0, 230);
    lv_obj_set_style_text_color(lv_label_0, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_0, np_font_48, 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "Panel");
    lv_obj_set_x(lv_label_1, 300);
    lv_obj_set_y(lv_label_1, 258);
    lv_obj_set_width(lv_label_1, 250);
    lv_obj_set_style_text_color(lv_label_1, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_1, np_font_48, 0);
    
    lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_2, 100);
    lv_obj_set_y(lv_obj_2, 380);
    lv_obj_set_width(lv_obj_2, 148);
    lv_obj_set_height(lv_obj_2, 5);
    lv_obj_set_style_bg_color(lv_obj_2, NP_ACCENT, 0);
    lv_obj_set_style_bg_opa(lv_obj_2, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_2, 0, 0);
    
    lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_3, 260);
    lv_obj_set_y(lv_obj_3, 380);
    lv_obj_set_width(lv_obj_3, 148);
    lv_obj_set_height(lv_obj_3, 5);
    lv_obj_set_style_bg_color(lv_obj_3, NP_ACCENT, 0);
    lv_obj_set_style_bg_opa(lv_obj_3, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_3, 0, 0);
    
    lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_4, 420);
    lv_obj_set_y(lv_obj_4, 380);
    lv_obj_set_width(lv_obj_4, 148);
    lv_obj_set_height(lv_obj_4, 5);
    lv_obj_set_style_bg_color(lv_obj_4, NP_ACCENT, 0);
    lv_obj_set_style_bg_opa(lv_obj_4, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_4, 0, 0);
    
    lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_5, 580);
    lv_obj_set_y(lv_obj_5, 380);
    lv_obj_set_width(lv_obj_5, 148);
    lv_obj_set_height(lv_obj_5, 5);
    lv_obj_set_style_bg_color(lv_obj_5, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_5, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_5, 0, 0);
    
    lv_obj_t * lv_obj_6 = lv_obj_create(lv_obj_0);
    lv_obj_set_x(lv_obj_6, 740);
    lv_obj_set_y(lv_obj_6, 380);
    lv_obj_set_width(lv_obj_6, 148);
    lv_obj_set_height(lv_obj_6, 5);
    lv_obj_set_style_bg_color(lv_obj_6, NP_HAIRLINE, 0);
    lv_obj_set_style_bg_opa(lv_obj_6, (255 * 100 / 100), 0);
    lv_obj_set_style_border_width(lv_obj_6, 0, 0);
    
    lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_2, "Display");
    lv_obj_set_x(lv_label_2, 100);
    lv_obj_set_y(lv_label_2, 391);
    lv_obj_set_width(lv_label_2, 148);
    lv_obj_set_style_text_color(lv_label_2, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_2, np_font_16, 0);
    
    lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_3, "Armazenamento");
    lv_obj_set_x(lv_label_3, 260);
    lv_obj_set_y(lv_label_3, 391);
    lv_obj_set_width(lv_label_3, 148);
    lv_obj_set_style_text_color(lv_label_3, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_3, np_font_16, 0);
    
    lv_obj_t * lv_label_4 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_4, "Rede");
    lv_obj_set_x(lv_label_4, 420);
    lv_obj_set_y(lv_label_4, 391);
    lv_obj_set_width(lv_label_4, 148);
    lv_obj_set_style_text_color(lv_label_4, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_4, np_font_16, 0);
    
    lv_obj_t * lv_label_5 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_5, "Hora");
    lv_obj_set_x(lv_label_5, 580);
    lv_obj_set_y(lv_label_5, 391);
    lv_obj_set_width(lv_label_5, 148);
    lv_obj_set_style_text_color(lv_label_5, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_5, np_font_16, 0);
    
    lv_obj_t * lv_label_6 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_6, "Dados");
    lv_obj_set_x(lv_label_6, 740);
    lv_obj_set_y(lv_label_6, 391);
    lv_obj_set_width(lv_label_6, 148);
    lv_obj_set_style_text_color(lv_label_6, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_6, np_font_16, 0);
    
    lv_obj_t * lv_label_7 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_7, &boot_status, NULL);
    lv_obj_set_x(lv_label_7, 100);
    lv_obj_set_y(lv_label_7, 435);
    lv_obj_set_width(lv_label_7, 788);
    lv_obj_set_style_text_color(lv_label_7, NP_TEXT, 0);
    lv_obj_set_style_text_font(lv_label_7, np_font_20, 0);
    
    lv_obj_t * lv_label_8 = lv_label_create(lv_obj_0);
    lv_label_bind_text(lv_label_8, &boot_detail, NULL);
    lv_obj_set_x(lv_label_8, 100);
    lv_obj_set_y(lv_label_8, 470);
    lv_obj_set_width(lv_label_8, 788);
    lv_obj_set_style_text_color(lv_label_8, NP_TEXT_3, 0);
    lv_obj_set_style_text_font(lv_label_8, np_font_16, 0);
    
    lv_obj_t * lv_label_9 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_9, "NovaOS · ESP32-P4 + C6");
    lv_obj_set_x(lv_label_9, 100);
    lv_obj_set_y(lv_label_9, 530);
    lv_obj_set_width(lv_label_9, 500);
    lv_obj_set_style_text_color(lv_label_9, NP_TEXT_DISABLED, 0);
    lv_obj_set_style_text_font(lv_label_9, np_font_16, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

