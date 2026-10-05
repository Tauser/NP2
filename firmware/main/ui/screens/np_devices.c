#include "np_screens.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "app_event_bus.h"
#include "app_state.h"
#include "camera_stream_service.h"
#include "esp_heap_caps.h"
#include "np_ewelink.h"
#include "np_form.h"
#include "sonoff_lan_service.h"
#include "np_styles.h"

#define IOT_SIDE 18
#define IOT_TOP 82
#define IOT_WIDTH 988
#define IOT_DEVICES_HEIGHT 502
#define IOT_LIST_X 14
#define IOT_LIST_Y 82
#define IOT_LIST_H 396
#define IOT_DEVICE_CARD_H 156
#define IOT_DEVICE_ROW_PITCH 168
#define IOT_CHANNEL_CARD_Y 56
#define IOT_CHANNEL_CARD_H 88
#define IOT_CHANNEL_CARD_GAP 8
#define IOT_CAMERA_CARD_WIDTH 420
#define IOT_CAMERA_CARD_HEIGHT 68
#define IOT_CAMERA_ROW_PITCH 80

static lv_image_dsc_t s_camera_stream_image = {0};

static void set_pos_if_changed(lv_obj_t *obj, int32_t x, int32_t y)
{
    if (obj == NULL) return;
    if (lv_obj_get_x(obj) != x || lv_obj_get_y(obj) != y)
        lv_obj_set_pos(obj, x, y);
}

static void set_size_if_changed(lv_obj_t *obj, int32_t width, int32_t height)
{
    if (obj == NULL) return;
    if (lv_obj_get_width(obj) != width || lv_obj_get_height(obj) != height)
        lv_obj_set_size(obj, width, height);
}

static void set_width_if_changed(lv_obj_t *obj, int32_t width)
{
    if (obj != NULL && lv_obj_get_width(obj) != width)
        lv_obj_set_width(obj, width);
}

