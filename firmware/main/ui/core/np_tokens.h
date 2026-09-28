/*
 * NovaPanel v5 -- tokens do design system (camada 1).
 *
 * Home V2: Dark Graphite, 1024x600.
 */

#ifndef NP_TOKENS_H
#define NP_TOKENS_H

#include "lvgl.h"
#ifdef NP2_PRODUCT_FONTS
#include "ui/fonts/np_fonts.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Tela                                                                */
/* ------------------------------------------------------------------ */

#define NP_SCREEN_W 1024
#define NP_SCREEN_H 600

/* ------------------------------------------------------------------ */
/* Grade legada 12 x 8                                                 */
/* Mantida para as demais telas. A Home V2 usa geometria explicita.     */
/* ------------------------------------------------------------------ */

#define NP_MARGIN_H 64
#define NP_COL_UNIT 60
#define NP_GUTTER_H 16

#define NP_MARGIN_V 24
#define NP_ROW_UNIT 55
#define NP_GUTTER_V 16

#define NP_COL_X(col)     (NP_MARGIN_H + (col) * (NP_COL_UNIT + NP_GUTTER_H))
#define NP_COL_W(span)    ((span) * (NP_COL_UNIT + NP_GUTTER_H) - NP_GUTTER_H)
#define NP_ROW_Y(row)     (NP_MARGIN_V + (row) * (NP_ROW_UNIT + NP_GUTTER_V))
#define NP_ROW_H(span)    ((span) * (NP_ROW_UNIT + NP_GUTTER_V) - NP_GUTTER_V)

#define NP_W_FULL   NP_COL_W(12)
#define NP_W_HALF   NP_COL_W(6)
#define NP_W_MAIN   NP_COL_W(7)
#define NP_W_ASIDE  NP_COL_W(5)
#define NP_X_ASIDE  NP_COL_X(7)

/* ------------------------------------------------------------------ */
/* Medidas compartilhadas                                              */
/* ------------------------------------------------------------------ */

#define NP_HEADER_H       64
#define NP_DRAWER_W       84
#define NP_TOUCH_TARGET   48
#define NP_STATUS_DOT_SIZE 9

#define NP_SP_4  4
#define NP_SP_8  8
#define NP_SP_12 12
#define NP_SP_16 16
#define NP_SP_24 24
#define NP_SP_32 32
#define NP_SP_48 48

#define NP_RADIUS_CONTROL 12
#define NP_RADIUS_TILE    16
#define NP_RADIUS_SURFACE 20
#define NP_INPUT_H        48

/* ------------------------------------------------------------------ */
/* Dark Graphite                                                       */
/* ------------------------------------------------------------------ */

static inline lv_color_t np_c_bg(void)             { return lv_color_hex(0x0B1016); }
static inline lv_color_t np_c_surface(void)        { return lv_color_hex(0x111820); }
static inline lv_color_t np_c_surface_raised(void) { return lv_color_hex(0x18212B); }
static inline lv_color_t np_c_surface_subtle(void) { return lv_color_hex(0x151D26); }
static inline lv_color_t np_c_weather_tile(void)   { return lv_color_hex(0x1B2834); }
static inline lv_color_t np_c_weather_tile_border(void){ return lv_color_hex(0x2B3A48); }
static inline lv_color_t np_c_hairline(void)       { return lv_color_hex(0x2A3542); }

static inline lv_color_t np_c_text(void)           { return lv_color_hex(0xF2F5F8); }
static inline lv_color_t np_c_text_2(void)         { return lv_color_hex(0xA8B4C3); }
static inline lv_color_t np_c_text_3(void)         { return lv_color_hex(0x7C8998); }
static inline lv_color_t np_c_text_disabled(void)  { return lv_color_hex(0x4F5A67); }
static inline lv_color_t np_c_text_on_accent(void) { return lv_color_hex(0xF7FAFF); }

static inline lv_color_t np_c_accent(void)         { return lv_color_hex(0x4E86F7); }
static inline lv_color_t np_c_accent_active(void)  { return lv_color_hex(0x3C78EE); }
static inline lv_color_t np_c_accent_bg(void)      { return lv_color_hex(0x14213A); }

