/*
 * NovaPanel v5 -- modelos de tela (camada 4).
 *
 * Home V2: Dark Graphite, 1024x600.
 */

#ifndef NP_SCREENS_H
#define NP_SCREENS_H

#include "np_components.h"
#include "np_modal.h"
#include "app_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    np_segbar_t progress;
    lv_obj_t *status;
    lv_obj_t *detail;
} np_boot_view_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;

    /* Clima */
    lv_obj_t *weather_card;
    lv_obj_t *weather_icon;
    lv_obj_t *weather_icon_fallback;
    np_status_dot_t weather_status;
    lv_obj_t *weather_place;
    lv_obj_t *weather_temperature;
    lv_obj_t *weather_summary;
    lv_obj_t *weather_date;
    np_metric_tile_t weather_wind;
    np_metric_tile_t weather_humidity;
    np_metric_tile_t weather_feels_like;
    np_metric_tile_t weather_uv;

    /* Bitcoin */
    lv_obj_t *btc_card;
    np_status_dot_t btc_status;
    lv_obj_t *btc_price_prefix;
    lv_obj_t *btc_price;
    lv_obj_t *btc_change_icon;
    lv_obj_t *btc_change;
    np_spark_t btc_spark;
    lv_obj_t *btc_high_24h;
    lv_obj_t *btc_low_24h;
    lv_obj_t *btc_volume_24h;

    /* Mercado secundario */
    np_market_strip_t usd;
    np_market_strip_t ibov;
} np_home_view_t;

typedef enum {
    NP_DEVICES_PAGE_OVERVIEW = 0,
    NP_DEVICES_PAGE_ADD_TYPE,
    NP_DEVICES_PAGE_EWELINK_LOGIN,
    NP_DEVICES_PAGE_EWELINK_SYNC,
} np_devices_page_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    np_devices_page_t page;
    lv_obj_t *overview_page;
    lv_obj_t *devices_title;
    lv_obj_t *devices_subtitle;
    lv_obj_t *devices_online_dot;
    lv_obj_t *add_device_button;
    lv_obj_t *devices_empty_state;
    lv_obj_t *empty_import_button;
    lv_obj_t *device_list;
    lv_obj_t *ewelink_cards[APP_IOT_MAX_EWELINK_DEVICES];
    lv_obj_t *ewelink_names[APP_IOT_MAX_EWELINK_DEVICES];
    lv_obj_t *ewelink_models[APP_IOT_MAX_EWELINK_DEVICES];
    lv_obj_t *ewelink_status[APP_IOT_MAX_EWELINK_DEVICES];
    lv_obj_t *ewelink_status_dots[APP_IOT_MAX_EWELINK_DEVICES];
    lv_obj_t *ewelink_channel_cards[APP_IOT_MAX_EWELINK_DEVICES][3];
    lv_obj_t *ewelink_channel_type_icons[APP_IOT_MAX_EWELINK_DEVICES][3];
    lv_obj_t *ewelink_channel_names[APP_IOT_MAX_EWELINK_DEVICES][3];
    lv_obj_t *ewelink_channel_states[APP_IOT_MAX_EWELINK_DEVICES][3];
    char ewelink_device_ids[APP_IOT_MAX_EWELINK_DEVICES][32];
    bool ewelink_channel_on[APP_IOT_MAX_EWELINK_DEVICES][3];
    bool ewelink_channel_control_enabled[APP_IOT_MAX_EWELINK_DEVICES][3];
    bool ewelink_channel_pending[APP_IOT_MAX_EWELINK_DEVICES][3];
    bool ewelink_channel_pending_seen[APP_IOT_MAX_EWELINK_DEVICES][3];
    int32_t ewelink_channel_last_result[APP_IOT_MAX_EWELINK_DEVICES][3];
    int32_t ewelink_channel_pending_result[APP_IOT_MAX_EWELINK_DEVICES][3];
    lv_event_cb_t ewelink_channel_event_cb;
    lv_obj_t *add_type_page;
    lv_obj_t *add_type_back_button;
    lv_obj_t *ewelink_choice_button;
    lv_obj_t *camera_choice_button;
    lv_obj_t *ewelink_login_page;
    lv_obj_t *ewelink_login_back_button;
    lv_obj_t *ewelink_cancel_button;
    lv_obj_t *ewelink_login_title;
    lv_obj_t *ewelink_username_label;
    lv_obj_t *ewelink_password_label;
    lv_obj_t *ewelink_submit_button;
    lv_obj_t *ewelink_sync_page;
    lv_obj_t *ewelink_sync_back_button;
    lv_obj_t *ewelink_sync_title;
    lv_obj_t *ewelink_sync_subtitle;
    lv_obj_t *ewelink_step_dots[4];
    lv_obj_t *ewelink_step_labels[4];
    lv_obj_t *ewelink_progress_arc;
    lv_obj_t *ewelink_progress_percent;
    lv_obj_t *ewelink_progress_step;
    lv_obj_t *ewelink_sync_action_button;
    lv_obj_t *ewelink_sync_status;
    lv_obj_t *camera_cards[APP_IOT_MAX_CAMERAS];
    lv_obj_t *camera_names[APP_IOT_MAX_CAMERAS];
    lv_obj_t *camera_addresses[APP_IOT_MAX_CAMERAS];
    lv_obj_t *camera_status_dots[APP_IOT_MAX_CAMERAS];
    char camera_ipv4[APP_IOT_MAX_CAMERAS][16];
    np_modal_t camera_viewer_modal;
    lv_obj_t *camera_stream_image;
    lv_obj_t *camera_username_input;
    lv_obj_t *camera_password_input;
    lv_obj_t *camera_stream_start_button;
    lv_obj_t *camera_stream_stop_button;
    lv_obj_t *camera_stream_status;
    uint8_t *camera_frame_buffer;
    uint32_t camera_frame_sequence;
    uint8_t selected_camera;
    np_modal_t add_modal;
    lv_obj_t *modal_description;
    lv_obj_t *scan_button;
    lv_obj_t *camera_scan_button;
    lv_obj_t *camera_address_label;
    lv_obj_t *camera_address_input;
    lv_obj_t *camera_address_check_button;
    lv_obj_t *scan_status;
    lv_obj_t *scan_results;
    lv_obj_t *modal_close_button;
    bool onvif_scan_selected;
    uint32_t onvif_seen_generation;
    lv_obj_t *ewelink_username_input;
    lv_obj_t *ewelink_password_input;
} np_devices_view_t;

