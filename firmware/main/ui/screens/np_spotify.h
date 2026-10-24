#pragma once
#include "np_components.h"
#include "np_modal.h"
#include "spotify_remote.h"
#include "spotify_cover.h"
typedef struct {
    lv_obj_t *root, *title, *artist, *status, *device, *link, *refresh;
    lv_obj_t *play, *previous, *next, *position, *time, *duration, *volume, *volume_text, *devices;
    np_modal_t device_modal;
    lv_obj_t *device_rows[SPOTIFY_REMOTE_DEVICES], *device_hint;
    spotify_remote_projection_t state;
    uint32_t modal_revision, usb_until;
    bool syncing;
    lv_obj_t *cover, *cover_shade;
    lv_image_dsc_t cover_image;
    int8_t cover_lease;
    uint32_t cover_generation;
} np_spotify_view_t;
np_spotify_view_t np_spotify_build(lv_obj_t *parent);
void np_spotify_bind(np_spotify_view_t *);
void np_spotify_sync(np_spotify_view_t *, bool online);