static lv_obj_t *ewelink_light_icon_create(lv_obj_t *parent,
                                           int32_t x, int32_t y)
{
    lv_obj_t *const icon = np_fill(parent, x, y, 52, 52,
        np_c_surface_raised(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_t *const glyph = np_label(icon, NP_ICON_LIGHTBULB, NP_FONT_ICON,
        np_c_text_3(), 0, (52 - NP_FONT_ICON->line_height) / 2,
        52, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(glyph, LV_OBJ_FLAG_EVENT_BUBBLE);
    return icon;
}

static void ewelink_light_icon_set_state(lv_obj_t *icon, bool on)
{
    if (icon == NULL) return;
    lv_obj_t *const glyph = lv_obj_get_child(icon, 0);
    if (glyph != NULL)
        np_set_text_color(glyph, on ? np_c_text() : np_c_text_3());
}

static void camera_viewer_before_hide(void *user_data)
{
    np_devices_view_t *const view = user_data;
    if (view == NULL) return;
    (void)camera_stream_service_stop();
    if (view->camera_username_input != NULL)
        lv_textarea_set_text(view->camera_username_input, "");
    if (view->camera_password_input != NULL)
        lv_textarea_set_text(view->camera_password_input, "");
}

static lv_obj_t *camera_card_create(lv_obj_t *parent, uint8_t index,
                                    np_devices_view_t *view)
{
    const int32_t x = 16 + (int32_t)(index % 2U) * 448;
    const int32_t y = (int32_t)(index / 2U) * 80;
    lv_obj_t *card = np_fill(parent, x, y, IOT_CAMERA_CARD_WIDTH,
        IOT_CAMERA_CARD_HEIGHT, np_c_surface_raised(), LV_OPA_COVER,
        NP_RADIUS_SURFACE);
    lv_obj_set_style_border_color(card, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *icon = np_label(card, NP_ICON_CAMERA, NP_FONT_ICON,
        np_c_accent(), 12, (IOT_CAMERA_CARD_HEIGHT - NP_FONT_ICON->line_height) / 2,
        42, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_height(icon, NP_FONT_ICON->line_height);
    view->camera_names[index] = np_label(card, "", NP_FONT_MD, np_c_text(),
        64, 7, 292, LV_TEXT_ALIGN_LEFT);
    view->camera_addresses[index] = np_label(card, "", NP_FONT_SM,
        np_c_text_2(), 64, 37, 270, LV_TEXT_ALIGN_LEFT);
    view->camera_status_dots[index] = np_fill(card, 384, 28, 12, 12,
        np_c_positive(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_set_visible(card, false);
    return card;
}

static lv_obj_t *ewelink_card_create(lv_obj_t *parent, uint8_t index,
                                     np_devices_view_t *view)
{
    const int32_t x = 12 + (int32_t)(index % 3U) * 312;
    const int32_t y = 10 + (int32_t)(index / 3U) * IOT_DEVICE_ROW_PITCH;
    lv_obj_t *card = np_fill(parent, x, y, 300, IOT_DEVICE_CARD_H,
        np_c_surface_raised(), LV_OPA_COVER,
        NP_RADIUS_SURFACE);
    lv_obj_set_style_border_color(card, np_c_hairline(), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    view->ewelink_names[index] = np_label(card, "", NP_FONT_MD, np_c_text(),
        12, 12, 240, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(view->ewelink_names[index], LV_OBJ_FLAG_EVENT_BUBBLE);
    view->ewelink_models[index] = np_label(card, "", NP_FONT_SM,
        np_c_accent(), 12, 12, 214, LV_TEXT_ALIGN_LEFT);
    view->ewelink_status_dots[index] = np_fill(card, 278, 18, 10, 10,
        np_c_text_3(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_add_flag(view->ewelink_status_dots[index], LV_OBJ_FLAG_EVENT_BUBBLE);
    view->ewelink_status[index] = np_label(card, "", NP_FONT_SM,
        np_c_text_2(), 0, 0, 1, LV_TEXT_ALIGN_LEFT);
    np_set_visible(view->ewelink_status[index], false);
    for (uint8_t channel = 0U; channel < 3U; ++channel) {
        view->ewelink_channel_cards[index][channel] = np_fill(card, 12,
            IOT_CHANNEL_CARD_Y, 276, IOT_CHANNEL_CARD_H, np_c_surface(),
            LV_OPA_COVER, NP_RADIUS_CONTROL);
        lv_obj_set_style_border_color(
            view->ewelink_channel_cards[index][channel], np_c_hairline(),
            LV_PART_MAIN);
        lv_obj_set_style_border_width(
            view->ewelink_channel_cards[index][channel], 1, LV_PART_MAIN);
        lv_obj_add_flag(view->ewelink_channel_cards[index][channel],
                        LV_OBJ_FLAG_CLICKABLE);
        view->ewelink_channel_type_icons[index][channel] =
            ewelink_light_icon_create(
                view->ewelink_channel_cards[index][channel], 8,
                (IOT_CHANNEL_CARD_H - 52) / 2);
        view->ewelink_channel_names[index][channel] = np_label(
            view->ewelink_channel_cards[index][channel], "", NP_FONT_SM,
            np_c_text(), 72,
            (IOT_CHANNEL_CARD_H - NP_FONT_SM->line_height) / 2,
            196, LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(view->ewelink_channel_names[index][channel],
                                LV_LABEL_LONG_DOT);
        lv_obj_add_flag(view->ewelink_channel_names[index][channel],
                        LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_height(view->ewelink_channel_names[index][channel],
                          NP_FONT_SM->line_height);
        view->ewelink_channel_states[index][channel] = np_label(
            view->ewelink_channel_cards[index][channel], "",
            NP_FONT_SM, np_c_accent(), 72, IOT_CHANNEL_CARD_H - 24, 196,
            LV_TEXT_ALIGN_LEFT);
        lv_obj_set_height(view->ewelink_channel_states[index][channel], 20);
        np_set_visible(view->ewelink_channel_states[index][channel], false);
        np_set_visible(view->ewelink_channel_cards[index][channel], false);
    }
    np_set_visible(card, false);
    return card;
}

static void ewelink_card_bind_channel_events(np_devices_view_t *view,
                                             uint8_t index)
{
    if (view == NULL || view->ewelink_channel_event_cb == NULL || index >=
        APP_IOT_MAX_EWELINK_DEVICES) return;
    for (uint8_t channel = 0U; channel < 3U; ++channel) {
        if (view->ewelink_channel_cards[index][channel] != NULL)
            lv_obj_add_event_cb(view->ewelink_channel_cards[index][channel],
                view->ewelink_channel_event_cb, LV_EVENT_CLICKED, NULL);
    }
}

static lv_obj_t *iot_section(lv_obj_t *parent, int32_t y, int32_t height)
{
    lv_obj_t *section = np_surface(parent, IOT_SIDE, y, IOT_WIDTH, height);
    lv_obj_set_style_border_width(section, 0, 0);
    lv_obj_set_style_pad_all(section, 0, 0);
    return section;
}

static lv_obj_t *subpage_root(lv_obj_t *parent, const char *title,
                              const char *subtitle, lv_obj_t **back_button)
{
    lv_obj_t *page = np_group(parent, 0, IOT_TOP, NP_SCREEN_W,
                              NP_SCREEN_H - IOT_TOP);
    /* These flows are not active while the overview is built. Hide the
     * subtree before adding its controls so LVGL does not repeatedly lay out
     * several full-screen pages on top of each other during navigation. */
    np_set_visible(page, false);
    lv_obj_t *back = np_icon_button(page, 28, 15, NP_TOUCH_TARGET,
                                    NP_ICON_ARROW_LEFT);
    np_label(page, title, NP_FONT_LG, np_c_text(), 82, 9, 850,
             LV_TEXT_ALIGN_LEFT);
    np_label(page, subtitle, NP_FONT_SM, np_c_text_2(), 82, 43, 880,
             LV_TEXT_ALIGN_LEFT);
    *back_button = back;
    return page;
}

static void set_page(np_devices_view_t *view, np_devices_page_t page)
{
    if (view == NULL) return;
    view->page = page;
    np_set_visible(view->overview_page, page == NP_DEVICES_PAGE_OVERVIEW);
    np_set_visible(view->add_type_page, page == NP_DEVICES_PAGE_ADD_TYPE);
    np_set_visible(view->ewelink_login_page, page == NP_DEVICES_PAGE_EWELINK_LOGIN);
    np_set_visible(view->ewelink_sync_page, page == NP_DEVICES_PAGE_EWELINK_SYNC);
}

static void button_set_text(lv_obj_t *button, const char *text)
{
    if (button == NULL) return;
    lv_obj_t *const label = lv_obj_get_child(button, 0);
    if (label != NULL) np_set_text(label, text);
}

np_devices_view_t np_devices_build_with_header(lv_obj_t *parent,
                                                const np_header_t *header,
                                                const app_iot_projection_t *projection)
{
    np_devices_view_t view = {0};
    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    view.overview_page = np_group(view.root, 0, 0, NP_SCREEN_W, NP_SCREEN_H);
    lv_obj_t *devices = iot_section(view.overview_page, IOT_TOP,
                                    IOT_DEVICES_HEIGHT);
    view.devices_title = np_label(devices, "Dispositivos", NP_FONT_LG,
        np_c_text(), 16, 8, 480, LV_TEXT_ALIGN_LEFT);
    view.devices_subtitle = np_label(devices, "", NP_FONT_SM, np_c_text_2(),
        36, 42, 660, LV_TEXT_ALIGN_LEFT);
    view.devices_online_dot = np_fill(devices, 16, 48, 10, 10,
        np_c_positive(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
    np_set_visible(view.devices_online_dot, false);
    view.add_device_button = np_button(devices, IOT_WIDTH - 230, 10, 210, 48,
                                        "+ Adicionar", true);
    view.device_list = np_surface(devices, 10, IOT_LIST_Y, IOT_WIDTH - 20,
                                  IOT_LIST_H);
    lv_obj_set_style_border_width(view.device_list, 0, 0);
    lv_obj_set_style_pad_all(view.device_list, 0, 0);
    lv_obj_add_flag(view.device_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.device_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(view.device_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_snap_y(view.device_list, LV_SCROLL_SNAP_NONE);
    view.devices_empty_state = np_fill(view.device_list, 0, 0,
        IOT_WIDTH - 20, IOT_LIST_H, np_c_surface(), LV_OPA_COVER,
        NP_RADIUS_SURFACE);
    np_label(view.devices_empty_state, NP_ICON_HOME, NP_FONT_ICON_BADGE,
        np_c_accent(), 0, 38, IOT_WIDTH - 20, LV_TEXT_ALIGN_CENTER);
    np_label(view.devices_empty_state, "Nenhum dispositivo configurado",
        NP_FONT_LG, np_c_text(), 80, 170, IOT_WIDTH - 180,
        LV_TEXT_ALIGN_CENTER);
    np_label(view.devices_empty_state,
        "Importe seus dispositivos eWeLink e prepare o inventário para uso local.",
        NP_FONT_SM, np_c_text_2(), 100, 212, IOT_WIDTH - 220,
        LV_TEXT_ALIGN_CENTER);
    view.empty_import_button = np_button(view.devices_empty_state,
        (IOT_WIDTH - 20 - 256) / 2, 290, 256, 52,
        "Importar dispositivos", true);
    const uint8_t ewelink_count = projection != NULL &&
            projection->ewelink_device_count <= APP_IOT_MAX_EWELINK_DEVICES
        ? projection->ewelink_device_count :
          (projection != NULL ? APP_IOT_MAX_EWELINK_DEVICES : 0U);
    for (uint8_t i = 0U; i < ewelink_count; ++i) {
        view.ewelink_cards[i] = ewelink_card_create(view.device_list, i, &view);
    }
    for (uint8_t i = 0U; i < APP_IOT_MAX_CAMERAS; ++i) {
        view.camera_cards[i] = camera_card_create(view.device_list, i, &view);
    }
    np_set_visible(view.devices_empty_state, true);
    np_set_visible(view.add_device_button, false);

    view.add_type_page = subpage_root(view.root, "Adicionar dispositivo",
        "Escolha o tipo de dispositivo para continuar.",
        &view.add_type_back_button);
    lv_obj_t *type_panel = np_fill(view.add_type_page, 120, 98, 784, 332,
        np_c_surface(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_label(type_panel, "Integrações disponíveis", NP_FONT_MD,
        np_c_text(), 28, 22, 728, LV_TEXT_ALIGN_LEFT);
    view.ewelink_choice_button = np_button(type_panel, 28, 78, 350, 142,
        "SONOFF\neWeLink", true);
    view.camera_choice_button = np_button(type_panel, 406, 78, 350, 142,
        "Câmera\nTapo ONVIF", false);
    np_label(type_panel, "A conta eWeLink será usada somente para importar e atualizar o inventário.",
        NP_FONT_SM, np_c_text_2(), 28, 250, 728, LV_TEXT_ALIGN_LEFT);

    view.ewelink_login_page = subpage_root(view.root, "Login eWeLink",
        "Use suas credenciais apenas para consultar e atualizar dispositivos.",
        &view.ewelink_login_back_button);
    lv_obj_t *login_panel = np_fill(view.ewelink_login_page, 120, 98, 784, 340,
        np_c_surface(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    np_fill(login_panel, 60, 120, 72, 72, np_c_accent(), LV_OPA_COVER,
            NP_RADIUS_TILE);
    np_label(login_panel, "e", NP_FONT_LG, np_c_text_on_accent(),
        60, 129, 72, LV_TEXT_ALIGN_CENTER);
    view.ewelink_login_title = np_label(login_panel, "Conta eWeLink",
        NP_FONT_MD, np_c_text(), 180, 24, 520, LV_TEXT_ALIGN_LEFT);
    view.ewelink_username_label = np_label(login_panel, "Email ou telefone",
        NP_FONT_SM, np_c_text_2(), 180, 68, 484, LV_TEXT_ALIGN_LEFT);
    view.ewelink_username_input = np_form_text_input(login_panel, 180, 90,
        460, 46, "usuario@email.com", NULL);
    lv_textarea_set_max_length(view.ewelink_username_input, 95U);
    view.ewelink_password_label = np_label(login_panel, "Senha", NP_FONT_SM,
        np_c_text_2(), 180, 147, 484, LV_TEXT_ALIGN_LEFT);
    view.ewelink_password_input = np_form_text_input(login_panel, 180, 169,
        460, 46, "Senha da conta eWeLink", NULL);
    lv_textarea_set_password_mode(view.ewelink_password_input, true);
    lv_textarea_set_password_show_time(view.ewelink_password_input, 0U);
    lv_textarea_set_max_length(view.ewelink_password_input, 127U);
    view.ewelink_cancel_button = np_button(login_panel, 180, 238, 220, 50,
        "Cancelar", false);
    view.ewelink_submit_button = np_button(login_panel, 420, 238, 220, 50,
        "Entrar e sincronizar", true);

    view.ewelink_sync_page = subpage_root(view.root,
        "Sincronizando dispositivos",
        "Aguarde enquanto o NovaPanel atualiza seu inventário.",
        &view.ewelink_sync_back_button);
    lv_obj_t *sync_panel = np_fill(view.ewelink_sync_page, 130, 98, 764, 340,
        np_c_surface(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    static const char *const sync_steps[4] = {
        "Autenticando na eWeLink...",
        "Obtendo lista de dispositivos...",
        "Processando inventário...",
        "Salvando localmente...",
    };
    for (uint8_t i = 0U; i < 4U; ++i) {
        const int32_t y = 48 + (int32_t)i * 58;
        view.ewelink_step_dots[i] = np_fill(sync_panel, 38, y + 4, 12, 12,
            np_c_text_3(), LV_OPA_COVER, LV_RADIUS_CIRCLE);
        view.ewelink_step_labels[i] = np_label(sync_panel, sync_steps[i],
            NP_FONT_SM, np_c_text_2(), 68, y, 340, LV_TEXT_ALIGN_LEFT);
    }
    view.ewelink_progress_arc = lv_arc_create(sync_panel);
    lv_obj_remove_style_all(view.ewelink_progress_arc);
    lv_obj_set_pos(view.ewelink_progress_arc, 508, 76);
    lv_obj_set_size(view.ewelink_progress_arc, 146, 146);
    lv_arc_set_range(view.ewelink_progress_arc, 0, 100);
    lv_arc_set_bg_angles(view.ewelink_progress_arc, 0, 359);
    lv_arc_set_rotation(view.ewelink_progress_arc, 270);
    lv_arc_set_value(view.ewelink_progress_arc, 0);
    lv_obj_set_style_arc_color(view.ewelink_progress_arc,
        np_c_surface_raised(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(view.ewelink_progress_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(view.ewelink_progress_arc,
        np_c_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(view.ewelink_progress_arc, 12,
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(view.ewelink_progress_arc, true,
                                 LV_PART_INDICATOR);
    lv_obj_remove_style(view.ewelink_progress_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(view.ewelink_progress_arc, LV_OBJ_FLAG_CLICKABLE);
    view.ewelink_progress_percent = np_label(sync_panel, "0%",
        NP_FONT_MD, np_c_text(), 508, 119, 146, LV_TEXT_ALIGN_CENTER);
    view.ewelink_progress_step = np_label(sync_panel, "0 de 4",
        NP_FONT_SM, np_c_text_2(), 508, 153, 146, LV_TEXT_ALIGN_CENTER);
    view.ewelink_sync_status = np_label(sync_panel, "",
        NP_FONT_SM, np_c_text_2(), 40, 276, 684, LV_TEXT_ALIGN_CENTER);
    view.ewelink_sync_action_button = np_button(sync_panel, 200, 300,
        364, 48, "Continuar em segundo plano", false);
    np_set_visible(view.ewelink_sync_action_button, false);

    set_page(&view, NP_DEVICES_PAGE_OVERVIEW);

    np_modal_create(&view.camera_viewer_modal, view.root, 18, 84, 988, 494,
                    NP_ICON_CAMERA, np_c_accent(), "Tapo C200", "RTSP local • vídeo 640×360");
    lv_obj_t *const video_surface = np_fill(view.camera_viewer_modal.content,
        22, 20, 640, 360, lv_color_black(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    view.camera_stream_image = lv_image_create(video_surface);
    lv_obj_center(view.camera_stream_image);
    lv_obj_set_size(view.camera_stream_image, 640, 360);
    view.camera_frame_buffer = heap_caps_malloc(CAMERA_STREAM_RGB565_BYTES,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    np_label(view.camera_viewer_modal.content, "Conta da Câmera", NP_FONT_MD,
             np_c_text(), 690, 18, 264, LV_TEXT_ALIGN_LEFT);
    view.camera_username_input = np_form_text_input(
        view.camera_viewer_modal.content, 690, 52, 264, 44,
        "Usuário local do Tapo", NULL);
    lv_textarea_set_max_length(view.camera_username_input, 47U);
    view.camera_password_input = np_form_text_input(
        view.camera_viewer_modal.content, 690, 108, 264, 44,
        "Senha da câmera", NULL);
    lv_textarea_set_password_mode(view.camera_password_input, true);
    lv_textarea_set_max_length(view.camera_password_input, 63U);
    view.camera_stream_start_button = np_button(
        view.camera_viewer_modal.content, 690, 166, 264, 48, "Iniciar vídeo", true);
    view.camera_stream_status = np_label(
        view.camera_viewer_modal.content, "Informe a conta local da câmera.",
        NP_FONT_SM, np_c_text_2(), 690, 226, 264, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(view.camera_stream_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(view.camera_stream_status, 48);
    view.camera_stream_stop_button = np_button(
        view.camera_viewer_modal.content, 690, 286, 264, 48, "Parar vídeo", false);
    np_label(view.camera_viewer_modal.content,
        "O usuário e a senha não são salvos. Digite-os novamente após reiniciar o painel.",
        NP_FONT_SM, np_c_text_3(), 690, 344, 264, LV_TEXT_ALIGN_LEFT);
    np_set_visible(view.camera_viewer_modal.scrim, false);

    np_modal_create(&view.add_modal, view.root, 192, 68, 640, 464,
                    NP_ICON_ROUTER, np_c_accent(), "Adicionar", "Integração local");
    view.modal_description = np_label(view.add_modal.content, "", NP_FONT_MD,
                                      np_c_text_2(), 32, 12, 576,
                                      LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(view.modal_description, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(view.modal_description, 72);
    view.scan_button = np_button(view.add_modal.content, 32, 88, 576, 46,
                                 "Buscar SONOFF eWeLink", true);
    view.camera_scan_button = np_button(view.add_modal.content, 330, 88, 278, 46,
                                        "Buscar câmera ONVIF", true);
    lv_obj_set_width(view.scan_button, 278);
    view.camera_address_label = np_label(view.add_modal.content,
        "Ou informe o IP da câmera (serviço ONVIF 2020)", NP_FONT_SM,
        np_c_text_2(), 32, 141, 576, LV_TEXT_ALIGN_LEFT);
    view.camera_address_input = np_form_text_input(view.add_modal.content,
        32, 163, 420, 44, "Ex.: 192.168.1.8", NULL);
    lv_textarea_set_max_length(view.camera_address_input, 15U);
    lv_textarea_set_accepted_chars(view.camera_address_input, "0123456789.");
    view.camera_address_check_button = np_button(view.add_modal.content,
        464, 163, 144, 44, "Verificar IP", true);
    view.scan_status = np_label(view.add_modal.content, "", NP_FONT_SM,
                                 np_c_text_2(), 32, 216, 576,
                                 LV_TEXT_ALIGN_LEFT);
    lv_obj_set_height(view.scan_status, 26);
    view.scan_results = np_label(view.add_modal.content, "", NP_FONT_SM,
                                 np_c_text(), 32, 246, 576,
                                 LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(view.scan_results, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(view.scan_results, 112);
    view.modal_close_button = np_button(view.add_modal.content, 208, 366,
                                         224, 46, "Fechar", true);
    np_set_visible(view.scan_button, false);
    np_set_visible(view.camera_scan_button, false);
    np_set_visible(view.camera_address_label, false);
    np_set_visible(view.camera_address_input, false);
    np_set_visible(view.camera_address_check_button, false);
    np_set_visible(view.scan_status, false);
    np_set_visible(view.scan_results, false);

    return view;
}

void np_devices_rebind(np_devices_view_t *view)
{
    if (view == NULL) return;
    np_modal_rebind(&view->camera_viewer_modal);
    np_modal_set_close_callback(&view->camera_viewer_modal,
                                camera_viewer_before_hide, view);
}

void np_devices_set_channel_event_cb(np_devices_view_t *view,
                                     lv_event_cb_t callback)
{
    if (view == NULL) return;
    view->ewelink_channel_event_cb = callback;
    for (uint8_t i = 0U; i < APP_IOT_MAX_EWELINK_DEVICES; ++i)
        ewelink_card_bind_channel_events(view, i);
}

void np_devices_open_camera(np_devices_view_t *view, uint8_t index)
{
    if (view == NULL || index >= APP_IOT_MAX_CAMERAS ||
        view->camera_ipv4[index][0] == '\0') return;
    view->selected_camera = index;
    lv_textarea_set_text(view->camera_username_input, "");
    lv_textarea_set_text(view->camera_password_input, "");
    char status[48] = {0};
    (void)snprintf(status, sizeof(status), "Câmera %s • stream2", view->camera_ipv4[index]);
    np_set_text(view->camera_stream_status, status);
    np_modal_show(&view->camera_viewer_modal);
}

void np_devices_close_camera(np_devices_view_t *view)
{
    if (view == NULL) return;
    np_modal_hide(&view->camera_viewer_modal);
}

static const char *camera_stream_state_text(const camera_stream_status_t *status)
{
    if (status == NULL) return "Vídeo parado.";
    if (status->state == CAMERA_STREAM_ERROR &&
        status->decoder_unsupported_profile)
        return "A câmera respondeu, mas o perfil H.264 não é compatível. Tentamos o stream alternativo.";
    switch (status->state) {
    case CAMERA_STREAM_WAITING_CREDENTIALS: return "Aguardando as credenciais da câmera.";
    case CAMERA_STREAM_CONNECTING: return "Conectando à câmera na rede local…";
    case CAMERA_STREAM_AUTHENTICATING: return "Autenticando a Conta da Câmera…";
    case CAMERA_STREAM_PLAYING: return "Vídeo ao vivo • RTSP";
    case CAMERA_STREAM_ERROR:
        if (status->last_rtsp_status == 401U) return "Conta ou senha da Câmera rejeitada (RTSP 401).";
        if (status->last_rtsp_status == 403U) return "A câmera negou o acesso ao stream (RTSP 403).";
        if (status->last_rtsp_status == 461U) return "A câmera não aceitou o transporte de vídeo solicitado (RTSP 461).";
        if (status->last_rtsp_status >= 400U) return "A câmera recusou o vídeo. Verifique a Conta da Câmera e o stream RTSP.";
        return "Sem resposta RTSP. Confira a rede e se o RTSP está habilitado na câmera.";
    case CAMERA_STREAM_STOPPED:
    default: return "Vídeo parado.";
    }
}

void np_devices_refresh_camera(np_devices_view_t *view)
{
    if (view == NULL || !np_modal_is_visible(&view->camera_viewer_modal)) return;
    camera_stream_status_t status = {0};
    camera_stream_service_get_status(&status);
    np_set_text(view->camera_stream_status, camera_stream_state_text(&status));
    if (status.state != CAMERA_STREAM_PLAYING) {
        if (view->camera_frame_sequence != 0U) {
            lv_image_set_src(view->camera_stream_image, NULL);
            view->camera_frame_sequence = 0U;
        }
        return;
    }
    if (status.frame_sequence == 0U ||
        status.frame_sequence == view->camera_frame_sequence ||
        view->camera_frame_buffer == NULL) return;
    uint16_t width = 0U;
    uint16_t height = 0U;
    uint32_t sequence = 0U;
    if (camera_stream_service_copy_frame(view->camera_frame_buffer,
            CAMERA_STREAM_RGB565_BYTES, &width, &height, &sequence) != ESP_OK ||
        width == 0U || height == 0U) return;
    s_camera_stream_image = (lv_image_dsc_t){
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = width,
            .h = height,
            .stride = (uint16_t)(width * 2U),
        },
        .data_size = (uint32_t)width * height * 2U,
        .data = view->camera_frame_buffer,
    };
    lv_image_set_src(view->camera_stream_image, &s_camera_stream_image);
    lv_obj_center(view->camera_stream_image);
    view->camera_frame_sequence = sequence;
}

void np_devices_open_add(np_devices_view_t *view, bool sensor)
{
    if (view == NULL || view->root == NULL || sensor) return;
    np_ewelink_status_t status = {0};
    np_ewelink_get_status(&status);
    if (status.busy) {
        np_devices_open_ewelink_sync(view);
        return;
    }
    set_page(view, NP_DEVICES_PAGE_ADD_TYPE);
}

void np_devices_open_camera_add(np_devices_view_t *view)
{
    if (view == NULL || view->root == NULL) return;
    np_set_text(view->add_modal.title, "Adicionar câmera Tapo");
    np_set_text(view->add_modal.subtitle, "Descoberta local ONVIF");
    np_set_text(view->modal_description,
        "No app Tapo, habilite ONVIF e crie uma conta local da câmera.");
    np_set_text(view->scan_status, "Pronto para buscar câmeras na rede local.");
    np_set_text(view->scan_results, "Use a busca ONVIF ou informe o IP da câmera.");
    np_set_visible(view->scan_button, false);
    np_set_visible(view->camera_scan_button, true);
    np_set_visible(view->camera_address_label, true);
    np_set_visible(view->camera_address_input, true);
    np_set_visible(view->camera_address_check_button, true);
    np_set_visible(view->scan_status, true);
    np_set_visible(view->scan_results, true);
    view->onvif_scan_selected = false;
    set_page(view, NP_DEVICES_PAGE_OVERVIEW);
    np_modal_show(&view->add_modal);
}

void np_devices_open_ewelink(np_devices_view_t *view)
{
    if (view == NULL || view->root == NULL) return;
    np_ewelink_status_t status = {0};
    np_ewelink_get_status(&status);
    if (status.busy) {
        np_devices_open_ewelink_sync(view);
        return;
    }
    lv_textarea_set_text(view->ewelink_username_input, "");
    lv_textarea_set_text(view->ewelink_password_input, "");
    set_page(view, NP_DEVICES_PAGE_EWELINK_LOGIN);
}

void np_devices_open_ewelink_sync(np_devices_view_t *view)
{
    if (view == NULL || view->root == NULL) return;
    set_page(view, NP_DEVICES_PAGE_EWELINK_SYNC);
}

void np_devices_return_to_overview(np_devices_view_t *view)
{
    if (view == NULL || view->root == NULL) return;
    lv_textarea_set_text(view->ewelink_username_input, "");
    lv_textarea_set_text(view->ewelink_password_input, "");
    set_page(view, NP_DEVICES_PAGE_OVERVIEW);
}

static uint8_t ewelink_stage_index(np_ewelink_sync_stage_t stage)
{
    switch (stage) {
    case NP_EWELINK_SYNC_AUTHENTICATING: return 0U;
    case NP_EWELINK_SYNC_FETCHING: return 1U;
    case NP_EWELINK_SYNC_RECONCILING: return 2U;
    case NP_EWELINK_SYNC_SAVING: return 3U;
    case NP_EWELINK_SYNC_COMPLETE: return 4U;
    case NP_EWELINK_SYNC_IDLE:
    case NP_EWELINK_SYNC_FAILED:
    default: return 0U;
    }
}

static void sync_progress_update(np_devices_view_t *view)
{
    np_ewelink_status_t status = {0};
    np_ewelink_get_status(&status);
    if (status.busy && view->page != NP_DEVICES_PAGE_EWELINK_SYNC) return;
    if (!status.busy && status.sync_stage == NP_EWELINK_SYNC_IDLE &&
        view->page != NP_DEVICES_PAGE_EWELINK_SYNC) return;

    const uint8_t stage_index = ewelink_stage_index(status.sync_stage);
    static const int16_t stage_progress[] = {10, 38, 64, 88, 100};
    int16_t progress = stage_progress[stage_index];
    if (status.sync_stage == NP_EWELINK_SYNC_FAILED) progress = 0;
    lv_arc_set_value(view->ewelink_progress_arc, progress);
    char percent[12] = {0};
    (void)snprintf(percent, sizeof(percent), "%d%%", (int)progress);
    np_set_text(view->ewelink_progress_percent, percent);
    char step[16] = {0};
    (void)snprintf(step, sizeof(step), "%u de 4",
        (unsigned int)(status.sync_stage == NP_EWELINK_SYNC_COMPLETE
            ? 4U : stage_index + 1U));
    np_set_text(view->ewelink_progress_step, step);
    for (uint8_t i = 0U; i < 4U; ++i) {
        const bool done = status.sync_stage == NP_EWELINK_SYNC_COMPLETE ||
                          (status.busy && i < stage_index);
        const bool active = status.busy && i == stage_index;
        np_set_bg_color(view->ewelink_step_dots[i], done
            ? np_c_positive() : (active ? np_c_accent() : np_c_text_3()));
        np_set_text_color(view->ewelink_step_labels[i], done
            ? np_c_text() : (active ? np_c_text() : np_c_text_2()));
    }
    if (status.busy) {
        np_set_text(view->ewelink_sync_title, "Sincronizando dispositivos");
        np_set_text(view->ewelink_sync_subtitle,
                    "Aguarde enquanto o NovaPanel atualiza seu inventário.");
        np_set_text(view->ewelink_sync_status,
                    "Você pode voltar aos dispositivos; a sincronização continuará em segundo plano.");
        button_set_text(view->ewelink_sync_action_button,
                        "Continuar em segundo plano");
        np_set_visible(view->ewelink_sync_action_button, true);
        lv_obj_clear_state(view->ewelink_sync_action_button, LV_STATE_DISABLED);
    } else if (status.sync_stage == NP_EWELINK_SYNC_COMPLETE) {
        np_set_text(view->ewelink_sync_title, "Sincronização concluída");
        np_set_text(view->ewelink_sync_subtitle,
                    "O inventário foi salvo para operação local.");
        char result[72] = {0};
        (void)snprintf(result, sizeof(result), "%u dispositivo(s) disponíveis no painel.",
                       (unsigned int)status.device_count);
        np_set_text(view->ewelink_sync_status, result);
        button_set_text(view->ewelink_sync_action_button, "Ver dispositivos");
        np_set_visible(view->ewelink_sync_action_button, true);
        lv_obj_clear_state(view->ewelink_sync_action_button, LV_STATE_DISABLED);
    } else if (status.sync_stage == NP_EWELINK_SYNC_FAILED) {
        np_set_text(view->ewelink_sync_title, "Não foi possível sincronizar");
        np_set_text(view->ewelink_sync_subtitle,
                    "Confira sua conexão e as credenciais e tente novamente.");
        np_set_text(view->ewelink_sync_status,
                    "A senha e os tokens temporários foram descartados.");
        button_set_text(view->ewelink_sync_action_button,
                        "Voltar aos dispositivos");
        np_set_visible(view->ewelink_sync_action_button, true);
        lv_obj_clear_state(view->ewelink_sync_action_button, LV_STATE_DISABLED);
        lv_obj_set_style_arc_color(view->ewelink_progress_arc,
                                   np_c_negative(), LV_PART_INDICATOR);
    }
}

void np_devices_refresh_discovery(np_devices_view_t *view)
{
    if (view == NULL || view->onvif_scan_selected || view->scan_status == NULL ||
        !np_modal_is_visible(&view->add_modal) ||
        !lv_obj_is_visible(view->scan_button)) return;

    sonoff_lan_status_t status = {0};
    sonoff_lan_service_get_status(&status);
    if (status.scan_busy) {
        np_set_text(view->scan_status, "Buscando dispositivos eWeLink...");
        return;
    }

    if (status.scan_generation == 0U) return;
    if (status.last_result != ESP_OK) {
        np_set_text(view->scan_status,
                    status.last_result == ESP_ERR_INVALID_STATE
                        ? "Verifique a conexão Wi-Fi e tente novamente."
                        : "Não foi possível concluir a busca. Tente novamente.");
        return;
    }

    np_set_text(view->scan_status,
                status.discovered_count == 0U
                    ? "Nenhum dispositivo eWeLink encontrado."
                    : "Dispositivos encontrados na rede:");
    if (status.discovered_count == 0U) {
        np_set_text(view->scan_results, "Confira se estão ligados ao mesmo Wi-Fi.");
        return;
    }

    char results[SONOFF_LAN_MAX_DISCOVERED * 96U + 32U] = {0};
    size_t used = 0U;
    for (uint8_t i = 0U; i < status.discovered_count; ++i) {
        const sonoff_lan_device_t *const device = &status.devices[i];
        const int written = snprintf(results + used, sizeof(results) - used,
                                     "%s%s  •  %s  •  %s%s",
                                     i == 0U ? "" : "\n",
                                     device->device_id, device->address,
                                     device->local_type[0] != '\0'
                                         ? device->local_type : "tipo a confirmar",
                                     device->encrypted ? "  •  protegido" : "");
        if (written < 0 || (size_t)written >= sizeof(results) - used) break;
        used += (size_t)written;
    }
    np_set_text(view->scan_results, results);
    memset(results, 0, sizeof(results));
}

void np_devices_sync(np_devices_view_t *view,
                     const app_iot_projection_t *projection)
{
    if (view == NULL || projection == NULL) return;
    const uint8_t camera_count = projection->camera_count > APP_IOT_MAX_CAMERAS
        ? APP_IOT_MAX_CAMERAS : projection->camera_count;
    const uint8_t ewelink_count = projection->ewelink_device_count >
        APP_IOT_MAX_EWELINK_DEVICES ? APP_IOT_MAX_EWELINK_DEVICES :
                                      projection->ewelink_device_count;
    const bool has_devices = camera_count > 0U || ewelink_count > 0U;
    np_set_visible(view->devices_empty_state, !has_devices);
    np_set_visible(view->add_device_button, has_devices);
    if (!has_devices) {
        np_set_text(view->devices_subtitle, "Nenhum dispositivo configurado");
        np_set_visible(view->devices_online_dot, false);
    } else {
        const unsigned int device_count =
            (unsigned int)(ewelink_count + camera_count);
        unsigned int online_count = 0U;
        for (uint8_t i = 0U; i < ewelink_count; ++i)
            if (projection->ewelink_devices[i].lan_online) ++online_count;
        for (uint8_t i = 0U; i < camera_count; ++i)
            if (projection->cameras[i].online) ++online_count;
        char subtitle[64] = {0};
        if (device_count == 1U) {
            (void)snprintf(subtitle, sizeof(subtitle),
                "1 dispositivo • %u online", online_count);
        } else {
            (void)snprintf(subtitle, sizeof(subtitle),
                "%u dispositivos • %u online", device_count, online_count);
        }
        np_set_text(view->devices_subtitle, subtitle);
        np_set_bg_color(view->devices_online_dot,
            online_count == device_count ? np_c_positive()
            : (online_count == 0U ? np_c_negative() : np_c_warning()));
        np_set_visible(view->devices_online_dot, true);
    }

    uint8_t row = 0U;
    uint8_t used_units = 0U;
    for (uint8_t i = 0U; i < APP_IOT_MAX_EWELINK_DEVICES; ++i) {
        const bool present = i < ewelink_count;
        if (present && view->ewelink_cards[i] == NULL) {
            view->ewelink_cards[i] = ewelink_card_create(view->device_list, i, view);
            ewelink_card_bind_channel_events(view, i);
        }
        np_set_visible(view->ewelink_cards[i], present);
        if (!present) {
            for (uint8_t channel = 0U; channel < 3U; ++channel)
                np_set_visible(view->ewelink_channel_cards[i][channel], false);
            continue;
        }
        const app_iot_ewelink_device_projection_t *const device =
            &projection->ewelink_devices[i];
        (void)snprintf(view->ewelink_device_ids[i],
                       sizeof(view->ewelink_device_ids[i]), "%s",
                       device->device_id);
        uint8_t channel_count = device->channel_count;
        if (channel_count > 3U) channel_count = 3U;
        const uint8_t units = channel_count == 0U ? 1U : channel_count;
        if (used_units + units > 3U) {
            ++row;
            used_units = 0U;
        }
        const int32_t card_width = (int32_t)units * 300 +
                                   (int32_t)(units - 1U) * 12;
        const int32_t card_x = 12 + (int32_t)used_units * 312;
        const int32_t card_y = 10 + (int32_t)row * IOT_DEVICE_ROW_PITCH;
        set_pos_if_changed(view->ewelink_cards[i], card_x, card_y);
        set_size_if_changed(view->ewelink_cards[i], card_width,
                            IOT_DEVICE_CARD_H);
        const uint8_t visible_channels = channel_count;
        set_width_if_changed(view->ewelink_names[i],
                             card_width - 48);
        set_width_if_changed(view->ewelink_models[i],
                             card_width - 48);
        set_width_if_changed(view->ewelink_status[i], card_width - 48);
        set_pos_if_changed(view->ewelink_status_dots[i], card_width - 26, 18);
        const uint8_t layout_channels = channel_count == 0U ? 1U : channel_count;
        const int32_t channel_card_width =
            (card_width - 24 -
             IOT_CHANNEL_CARD_GAP * (int32_t)(layout_channels - 1U)) /
            (int32_t)layout_channels;
        for (uint8_t channel = 0U; channel < 3U; ++channel) {
            set_pos_if_changed(view->ewelink_channel_cards[i][channel],
                12 + (int32_t)channel *
                    (channel_card_width + IOT_CHANNEL_CARD_GAP),
                IOT_CHANNEL_CARD_Y);
            set_size_if_changed(view->ewelink_channel_cards[i][channel],
                                channel_card_width, IOT_CHANNEL_CARD_H);
            set_width_if_changed(view->ewelink_channel_names[i][channel],
                                 channel_card_width - 80);
            set_width_if_changed(view->ewelink_channel_states[i][channel],
                                 channel_card_width - 80);
            np_set_visible(view->ewelink_channel_cards[i][channel],
                           channel < visible_channels);
        }
        used_units = (uint8_t)(used_units + units);

        const char *const display_name = device->name[0] != '\0'
            ? device->name : device->device_id;
        np_set_text(view->ewelink_names[i], display_name);
        np_set_text(view->ewelink_models[i], "");
        np_set_bg_color(view->ewelink_status_dots[i],
            device->lan_online ? np_c_positive() : np_c_negative());
        for (uint8_t channel = 0U; channel < 3U; ++channel) {
            if (channel >= channel_count) continue;
            const char *const name = device->channel_names[channel][0] != '\0'
                ? device->channel_names[channel] : "Canal";
            np_set_text(view->ewelink_channel_names[i][channel], name);
            const bool state_known = device->lan_online &&
                                     device->channel_state_known[channel];
            const bool service_pending = device->channel_pending[channel];
            bool pending = view->ewelink_channel_pending[i][channel];
            const int32_t channel_result =
                (int32_t)device->channel_result[channel];
            if (pending) {
                if (service_pending)
                    view->ewelink_channel_pending_seen[i][channel] = true;
                const bool confirmed = state_known &&
                    device->channel_on[channel] ==
                        view->ewelink_channel_on[i][channel];
                const bool failed_before_service_pending = !service_pending &&
                    !view->ewelink_channel_pending_seen[i][channel] &&
                    channel_result !=
                        view->ewelink_channel_pending_result[i][channel];
                const bool service_completed = !service_pending &&
                    view->ewelink_channel_pending_seen[i][channel];
                if (!service_pending && (confirmed ||
                    failed_before_service_pending || service_completed)) {
                    pending = false;
                    view->ewelink_channel_pending_seen[i][channel] = false;
                    if (state_known)
                        view->ewelink_channel_on[i][channel] =
                            device->channel_on[channel];
                    else
                        view->ewelink_channel_on[i][channel] = false;
                }
            } else if (service_pending) {
                pending = true;
                view->ewelink_channel_pending_seen[i][channel] = true;
                view->ewelink_channel_pending_result[i][channel] =
                    channel_result;
            } else {
                view->ewelink_channel_on[i][channel] =
                    state_known && device->channel_on[channel];
            }
            view->ewelink_channel_last_result[i][channel] = channel_result;
            const bool channel_on = view->ewelink_channel_on[i][channel];
            view->ewelink_channel_pending[i][channel] = pending;
            view->ewelink_channel_control_enabled[i][channel] =
                state_known && !pending;
            lv_obj_t *const channel_card =
                view->ewelink_channel_cards[i][channel];
            const bool highlighted = channel_on;
            ewelink_light_icon_set_state(
                view->ewelink_channel_type_icons[i][channel], highlighted);
            if (pending) {
                const char *const pending_text = channel_on
                    ? "Ligando..." : "Desligando...";
                np_set_text(view->ewelink_channel_states[i][channel],
                            pending_text);
                np_set_visible(view->ewelink_channel_states[i][channel], true);
            } else {
                np_set_visible(view->ewelink_channel_names[i][channel], true);
                np_set_visible(view->ewelink_channel_states[i][channel], false);
            }
            if (highlighted) {
                lv_obj_set_style_bg_color(channel_card, np_c_accent_bg(),
                                          LV_PART_MAIN);
                lv_obj_set_style_border_color(channel_card, np_c_accent(),
                                              LV_PART_MAIN);
            } else {
                lv_obj_set_style_bg_color(channel_card, np_c_surface(),
                                          LV_PART_MAIN);
                lv_obj_set_style_border_color(channel_card, np_c_hairline(),
                                              LV_PART_MAIN);
            }
        }
    }

    const uint8_t ewelink_rows = ewelink_count == 0U
        ? 0U : (uint8_t)(row + 1U);
    for (uint8_t i = 0U; i < APP_IOT_MAX_CAMERAS; ++i) {
        const bool present = i < camera_count;
        np_set_visible(view->camera_cards[i], present);
        if (!present) {
            view->camera_ipv4[i][0] = '\0';
            continue;
        }
        set_pos_if_changed(view->camera_cards[i],
            12 + (int32_t)(i % 2U) * 448,
            10 + (int32_t)ewelink_rows * IOT_DEVICE_ROW_PITCH +
                (int32_t)(i / 2U) * IOT_CAMERA_ROW_PITCH);
        const app_iot_camera_projection_t *const camera = &projection->cameras[i];
        (void)snprintf(view->camera_ipv4[i], sizeof(view->camera_ipv4[i]),
                       "%s", camera->address);
        np_set_text(view->camera_names[i], camera->model);
        char address[28] = {0};
        (void)snprintf(address, sizeof(address), "%s  •  ONVIF",
                       camera->address);
        np_set_text(view->camera_addresses[i], address);
        np_set_bg_color(view->camera_status_dots[i], camera->online
            ? np_c_positive() : np_c_text_3());
    }

    np_devices_refresh_camera(view);
    sync_progress_update(view);

    if (!view->onvif_scan_selected ||
        !np_modal_is_visible(&view->add_modal)) return;
    if (projection->scan_busy) {
        np_set_text(view->scan_status, "Buscando câmeras ONVIF na rede...");
        return;
    }
    if (projection->scan_generation == 0U ||
        projection->scan_generation == view->onvif_seen_generation) return;
    view->onvif_seen_generation = projection->scan_generation;
    if (projection->last_result == ESP_ERR_INVALID_STATE) {
        np_set_text(view->scan_status, "Conecte o painel ao Wi-Fi e tente novamente.");
        np_set_text(view->scan_results, "");
    } else if (projection->last_result != ESP_OK) {
        np_set_text(view->scan_status, "Falha na busca ONVIF. Tente novamente.");
        np_set_text(view->scan_results, "");
    } else if (projection->camera_count == 0U) {
        np_set_text(view->scan_status, "Nenhuma câmera ONVIF encontrada.");
        np_set_text(view->scan_results,
            "Sem resposta multicast. Informe o IP acima ou confira se a porta "
            "ONVIF 2020 está acessível.");
    } else {
        np_set_text(view->scan_status, "Câmeras encontradas na rede:");
        char results[APP_IOT_MAX_CAMERAS * 72U + 1U] = {0};
        size_t used = 0U;
        for (uint8_t i = 0U; i < camera_count; ++i) {
            const int written = snprintf(results + used, sizeof(results) - used,
                "%s%s  •  %s", i == 0U ? "" : "\n",
                projection->cameras[i].model,
                projection->cameras[i].address);
            if (written < 0 || (size_t)written >= sizeof(results) - used) break;
            used += (size_t)written;
        }
        np_set_text(view->scan_results, results);
    }
}

lv_obj_t *np_devices_create(lv_obj_t *parent)
{
    return np_devices_build_with_header(parent, NULL, NULL).root;
}
