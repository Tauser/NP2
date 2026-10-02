#include "np_screens.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "app_event_bus.h"
#include "app_state.h"
#include "camera_stream_service.h"
#include "esp_heap_caps.h"
#include "np_form.h"
#include "sonoff_lan_service.h"
#include "np_styles.h"

#define IOT_SIDE 18
#define IOT_TOP 82
#define IOT_WIDTH 988
#define IOT_DEVICES_HEIGHT 282
#define IOT_SENSORS_Y 376
#define IOT_SENSORS_HEIGHT 200
#define IOT_CAMERA_CARD_WIDTH 420
#define IOT_CAMERA_CARD_HEIGHT 68

static lv_image_dsc_t s_camera_stream_image = {0};

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
    const int32_t x = 60 + (int32_t)(index % 2U) * 448;
    const int32_t y = 66 + (int32_t)(index / 2U) * 80;
    lv_obj_t *card = np_fill(parent, x, y, IOT_CAMERA_CARD_WIDTH,
        IOT_CAMERA_CARD_HEIGHT, np_c_surface_raised(), LV_OPA_COVER,
        NP_RADIUS_SURFACE);
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

static lv_obj_t *iot_section(lv_obj_t *parent, int32_t y, int32_t height)
{
    lv_obj_t *section = np_surface(parent, IOT_SIDE, y, IOT_WIDTH, height);
    lv_obj_set_style_border_width(section, 0, 0);
    lv_obj_set_style_pad_all(section, 0, 0);
    return section;
}

static lv_obj_t *empty_state(lv_obj_t *parent, int32_t y, int32_t height,
                             const char *icon, const char *title,
                             const char *detail)
{
    lv_obj_t *card = np_fill(parent, 14, y, IOT_WIDTH - 28, height,
                             np_c_surface_raised(), LV_OPA_COVER,
                             NP_RADIUS_SURFACE);
    np_label(card, icon, NP_FONT_ICON, np_c_text_2(), 22,
             (height - NP_FONT_ICON->line_height) / 2,
             64, LV_TEXT_ALIGN_CENTER);
    np_label(card, title, NP_FONT_LG, np_c_text(), 104,
             height / 2 - 34, IOT_WIDTH - 164, LV_TEXT_ALIGN_LEFT);
    np_label(card, detail, NP_FONT_SM, np_c_text_2(), 104,
             height / 2 + 4, IOT_WIDTH - 164, LV_TEXT_ALIGN_LEFT);
    return card;
}

np_devices_view_t np_devices_build_with_header(lv_obj_t *parent,
                                                const np_header_t *header)
{
    np_devices_view_t view = {0};
    view.root = np_scene(parent);
    view.header = header != NULL ? *header : np_header(view.root);

    lv_obj_t *devices = iot_section(view.root, IOT_TOP, IOT_DEVICES_HEIGHT);
    np_label(devices, "Dispositivos", NP_FONT_LG, np_c_text_2(),
             16, 12, 480, LV_TEXT_ALIGN_LEFT);
    view.add_device_button = np_button(devices, IOT_WIDTH - 230, 10, 210, 48,
                                        "+ Adicionar dispositivo", true);
    view.devices_empty_state = empty_state(devices, 66, 192, NP_ICON_ROUTER,
        "Nenhum dispositivo conectado",
        "Busque dispositivos compatíveis na rede local.");
    for (uint8_t i = 0U; i < APP_IOT_MAX_CAMERAS; ++i) {
        view.camera_cards[i] = camera_card_create(devices, i, &view);
    }

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

    lv_obj_t *sensors = iot_section(view.root, IOT_SENSORS_Y, IOT_SENSORS_HEIGHT);
    np_label(sensors, "Sensores", NP_FONT_LG, np_c_text_2(),
             16, 12, 480, LV_TEXT_ALIGN_LEFT);
    view.add_sensor_button = np_button(sensors, IOT_WIDTH - 230, 10, 210, 48,
                                        "+ Adicionar sensor", true);
    (void)empty_state(sensors, 66, 112, NP_ICON_TEMPERATURE,
                      "Nenhum sensor disponível",
                      "Sensores compatíveis aparecem após conectar uma fonte.");

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
    if (view == NULL || view->root == NULL) return;
    if (sensor) {
        np_set_text(view->add_modal.title, "Adicionar sensor");
        np_set_text(view->add_modal.subtitle, "Fonte de dados necessária");
        np_set_text(view->modal_description,
                    "Os sensores serão adicionados pela integração conectada. "
                    "Assim, valores, unidades e estado vêm do dispositivo real, "
                    "sem dados de demonstração.");
        np_set_visible(view->scan_button, false);
        np_set_visible(view->camera_scan_button, false);
        np_set_visible(view->camera_address_label, false);
        np_set_visible(view->camera_address_input, false);
        np_set_visible(view->camera_address_check_button, false);
        np_set_visible(view->scan_status, false);
        np_set_visible(view->scan_results, false);
    } else {
        np_set_text(view->add_modal.title, "Adicionar dispositivo");
        np_set_text(view->add_modal.subtitle, "SONOFF e câmera Tapo C200");
        np_set_text(view->modal_description,
                    "Escolha a integração para buscar dispositivos disponíveis "
                    "na rede local.");
        np_set_text(view->scan_status, "Pronto para buscar na rede local.");
        np_set_text(view->scan_results, "Selecione SONOFF ou câmera ONVIF.");
        np_set_visible(view->scan_button, true);
        np_set_visible(view->camera_scan_button, true);
        np_set_visible(view->camera_address_label, false);
        np_set_visible(view->camera_address_input, false);
        np_set_visible(view->camera_address_check_button, false);
        np_set_visible(view->scan_status, true);
        np_set_visible(view->scan_results, true);
        view->onvif_scan_selected = false;
    }
    np_modal_show(&view->add_modal);
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
                status.device_count == 0U
                    ? "Nenhum dispositivo eWeLink encontrado."
                    : "Dispositivos encontrados na rede:");
    if (status.device_count == 0U) {
        np_set_text(view->scan_results, "Confira se estão ligados ao mesmo Wi-Fi.");
        return;
    }

    char results[SONOFF_LAN_MAX_DISCOVERED * 96U + 32U] = {0};
    size_t used = 0U;
    for (uint8_t i = 0U; i < status.device_count; ++i) {
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
    const uint8_t count = projection->camera_count > APP_IOT_MAX_CAMERAS
        ? APP_IOT_MAX_CAMERAS : projection->camera_count;
    np_set_visible(view->devices_empty_state, count == 0U);
    for (uint8_t i = 0U; i < APP_IOT_MAX_CAMERAS; ++i) {
        const bool present = i < count;
        np_set_visible(view->camera_cards[i], present);
        if (!present) {
            view->camera_ipv4[i][0] = '\0';
            continue;
        }
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
        for (uint8_t i = 0U; i < count; ++i) {
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
    return np_devices_build_with_header(parent, NULL).root;
}
