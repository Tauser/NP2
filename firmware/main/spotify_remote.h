#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define SPOTIFY_REMOTE_DEVICES 6U
#define SPOTIFY_REMOTE_AUTH_REQUIRED (ESP_ERR_INVALID_RESPONSE + 210)
#define SPOTIFY_REMOTE_FORBIDDEN (ESP_ERR_INVALID_RESPONSE + 211)
#define SPOTIFY_REMOTE_RATE_LIMIT (ESP_ERR_INVALID_RESPONSE + 212)
typedef struct {
    char id[129], name[97];
    bool restricted, supports_volume, active;
} spotify_remote_device_t;
typedef struct {
    bool linked, auth_required, busy, available, playing, restricted, supports_volume;
    uint8_t volume, device_count;
    uint32_t progress_ms, duration_ms, revision, completion, cover_generation;
    uint64_t sampled_ms;
    esp_err_t result;
    char title[193], artist[193], device_name[97], device_id[129];
    spotify_remote_device_t devices[SPOTIFY_REMOTE_DEVICES];
} spotify_remote_projection_t;
typedef enum {
    SPOTIFY_REMOTE_REFRESH, SPOTIFY_REMOTE_PLAY, SPOTIFY_REMOTE_PAUSE,
    SPOTIFY_REMOTE_NEXT, SPOTIFY_REMOTE_PREVIOUS, SPOTIFY_REMOTE_VOLUME,
    SPOTIFY_REMOTE_SEEK, SPOTIFY_REMOTE_DEVICES_REQUEST, SPOTIFY_REMOTE_TRANSFER,
    SPOTIFY_REMOTE_VISIBLE, SPOTIFY_REMOTE_HIDDEN,
} spotify_remote_action_t;
typedef struct { spotify_remote_action_t action; uint32_t value, revision; } spotify_remote_command_t;

/* app_loop owns this projection and invokes both reducers. UI posts intents. */
void spotify_remote_tick(spotify_remote_projection_t *, bool online, bool trusted, uint64_t now_ms);
void spotify_remote_command(spotify_remote_projection_t *, const spotify_remote_command_t *);
/* Sole HTTPS worker only. Always completes its one-slot mailbox on failure. */
esp_err_t spotify_remote_process(esp_err_t prerequisite);

/* Private vault: never put these fields in events, projection or logs. */
typedef struct { char client_id[33], refresh_token[513]; } spotify_remote_credentials_t;
bool spotify_remote_linked(void);
esp_err_t spotify_remote_provision_json(const char *, size_t);
esp_err_t spotify_remote_credentials_owner_load(void);
esp_err_t spotify_remote_credentials_owner_write(const spotify_remote_credentials_t *);
esp_err_t spotify_remote_credentials_copy_private(spotify_remote_credentials_t *);
void spotify_remote_credentials_tick(void);
esp_err_t spotify_remote_credentials_rotate_private(const spotify_remote_credentials_t *);

uint32_t spotify_remote_credentials_version(void);
bool spotify_remote_credentials_settled(void);
bool spotify_remote_provision_begin(void);
void spotify_remote_provision_end(void);
