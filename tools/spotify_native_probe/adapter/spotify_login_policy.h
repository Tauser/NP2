#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t type; size_t offset, size; } spotify_login_auth_t;
/* Helpers for already decrypted Connect material. Never log the input. */
bool spotify_login_auth(const uint8_t *data,size_t size,spotify_login_auth_t *);
bool spotify_login_mac(const uint8_t *expected,size_t expected_size,const uint8_t *received,size_t received_size);
bool spotify_login_username(const char *,size_t);
#ifdef __cplusplus
}
#endif
