#include "np_home_camera.h"


#include "camera_stream_service.h"
#include "esp_heap_caps.h"
#include "np_tokens.h"

#define HOME_CAMERA_X 24
#define HOME_CAMERA_Y 76
#define HOME_CAMERA_W 560
#define HOME_CAMERA_H 376
#define HOME_CAMERA_MARGIN 6
#define HOME_CAMERA_VIDEO_W (HOME_CAMERA_W - 2 * HOME_CAMERA_MARGIN)
#define HOME_CAMERA_VIDEO_H (HOME_CAMERA_H - 2 * HOME_CAMERA_MARGIN)
#define HOME_CAMERA_STATUS_W 440

/* One copy shared by every Home build: Home is rebuilt on each return, and the frame must outlive
 * the descriptor LVGL caches by address. Allocated on first use, in PSRAM. */
static uint8_t *s_frame;
static lv_image_dsc_t s_image;
/* Outlives the view: Home is destroyed and rebuilt while the session may still be running. */
static bool s_running;

np_home_camera_view_t np_home_camera_build(lv_obj_t *parent)
{
    np_home_camera_view_t view = {0};
    view.root = np_panel(parent, HOME_CAMERA_X, HOME_CAMERA_Y, HOME_CAMERA_W, HOME_CAMERA_H);
    np_set_clip_corner(view.root, true);
    lv_obj_remove_flag(view.root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *const surface = np_fill(view.root, HOME_CAMERA_MARGIN, HOME_CAMERA_MARGIN,
        HOME_CAMERA_VIDEO_W, HOME_CAMERA_VIDEO_H, lv_color_black(), LV_OPA_COVER, NP_RADIUS_SURFACE);
    lv_obj_remove_flag(surface, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_clip_corner(surface, true, 0);

    view.image = lv_image_create(surface);
    lv_obj_set_size(view.image, HOME_CAMERA_VIDEO_W, HOME_CAMERA_VIDEO_H);
    lv_obj_center(view.image);
    lv_obj_remove_flag(view.image, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_inner_align(view.image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_antialias(view.image, false);

    view.status = np_label(surface, "", NP_FONT_MD, lv_color_white(),
        (HOME_CAMERA_VIDEO_W - HOME_CAMERA_STATUS_W) / 2, HOME_CAMERA_VIDEO_H / 2 - 24,
        HOME_CAMERA_STATUS_W, LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(view.status, LV_LABEL_LONG_WRAP);
    return view;
}

static const char *status_text(const camera_stream_status_t *status, const char *camera_ipv4)
{
    if (camera_ipv4 == NULL || camera_ipv4[0] == '\0') return "Nenhuma câmera cadastrada.\nCadastre em Dispositivos.";
    switch (status->state) {
    case CAMERA_STREAM_WAITING_CREDENTIALS: return "Informe a conta da câmera em Dispositivos.";
    case CAMERA_STREAM_CONNECTING: return "Conectando à câmera…";
    case CAMERA_STREAM_AUTHENTICATING: return "Autenticando…";
    case CAMERA_STREAM_PLAYING: return "Aguardando imagem…";
    case CAMERA_STREAM_ERROR:
        if (status->last_rtsp_status == 401U || status->last_rtsp_status == 403U)
            return "Conta da câmera recusada.\nAltere em Dispositivos.";
        return "Sem resposta da câmera.";
    case CAMERA_STREAM_STOPPED:
    default: return "Conectando à câmera…";
    }
}

void np_home_camera_stop(void)
{
    if (!s_running) return;
    (void)camera_stream_service_stop();
    s_running = false;
}

static void stop(np_home_camera_view_t *view)
{
    np_home_camera_stop();
    view->frame_sequence = 0U;
    lv_image_set_src(view->image, NULL);
}

void np_home_camera_sync(np_home_camera_view_t *view, bool visible, const char *camera_ipv4)
{
    if (view == NULL || view->root == NULL) return;
    const bool has_camera = camera_ipv4 != NULL && camera_ipv4[0] != '\0';
    if (!visible || !has_camera) {
        stop(view);
        if (visible) np_set_text(view->status, status_text(NULL, camera_ipv4));
        return;
    }
    if (!s_running) {
        const esp_err_t result = camera_stream_service_play_saved(camera_ipv4);
        if (result == ESP_ERR_NOT_FOUND) {
            np_set_text(view->status, "Informe a conta da câmera em Dispositivos.");
            return;
        }
        if (result != ESP_OK) {
            np_set_text(view->status, "Não foi possível iniciar o vídeo.");
            return;
        }
        s_running = true;
    }

    camera_stream_status_t status = {0};
    camera_stream_service_get_status(&status);
    const bool picture = status.state == CAMERA_STREAM_PLAYING && status.frame_sequence != 0U;
    np_set_visible(view->status, !picture);
    if (!picture) {
        np_set_text(view->status, status_text(&status, camera_ipv4));
        return;
    }
    np_home_camera_present(view);
}

void np_home_camera_present(np_home_camera_view_t *view)
{
    if (view == NULL || view->root == NULL || !s_running) return;
    camera_stream_status_t status = {0};
    camera_stream_service_get_status(&status);
    if (status.state != CAMERA_STREAM_PLAYING || status.frame_sequence == 0U ||
        status.frame_sequence == view->frame_sequence) return;
    if (s_frame == NULL) {
        s_frame = heap_caps_malloc(CAMERA_STREAM_RGB565_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_frame == NULL) return;
    }
    uint16_t width = 0U;
    uint16_t height = 0U;
    uint32_t sequence = 0U;
    if (camera_stream_service_copy_frame(s_frame, CAMERA_STREAM_RGB565_BYTES, &width, &height,
                                         &sequence) != ESP_OK || width == 0U || height == 0U) return;
    s_image = (lv_image_dsc_t){
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .w = width,
            .h = height,
            .stride = (uint16_t)(width * 2U),
        },
        .data_size = (uint32_t)width * height * 2U,
        .data = s_frame,
    };
    lv_image_set_src(view->image, &s_image);
    view->frame_sequence = sequence;
}
