#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SPOTIFY_PAIRING_BODY_MAX 4096U
typedef struct { char client_id[33], client_secret[33]; } spotify_app_credentials_t;
typedef struct { char *username, *blob, *client_key, *device_name; } spotify_pairing_form_t;
/* Input is private, mutable, NUL-terminated at data[size]. Pointers borrow it. */
bool spotify_pairing_form(char *data, size_t size, spotify_pairing_form_t *out);
bool spotify_app_credentials(const char *json, size_t size, spotify_app_credentials_t *out, void *workspace);
#ifdef __cplusplus
}
#endif