static inline lv_color_t np_c_positive(void)       { return lv_color_hex(0x36D59C); }
static inline lv_color_t np_c_positive_bg(void)    { return lv_color_hex(0x10261F); }
static inline lv_color_t np_c_negative(void)       { return lv_color_hex(0xFF676D); }
static inline lv_color_t np_c_negative_bg(void)    { return lv_color_hex(0x2B171B); }
static inline lv_color_t np_c_warning(void)        { return lv_color_hex(0xF1B847); }
static inline lv_color_t np_c_btc(void)            { return lv_color_hex(0xF7931A); }

static inline lv_color_t np_c_weather_wind(void)   { return lv_color_hex(0x4E86F7); }
static inline lv_color_t np_c_weather_water(void)  { return lv_color_hex(0x3F87FF); }
static inline lv_color_t np_c_weather_temp(void)   { return lv_color_hex(0xFF6B73); }
static inline lv_color_t np_c_weather_uv(void)     { return lv_color_hex(0xF5C542); }
static inline lv_color_t np_c_market_blue(void)    { return lv_color_hex(0x4E86F7); }

/* ------------------------------------------------------------------ */
/* Tipografia                                                          */
/* ------------------------------------------------------------------ */

#ifdef NP2_PRODUCT_FONTS
#define NP_FONT_HERO     (&ui_font_np_montserrat_92_bold_ptbr)
#define NP_FONT_BRAND    (&ui_font_np_montserrat_64_bold_ptbr)
#define NP_FONT_DISPLAY  (&ui_font_np_montserrat_48_bold_ptbr)
#define NP_FONT_TITLE    (&ui_font_np_montserrat_32_ptbr)
#define NP_FONT_LG       (&ui_font_np_montserrat_24_ptbr)
#define NP_FONT_MD       (&ui_font_np_montserrat_20_ptbr)
#define NP_FONT_SM       (&ui_font_np_monserrat_16_ptbr)
#define NP_FONT_ICON     (&ui_font_np_material_24)
#define NP_FONT_ICON_BADGE (&ui_font_np_material_48)
/* UI icons are always Material glyphs. Add a semantic constant here and its
 * codepoint to ui_font_np_material_24.c; never draw a substitute shape. */
