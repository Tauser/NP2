/**
 * @file d_Projetos_NP2_firmware_main_ui_lvgl_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "d_Projetos_NP2_firmware_main_ui_lvgl_gen.h"

#if LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/*----------------
 * Translations
 *----------------*/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

/*----------------
 * Global styles
 *----------------*/

lv_style_t np_screen;
lv_style_t np_card;
lv_style_t np_caption;
lv_style_t np_body;
lv_style_t np_metric;

/*----------------
 * Fonts
 *----------------*/

lv_font_t * np_font_16;
lv_font_t * np_font_20;
lv_font_t * np_font_24;
lv_font_t * np_font_32;
lv_font_t * np_font_48;

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

lv_subject_t boot_status;
lv_subject_t boot_detail;
lv_subject_t home_clock;
lv_subject_t home_date;
lv_subject_t weather_temperature;
lv_subject_t weather_place;
lv_subject_t weather_summary;
lv_subject_t weather_humidity;
lv_subject_t btc_whole;
lv_subject_t btc_fraction;
lv_subject_t btc_change;
lv_subject_t btc_age;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void d_Projetos_NP2_firmware_main_ui_lvgl_init_gen(const char * asset_path)
{
    char buf[256];

    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat-Medium.ttf");
    np_font_16 = lv_tiny_ttf_create_file(buf, 16);
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat-Medium.ttf");
    np_font_20 = lv_tiny_ttf_create_file(buf, 20);
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat-Medium.ttf");
    np_font_24 = lv_tiny_ttf_create_file(buf, 24);
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat-Medium.ttf");
    np_font_32 = lv_tiny_ttf_create_file(buf, 32);
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat-Medium.ttf");
    np_font_48 = lv_tiny_ttf_create_file(buf, 48);

    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&np_screen);
        lv_style_set_bg_color(&np_screen, NP_BG);
        lv_style_set_bg_opa(&np_screen, (255 * 100 / 100));
        lv_style_set_border_width(&np_screen, 0);
        lv_style_set_pad_all(&np_screen, 0);

        lv_style_init(&np_card);
        lv_style_set_bg_color(&np_card, NP_SURFACE);
        lv_style_set_bg_opa(&np_card, (255 * 80 / 100));
        lv_style_set_border_color(&np_card, NP_HAIRLINE);
        lv_style_set_border_width(&np_card, 1);
        lv_style_set_radius(&np_card, NP_RADIUS_SURFACE);
        lv_style_set_pad_all(&np_card, 24);

        lv_style_init(&np_caption);
        lv_style_set_text_color(&np_caption, NP_TEXT_3);
        lv_style_set_text_font(&np_caption, np_font_16);

        lv_style_init(&np_body);
        lv_style_set_text_color(&np_body, NP_TEXT);
        lv_style_set_text_font(&np_body, np_font_20);

        lv_style_init(&np_metric);
        lv_style_set_text_color(&np_metric, NP_TEXT);
        lv_style_set_text_font(&np_metric, np_font_24);

        style_inited = true;
    }

    /*----------------
     * Fonts
     *----------------*/

/*----------------
     * Images
     *----------------*/
    /*----------------
     * Subjects
     *----------------*/
    static char boot_status_buf[UI_SUBJECT_STRING_LENGTH];
    static char boot_status_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&boot_status,
                           boot_status_buf,
                           boot_status_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "Preparando o display"
                          );
    static char boot_detail_buf[UI_SUBJECT_STRING_LENGTH];
    static char boot_detail_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&boot_detail,
                           boot_detail_buf,
                           boot_detail_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "A inicialização continua mesmo sem internet."
                          );
    static char home_clock_buf[UI_SUBJECT_STRING_LENGTH];
    static char home_clock_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&home_clock,
                           home_clock_buf,
                           home_clock_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "--:--"
                          );
    static char home_date_buf[UI_SUBJECT_STRING_LENGTH];
    static char home_date_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&home_date,
                           home_date_buf,
                           home_date_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "Aguardando sincronização de data e hora"
                          );
    static char weather_temperature_buf[UI_SUBJECT_STRING_LENGTH];
    static char weather_temperature_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&weather_temperature,
                           weather_temperature_buf,
                           weather_temperature_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "--°"
                          );
    static char weather_place_buf[UI_SUBJECT_STRING_LENGTH];
    static char weather_place_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&weather_place,
                           weather_place_buf,
                           weather_place_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "Brasília"
                          );
    static char weather_summary_buf[UI_SUBJECT_STRING_LENGTH];
    static char weather_summary_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&weather_summary,
                           weather_summary_buf,
                           weather_summary_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "Dados de clima indisponíveis"
                          );
    static char weather_humidity_buf[UI_SUBJECT_STRING_LENGTH];
    static char weather_humidity_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&weather_humidity,
                           weather_humidity_buf,
                           weather_humidity_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "--"
                          );
    static char btc_whole_buf[UI_SUBJECT_STRING_LENGTH];
    static char btc_whole_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&btc_whole,
                           btc_whole_buf,
                           btc_whole_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "--"
                          );
    static char btc_fraction_buf[UI_SUBJECT_STRING_LENGTH];
    static char btc_fraction_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&btc_fraction,
                           btc_fraction_buf,
                           btc_fraction_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           ",--"
                          );
    static char btc_change_buf[UI_SUBJECT_STRING_LENGTH];
    static char btc_change_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&btc_change,
                           btc_change_buf,
                           btc_change_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "-- em 24 h"
                          );
    static char btc_age_buf[UI_SUBJECT_STRING_LENGTH];
    static char btc_age_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&btc_age,
                           btc_age_buf,
                           btc_age_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "Aguardando primeira cotação válida"
                          );

    /*----------------
     * Translations
     *----------------*/

#if LV_USE_XML
    /* Register widgets */

    /* Register fonts */
    lv_xml_register_font(NULL, "np_font_16", np_font_16);
    lv_xml_register_font(NULL, "np_font_20", np_font_20);
    lv_xml_register_font(NULL, "np_font_24", np_font_24);
    lv_xml_register_font(NULL, "np_font_32", np_font_32);
    lv_xml_register_font(NULL, "np_font_48", np_font_48);

    /* Register subjects */
    lv_xml_register_subject(NULL, "boot_status", &boot_status);
    lv_xml_register_subject(NULL, "boot_detail", &boot_detail);
    lv_xml_register_subject(NULL, "home_clock", &home_clock);
    lv_xml_register_subject(NULL, "home_date", &home_date);
    lv_xml_register_subject(NULL, "weather_temperature", &weather_temperature);
    lv_xml_register_subject(NULL, "weather_place", &weather_place);
    lv_xml_register_subject(NULL, "weather_summary", &weather_summary);
    lv_xml_register_subject(NULL, "weather_humidity", &weather_humidity);
    lv_xml_register_subject(NULL, "btc_whole", &btc_whole);
    lv_xml_register_subject(NULL, "btc_fraction", &btc_fraction);
    lv_xml_register_subject(NULL, "btc_change", &btc_change);
    lv_xml_register_subject(NULL, "btc_age", &btc_age);

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
#endif

#if LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manaully from XML using lv_xml_create() */
#endif
}

/* Callbacks */

/**********************
 *   STATIC FUNCTIONS
 **********************/