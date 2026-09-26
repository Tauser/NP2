/*
 * Boot -- arquetipo Fluxo. Full-bleed, sem chrome, um passo por vez.
 *
 * Cinco estagios derivados de estado real (display, storage, rede,
 * relogio, dados). Sem spinner e sem animacao continua: o boot util dura
 * ~1,5 s e uma animacao ai so compete por banda de MSPI justamente
 * enquanto o resto do sistema esta subindo.
 */

#include "np_components.h"
#include "np_screens.h"
#include "np_styles.h"

np_boot_view_t np_boot_build(lv_obj_t *parent)
{
    np_boot_view_t view = {0};
    view.root = np_scene(parent);

    /* Marca de acento a esquerda do wordmark: um retangulo, nao um logo
     * bitmap -- 0 B de asset. */
    np_fill(view.root, 100, 232, 11, 11, np_c_accent(), LV_OPA_COVER, 2);

    np_label(view.root, "Nova", NP_FONT_BRAND, np_c_text(), 100, 258, 230,
             LV_TEXT_ALIGN_LEFT);
    np_label(view.root, "Panel", NP_FONT_BRAND, np_c_text_3(), 300, 258, 250,
             LV_TEXT_ALIGN_LEFT);

    /* Estagios: preenchidos conforme o boot avanca. */
    view.progress = np_segbar(view.root, 100, 380, 788, 5);
    static const char *const stages[] = {"Display", "Armazenamento", "Rede", "Hora", "Dados"};
    for (int i = 0; i < 5; ++i) {
        np_label(view.root, stages[i], NP_FONT_SM, np_c_text_3(), 100 + i * 160, 391, 148,
                 LV_TEXT_ALIGN_LEFT);
    }

    view.status = np_label(view.root, "Preparando o display", NP_FONT_MD, np_c_text(),
                           100, 435, 788, LV_TEXT_ALIGN_LEFT);
    view.detail = np_label(view.root, "A inicialização continua mesmo sem internet.", NP_FONT_SM,
                           np_c_text_3(), 100, 470, 788, LV_TEXT_ALIGN_LEFT);
    np_label(view.root, "NovaOS · ESP32-P4 + C6", NP_FONT_SM, np_c_text_disabled(),
             100, 530, 500, LV_TEXT_ALIGN_LEFT);
    return view;
}

lv_obj_t *np_boot_create(lv_obj_t *parent)
{
    np_boot_view_t view = np_boot_build(parent);
    np_segbar_set(&view.progress, 3, np_c_accent());
    return view.root;
}