#define NP_ICON_MENU          "\xEE\xA6\xB9"
#define NP_ICON_WIFI          "\xEE\x98\xBE"
#define NP_ICON_WIFI_LOW      "\xEE\x93\x8A"
#define NP_ICON_WIFI_MEDIUM   "\xEE\x93\x99"
#define NP_ICON_WIFI_OFF      "\xEE\x99\x88"
#define NP_ICON_BLUETOOTH     "\xEE\x86\xA7"
#define NP_ICON_NOTIFICATIONS "\xEE\x9F\xB4"
#define NP_ICON_SETTINGS      "\xEE\xA2\xB8"
#define NP_ICON_RISE          "\xEE\x97\x87"
#define NP_ICON_FALL          "\xEE\x97\x85"
#define NP_ICON_WIND          "\xEE\xBF\x98"
#define NP_ICON_HUMIDITY      "\xEF\x85\xA3"
#define NP_ICON_TEMPERATURE   "\xEE\x87\xBF"
#define NP_ICON_UV            "\xEE\xA0\x9A"
#define NP_ICON_LOCATION      "\xEE\x83\x88"
#define NP_ICON_BITCOIN       "\xEE\xAF\x85"
#define NP_ICON_DOLLAR        "\xEE\x88\xA7"
#define NP_ICON_MARKET        "\xEE\xBE\x92"
#define NP_ICON_HOME          "\xEE\xA2\x8A"
#define NP_ICON_WEATHER       "\xEF\x85\xB2"
#define NP_ICON_CALENDAR      "\xEE\xA4\xB5"
#define NP_ICON_DISPLAY       "\xEE\x8C\x8C" /* desktop_windows U+E30C */
#define NP_ICON_CLOCK         "\xEE\xA2\xB5" /* schedule U+E8B5 */
#define NP_ICON_NIGHT         "\xEE\x94\x9C" /* dark_mode U+E51C */
#define NP_ICON_VOLUME_UP     "\xEE\x81\x90"
#define NP_ICON_VOLUME_MUTE   "\xEE\x81\x8E"
#define NP_ICON_WARNING       "\xEE\x80\x82"
#define NP_ICON_INFO          "\xEE\xA2\x8E"
#define NP_ICON_CHECK         "\xEE\x97\x8A"
#define NP_ICON_CLOSE         "\xEE\x97\x8D"
#define NP_ICON_SEARCH        "\xEE\xA2\xB6"
#define NP_ICON_ARROW_DOWN    "\xEE\x8C\x93"
#define NP_ICON_ARROW_LEFT    "\xEE\x8C\x94"
#define NP_ICON_ARROW_RIGHT   "\xEE\x8C\x95"
#define NP_ICON_ARROW_UP      "\xEE\x8C\x96"
#define NP_ICON_BACKSPACE     "\xEE\x85\x8A"
#define NP_ICON_KEYBOARD_CAPSLOCK "\xEE\x8C\x98"
#define NP_ICON_KEYBOARD_HIDE "\xEE\x8C\x9A"
#define NP_ICON_LOCK          "\xEE\xA2\x99"
#define NP_ICON_VISIBILITY    "\xEE\xA3\xB4"
#define NP_ICON_VISIBILITY_OFF "\xEE\xA3\xB5"
#else
#define NP_FONT_HERO     (&lv_font_montserrat_48)
#define NP_FONT_BRAND    (&lv_font_montserrat_48)
#define NP_FONT_DISPLAY  (&lv_font_montserrat_48)
#define NP_FONT_TITLE    (&lv_font_montserrat_32)
#define NP_FONT_LG       (&lv_font_montserrat_24)
#define NP_FONT_MD       (&lv_font_montserrat_20)
#define NP_FONT_SM       (&lv_font_montserrat_16)
#define NP_FONT_ICON     (&lv_font_montserrat_20)
#define NP_FONT_ICON_BADGE (&lv_font_montserrat_48)
#define NP_ICON_MENU          "M"
#define NP_ICON_WIFI          "W"
#define NP_ICON_WIFI_LOW      "1"
#define NP_ICON_WIFI_MEDIUM   "2"
#define NP_ICON_WIFI_OFF      "x"
#define NP_ICON_BLUETOOTH     "B"
#define NP_ICON_NOTIFICATIONS "N"
#define NP_ICON_SETTINGS      "S"
#define NP_ICON_RISE          "^"
#define NP_ICON_FALL          "v"
#define NP_ICON_WIND          "~"
#define NP_ICON_HUMIDITY      "%"
#define NP_ICON_TEMPERATURE   "T"
#define NP_ICON_UV            "*"
#define NP_ICON_LOCATION      "@"
#define NP_ICON_BITCOIN       "B"
#define NP_ICON_DOLLAR        "$"
#define NP_ICON_MARKET        "I"
#define NP_ICON_HOME          "H"
#define NP_ICON_WEATHER       "W"
#define NP_ICON_CALENDAR      "C"
#define NP_ICON_DISPLAY       "D"
#define NP_ICON_CLOCK         "C"
#define NP_ICON_NIGHT         "N"
#define NP_ICON_VOLUME_UP     "V"
#define NP_ICON_VOLUME_MUTE   "v"
#define NP_ICON_WARNING       "!"
#define NP_ICON_INFO          "i"
#define NP_ICON_CHECK         "+"
#define NP_ICON_CLOSE         "x"
#define NP_ICON_SEARCH        "?"
#define NP_ICON_ARROW_DOWN    "v"
#define NP_ICON_ARROW_LEFT    "<"
#define NP_ICON_ARROW_RIGHT   ">"
#define NP_ICON_ARROW_UP      "^"
#define NP_ICON_BACKSPACE     "<"
#define NP_ICON_KEYBOARD_CAPSLOCK "^"
#define NP_ICON_KEYBOARD_HIDE "v"
#define NP_ICON_LOCK          "L"
#define NP_ICON_VISIBILITY    "o"
#define NP_ICON_VISIBILITY_OFF "x"
#endif

/* ------------------------------------------------------------------ */
/* Estado de dado                                                      */
/* ------------------------------------------------------------------ */

typedef enum {
    NP_DATA_UNAVAILABLE = 0,
    NP_DATA_STALE       = 1,
    NP_DATA_LIVE        = 2
} np_data_state_t;

static inline lv_color_t np_c_for_state(np_data_state_t state)
{
    switch (state) {
        case NP_DATA_LIVE:  return np_c_positive();
        case NP_DATA_STALE: return np_c_warning();
        default:            return np_c_text_3();
    }
}

/* Mantido para componentes legados. A Home V2 usa somente status dot. */
static inline const char *np_label_for_state(np_data_state_t state)
{
    switch (state) {
        case NP_DATA_LIVE:  return "Ao vivo";
        case NP_DATA_STALE: return "Cache";
        default:            return "Indisponivel";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NP_TOKENS_H */
