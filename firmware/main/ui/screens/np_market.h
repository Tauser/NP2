#pragma once

#include "offline_data_model.h"
#include "np_components.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *value;
    lv_obj_t *change_icon;
    lv_obj_t *change;
} np_market_strip_view_t;

typedef struct {
    lv_obj_t *value;
    lv_obj_t *change_icon;
    lv_obj_t *change;
} np_market_altcoin_view_t;

typedef struct {
    lv_obj_t *root;
    np_header_t header;
    np_status_dot_t btc_status;
    np_spark_t btc_spark;
    lv_obj_t *btc_price;
    lv_obj_t *btc_exchange;
    lv_obj_t *btc_exchange_change_icon;
    lv_obj_t *btc_exchange_change;
    lv_obj_t *btc_change_icon;
    lv_obj_t *btc_change;
    lv_obj_t *btc_high;
    lv_obj_t *btc_low;
    lv_obj_t *btc_volume;
    np_market_altcoin_view_t altcoins[4];
    lv_obj_t *fear_root;
    lv_obj_t *fear_gauge;
    lv_obj_t *fear_value;
    lv_obj_t *fear_classification;
    lv_obj_t *fear_marker;
    np_market_strip_view_t sp500;
    np_market_strip_view_t nasdaq;
    np_market_strip_view_t ibovespa;
} np_market_view_t;

np_market_view_t np_market_build_with_header(lv_obj_t *parent, const np_header_t *header);
void np_market_sync(np_market_view_t *view, const offline_data_snapshot_t *snapshot);
