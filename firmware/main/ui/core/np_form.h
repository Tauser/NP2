/* Template único de controles de formulário do NovaPanel. */
#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "np_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NP_FORM_BUTTON_SECONDARY = 0,
    NP_FORM_BUTTON_PRIMARY,
    NP_FORM_BUTTON_DESTRUCTIVE,
} np_form_button_kind_t;

/* Texto Montserrat com fallback para os ícones Material do produto. */
const lv_font_t *np_form_text_font(void);

/* Aplica borda, foco, tipografia e espaçamento padrão a qualquer campo. */
void np_form_apply_field_style(lv_obj_t *field);

lv_obj_t *np_form_text_input(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t w, int32_t h, const char *placeholder,
                              const char *leading_icon);
lv_obj_t *np_form_dropdown(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t w, int32_t h);

/* Cria uma ação com os estados normal, pressionado, foco e desabilitado. */
lv_obj_t *np_form_button(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t w, int32_t h, const char *text,
                         np_form_button_kind_t kind);
void np_form_apply_button_style(lv_obj_t *button, np_form_button_kind_t kind);

/* Compact action embedded in a form field, rendered from NP_FONT_ICON. */
lv_obj_t *np_form_icon_button(lv_obj_t *parent, int32_t x, int32_t y,
                              int32_t size, const char *icon);

#ifdef __cplusplus
}
#endif
