/*
 * NovaPanel v5 -- estilos compartilhados (camada 2).
 */

#ifndef NP_STYLES_H
#define NP_STYLES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void np_styles_init(void);

/* Legado / demais telas */
const lv_style_t *np_st_screen(void);
const lv_style_t *np_st_surface(void);
const lv_style_t *np_st_raised(void);
const lv_style_t *np_st_hairline(void);
const lv_style_t *np_st_label(void);
const lv_style_t *np_st_tile(void);
const lv_style_t *np_st_tile_on(void);
const lv_style_t *np_st_row(void);

/* Home V2 */
const lv_style_t *np_st_panel(void);
const lv_style_t *np_st_metric_tile(void);
const lv_style_t *np_st_icon_button(void);
const lv_style_t *np_st_drawer(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_STYLES_H */
