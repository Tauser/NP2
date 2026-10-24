#pragma once
#include "np_components.h"
#include "offline_data_model.h"
#include "photo_frame_model.h"
typedef struct {
    lv_obj_t *root;
    lv_obj_t *image;
    lv_obj_t *clock;
    lv_obj_t *date;
    lv_obj_t *weekday;
    lv_obj_t *bitcoin;
    lv_obj_t *bitcoin_label;
    lv_obj_t *widgets[2];
    uint8_t widget_config[2];
    bool configured;
} np_photo_frame_view_t;
np_photo_frame_view_t np_photo_frame_build(lv_obj_t *parent);
void np_photo_frame_set_source(np_photo_frame_view_t *view, const void *source);
void np_photo_frame_sync(np_photo_frame_view_t *view, uint32_t unix_s, bool trusted,
                         const offline_market_data_t *market,
                         uint8_t bitcoin_widget, uint8_t clock_widget);
