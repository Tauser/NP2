#include "np_settings_photo_frame.h"
#include <stdio.h>

static lv_obj_t *frame_choice(lv_obj_t *parent, int32_t y, const char *options)
{
    lv_obj_t *obj = lv_dropdown_create(parent);
    lv_obj_set_pos(obj, 24, y);
    lv_obj_set_size(obj, 264, 48);
    lv_dropdown_set_options(obj, options);
    lv_dropdown_set_symbol(obj, NP_ICON_ARROW_DOWN);
    lv_obj_set_style_text_font(obj, NP_FONT_SM, 0);
    lv_obj_set_style_text_font(obj, NP_FONT_ICON, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(obj, np_c_text(), 0);
    lv_obj_set_style_bg_color(obj, np_c_surface(), 0);
    lv_obj_set_style_border_color(obj, np_c_hairline(), 0);
    lv_obj_set_style_radius(obj, NP_RADIUS_CONTROL, 0);
    lv_obj_t *list = lv_dropdown_get_list(obj);
    lv_obj_set_style_text_font(list, NP_FONT_SM, 0);
    lv_obj_set_style_text_color(list, np_c_text(), 0);
    lv_obj_set_style_bg_color(list, np_c_surface_raised(), 0);
    lv_obj_set_style_max_height(list, 300, 0);
    return obj;
}

np_settings_photo_frame_view_t np_settings_photo_frame_build(lv_obj_t *parent)
{
    np_settings_photo_frame_view_t view = {0};
    view.root = np_scene(parent);
    np_set_visible(view.root, false);
    view.header = np_header(view.root);
    view.back_button = np_button(view.root, NP_HEADER_NAV_X, 12, NP_HEADER_NAV_W,
        NP_TOUCH_TARGET, "Voltar", false);
    lv_obj_t *panel = np_panel(view.root, 24, 80, 976, 496);
    np_label(panel, "Quadro de fotos", NP_FONT_TITLE, np_c_text(), 24, 16, 290, LV_TEXT_ALIGN_LEFT);
    np_label(panel, "Suas fotos, no seu tempo", NP_FONT_SM, np_c_text_2(), 24, 64, 290, LV_TEXT_ALIGN_LEFT);
    np_vline(panel, 316, 16, 460);
    np_label(panel, "Ativar quadro", NP_FONT_MD, np_c_text(), 24, 112, 210, LV_TEXT_ALIGN_LEFT);
    view.photo_switch = lv_switch_create(panel);
    lv_obj_set_pos(view.photo_switch, 236, 110);
    lv_obj_set_size(view.photo_switch, 52, 28);
    lv_obj_set_style_bg_color(view.photo_switch, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view.photo_switch, np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    np_label(panel, "Exibir após inatividade", NP_FONT_SM, np_c_text_2(), 24, 160, 264, LV_TEXT_ALIGN_LEFT);
    view.photo_idle = frame_choice(panel, 184, "30 s\n1 min\n2 min\n5 min\n10 min\n15 min\n30 min\n1 h");
    np_label(panel, "Tempo de cada foto", NP_FONT_SM, np_c_text_2(), 24, 244, 264, LV_TEXT_ALIGN_LEFT);
    view.photo_interval = frame_choice(panel, 268, "10 s\n15 s\n30 s\n1 min\n2 min\n5 min\n10 min\n15 min\n30 min\n1 h");
    np_label(panel, "Voltar após", NP_FONT_SM, np_c_text_2(), 24, 328, 264, LV_TEXT_ALIGN_LEFT);
    view.photo_exit = frame_choice(panel, 352, "Não sair\n30 s\n1 min\n2 min\n5 min\n10 min\n15 min\n30 min\n1 h");
    view.photo_preview = np_button(panel, 24, 428, 128, 48, "Ver fotos", true);
    view.photo_widgets = np_button(panel, 160, 428, 128, 48, "Widgets", false);
    np_label(panel, "Suas imagens", NP_FONT_LG, np_c_text(), 344, 22, 310, LV_TEXT_ALIGN_LEFT);
    view.previous = np_button(panel, 840, 16, 48, 48, NP_ICON_ARROW_LEFT, false);
    view.next = np_button(panel, 900, 16, 48, 48, NP_ICON_ARROW_RIGHT, false);
    lv_obj_set_style_text_font(lv_obj_get_child(view.previous, 0), NP_FONT_ICON, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(view.next, 0), NP_FONT_ICON, 0);
    view.summary = np_label(panel, "Lendo cartão SD...", NP_FONT_SM, np_c_text_2(), 344, 64, 400, LV_TEXT_ALIGN_LEFT);
    view.page_label = np_label(panel, "", NP_FONT_SM, np_c_text_2(), 800, 72, 148, LV_TEXT_ALIGN_RIGHT);
    for (unsigned i = 0; i < PHOTO_PAGE_SIZE; ++i) {
        lv_obj_t *tile = np_fill(panel, 344 + (int32_t)(i%2U)*304,
            112 + (int32_t)(i/2U)*184, 288, 168, np_c_surface_subtle(), LV_OPA_COVER, 14);
        view.tiles[i] = tile;
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_width(tile, 2, 0);
        lv_obj_set_style_border_color(tile, np_c_hairline(), 0);
        view.images[i] = lv_image_create(tile);
        lv_image_set_pivot(view.images[i], 0, 0);
        lv_image_set_scale(view.images[i], 384);
        lv_obj_set_pos(view.images[i], 0, 0);
        lv_obj_remove_flag(view.images[i], LV_OBJ_FLAG_CLICKABLE);
        view.placeholders[i] = np_label(tile, "Preparando...", NP_FONT_SM,
            np_c_text_2(), 16, 68, 256, LV_TEXT_ALIGN_CENTER);
        lv_obj_t *strip = np_fill(tile, 0, 132, 288, 36, lv_color_black(), 160, 0);
        lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
        view.names[i] = np_label(strip, "", NP_FONT_SM, lv_color_white(), 12, 8, 264, LV_TEXT_ALIGN_LEFT);
        view.checks[i] = np_label(tile, "", NP_FONT_ICON, lv_color_white(), 242, 10, 32, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_style_bg_color(view.checks[i], np_c_accent(), 0);
        lv_obj_set_style_bg_opa(view.checks[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(view.checks[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_ver(view.checks[i], 4, 0);
    }
    view.status = np_label(panel, "Toque nas fotos para ativar ou desativar", NP_FONT_SM,
        np_c_text_2(), 344, 472, 604, LV_TEXT_ALIGN_LEFT);
    return view;
}

void np_settings_photo_frame_set_thumbnails(np_settings_photo_frame_view_t *view,
    const photo_thumbnail_page_t *page)
{
    if (!view || !view->root) return;
    const bool matches = page && page->page == view->page && page->catalog == view->catalog;
    for (unsigned i = 0; i < PHOTO_PAGE_SIZE; ++i) {
        const void *source = matches && page->ids[i] == view->ids[i] ? page->images[i] : NULL;
        if (lv_image_get_src(view->images[i]) != source) lv_image_set_src(view->images[i], source);
        np_set_visible(view->images[i], source != NULL);
        np_set_visible(view->placeholders[i], source == NULL);
        np_set_text(view->placeholders[i], matches ? "Imagem indisponível" : "Preparando...");
    }
}

void np_settings_photo_frame_sync(np_settings_photo_frame_view_t *view,
    const photo_frame_status_t *status, uint64_t enabled_mask, bool enabled,
    uint8_t idle_choice, uint8_t interval_choice, uint8_t exit_choice, bool saving)
{
    if (!view || !view->root || !status) return;
    const uint8_t pages = status->count ? (status->count + PHOTO_PAGE_SIZE-1U)/PHOTO_PAGE_SIZE : 1U;
    if (view->page >= pages) view->page = pages-1U;
    if (enabled) lv_obj_add_state(view->photo_switch, LV_STATE_CHECKED);
    else lv_obj_remove_state(view->photo_switch, LV_STATE_CHECKED);
    lv_dropdown_set_selected(view->photo_idle, idle_choice);
    lv_dropdown_set_selected(view->photo_interval, interval_choice);
    lv_dropdown_set_selected(view->photo_exit, exit_choice);
    view->catalog = status->catalog;
    view->enabled_mask = enabled_mask;
    photo_page_ids(view->catalog, view->page, view->ids);
    char summary[64];
    const uint8_t selected = photo_mask_count(view->catalog & enabled_mask);
    snprintf(summary, sizeof(summary), "%u foto%s · %u ativa%s", status->count,
        status->count == 1 ? "" : "s", selected, selected == 1 ? "" : "s");
    np_set_text(view->summary, !status->mounted ? "Cartão SD indisponível" : summary);
    snprintf(summary, sizeof(summary), "%u / %u", view->page+1U, pages);
    np_set_text(view->page_label, summary);
    if (!view->page) lv_obj_add_state(view->previous, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->previous, LV_STATE_DISABLED);
    if (view->page+1U >= pages) lv_obj_add_state(view->next, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->next, LV_STATE_DISABLED);
    for (unsigned i = 0; i < PHOTO_PAGE_SIZE; ++i) {
        np_set_visible(view->tiles[i], view->ids[i] != 0);
        const bool checked = view->ids[i] && (enabled_mask & (UINT64_C(1) << (view->ids[i]-1U)));
        snprintf(summary, sizeof(summary), "Foto %03u", view->ids[i]);
        np_set_text(view->names[i], summary);
        np_set_text(view->checks[i], checked ? NP_ICON_CHECK : "");
        np_set_visible(view->checks[i], checked);
        lv_obj_set_style_border_color(view->tiles[i], checked ? np_c_accent() : np_c_hairline(), 0);
    }
    np_set_text(view->status, !status->mounted ? "Insira o cartão e reinicie o painel" :
        !status->count ? "Nenhuma foto na pasta np2/photos" : !selected ? "Selecione uma foto para usar o quadro" :
        saving ? "Salvando seleção e preferências..." : "Toque nas fotos para ativar ou desativar");
    if (!selected || !status->mounted) lv_obj_add_state(view->photo_preview, LV_STATE_DISABLED);
    else lv_obj_remove_state(view->photo_preview, LV_STATE_DISABLED);
}

static lv_obj_t *photo_choice(lv_obj_t *parent, int32_t x, int32_t y,
                               const char *options)
{
    lv_obj_t *choice = lv_dropdown_create(parent);
    lv_obj_set_pos(choice, x, y);
    lv_obj_set_size(choice, 200, 48);
    lv_dropdown_set_options(choice, options);
    lv_obj_set_style_text_font(choice, NP_FONT_SM, 0);
    lv_dropdown_set_symbol(choice, NP_ICON_ARROW_DOWN);
    lv_obj_set_style_text_font(choice, NP_FONT_ICON, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(choice, np_c_text_2(), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(choice, np_c_text(), 0);
    lv_obj_set_style_bg_color(choice, np_c_surface(), 0);
    lv_obj_set_style_border_color(choice, np_c_hairline(), 0);
    lv_obj_set_style_radius(choice, NP_RADIUS_CONTROL, 0);
    lv_obj_t *list = lv_dropdown_get_list(choice);
    lv_obj_set_style_text_font(list, NP_FONT_SM, 0);
    lv_obj_set_style_text_color(list, np_c_text(), 0);
    lv_obj_set_style_bg_color(list, np_c_surface_raised(), 0);
    return choice;
}

void np_photo_widget_editor_open(np_settings_photo_frame_view_t *view,
                                  uint8_t bitcoin, uint8_t clock)
{
    if (!view || !view->root) return;
    np_photo_widget_editor_t *editor = &view->widget_editor;
    if (!editor->modal.scrim) {
        np_modal_create(&editor->modal, view->root, 132, 90, 760, 420,
            NP_ICON_IMAGE, np_c_accent(), "Widgets das fotos",
            "Bitcoin acompanha o tamanho do relógio");
        lv_obj_remove_flag(editor->modal.content, LV_OBJ_FLAG_SCROLLABLE);
        for (unsigned i = 0; i < 2; ++i) {
            const int32_t x = 24 + (int32_t)i * 380;
            np_label(editor->modal.content, i ? "Data e hora" : "Bitcoin", NP_FONT_LG,
                np_c_text(), x, 14, 240, LV_TEXT_ALIGN_LEFT);
            editor->enabled[i] = lv_switch_create(editor->modal.content);
            lv_obj_set_pos(editor->enabled[i], x + 272, 16);
            lv_obj_set_size(editor->enabled[i], 52, 28);
            lv_obj_set_style_bg_color(editor->enabled[i], np_c_hairline(), LV_PART_MAIN);
            lv_obj_set_style_bg_color(editor->enabled[i], np_c_accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
            np_label(editor->modal.content, "Posição", NP_FONT_SM, np_c_text_2(), x, 72, 100, LV_TEXT_ALIGN_LEFT);
            np_label(editor->modal.content, "Tamanho", NP_FONT_SM, np_c_text_2(), x, 144, 100, LV_TEXT_ALIGN_LEFT);
            np_label(editor->modal.content, "Fundo", NP_FONT_SM, np_c_text_2(), x, 216, 100, LV_TEXT_ALIGN_LEFT);
            editor->position[i] = photo_choice(editor->modal.content, x + 124, 56, "Inferior\nSuperior");
            editor->size[i] = photo_choice(editor->modal.content, x + 124, 128,
                i ? "Normal\nGrande" : "Igual ao relógio");
            if (!i) lv_obj_add_state(editor->size[i], LV_STATE_DISABLED);
            editor->contrast[i] = photo_choice(editor->modal.content, x + 124, 200,
                "Transparente\nLeve (24%)\nEquilibrado (36%)\nForte (60%)");
        }
        np_vline(editor->modal.content, 380, 16, 236);
        np_label(editor->modal.content, "Preferências aplicadas ao salvar", NP_FONT_SM,
            np_c_text_2(), 24, 293, 460, LV_TEXT_ALIGN_LEFT);
        editor->save = np_button(editor->modal.content, 560, 277, 172, 48, "Salvar", true);
    }
    const uint8_t configs[] = {bitcoin, clock};
    for (unsigned i = 0; i < 2; ++i) {
        const uint8_t value = configs[i];
        if (value & PHOTO_WIDGET_VISIBLE) lv_obj_add_state(editor->enabled[i], LV_STATE_CHECKED);
        else lv_obj_remove_state(editor->enabled[i], LV_STATE_CHECKED);
        lv_dropdown_set_selected(editor->position[i], (value & PHOTO_WIDGET_TOP) ? 1 : 0);
        if (i) lv_dropdown_set_selected(editor->size[i], (value & PHOTO_WIDGET_LARGE) ? 1 : 0);
        const uint8_t level = (value >> 3U) & 7U;
        lv_dropdown_set_selected(editor->contrast[i], level >= 5 ? 3 : level >= 3 ? 2 : level ? 1 : 0);
    }
    np_modal_show(&editor->modal);
}

uint8_t np_photo_widget_editor_value(const np_photo_widget_editor_t *editor, unsigned i)
{
    if (!editor || i >= 2 || !editor->save) return 0;
    const uint8_t levels[] = {0, 2, 3, 5};
    const unsigned contrast = lv_dropdown_get_selected(editor->contrast[i]);
    return (lv_obj_has_state(editor->enabled[i], LV_STATE_CHECKED) ? PHOTO_WIDGET_VISIBLE : 0) |
        (lv_dropdown_get_selected(editor->position[i]) ? PHOTO_WIDGET_TOP : 0) |
        (lv_dropdown_get_selected(editor->size[1]) ? PHOTO_WIDGET_LARGE : 0) |
        ((uint8_t)levels[contrast < 4 ? contrast : 2] << 3U);
}