np_boot_view_t np_boot_build(lv_obj_t *parent);
np_home_view_t np_home_build(lv_obj_t *parent);
np_home_view_t np_home_build_with_header(lv_obj_t *parent, const np_header_t *header);

void np_home_set_weather_icon_source(np_home_view_t *view, const void *source);
void np_home_set_btc_spark(np_home_view_t *view, const int32_t *samples,
                           uint8_t count, lv_color_t color);

/* Wrappers para o catalogo/simulador. */
lv_obj_t *np_boot_create(lv_obj_t *parent);
lv_obj_t *np_home_create(lv_obj_t *parent);
lv_obj_t *np_market_create(lv_obj_t *parent);
lv_obj_t *np_setup_create(lv_obj_t *parent);

/* Aspiracional */
lv_obj_t *np_weather_create(lv_obj_t *parent);
lv_obj_t *np_timer_create(lv_obj_t *parent);
lv_obj_t *np_agenda_create(lv_obj_t *parent);
lv_obj_t *np_alarms_create(lv_obj_t *parent);
lv_obj_t *np_notifications_create(lv_obj_t *parent);
lv_obj_t *np_devices_create(lv_obj_t *parent);
np_devices_view_t np_devices_build_with_header(lv_obj_t *parent,
                                                const np_header_t *header,
                                                const app_iot_projection_t *projection);
void np_devices_open_add(np_devices_view_t *view, bool sensor);
void np_devices_open_camera_add(np_devices_view_t *view);
void np_devices_open_ewelink(np_devices_view_t *view);
void np_devices_open_ewelink_sync(np_devices_view_t *view);
void np_devices_return_to_overview(np_devices_view_t *view);
void np_devices_refresh_discovery(np_devices_view_t *view);
void np_devices_sync(np_devices_view_t *view,
                     const app_iot_projection_t *projection);
void np_devices_set_channel_event_cb(np_devices_view_t *view,
                                     lv_event_cb_t callback);
void np_devices_rebind(np_devices_view_t *view);
void np_devices_open_camera(np_devices_view_t *view, uint8_t index);
void np_devices_close_camera(np_devices_view_t *view);
void np_devices_refresh_camera(np_devices_view_t *view);
lv_obj_t *np_sheets_create(lv_obj_t *parent);

typedef struct {
    const char *id;
    const char *title;
    const char *archetype;
    lv_obj_t *(*create)(lv_obj_t *parent);
} np_screen_entry_t;

const np_screen_entry_t *np_screen_catalog(void);
int np_screen_catalog_count(void);

#ifdef __cplusplus
}
#endif

#endif /* NP_SCREENS_H */
