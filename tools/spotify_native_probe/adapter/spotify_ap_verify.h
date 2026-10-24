#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const uint8_t *key, *signature; } spotify_ap_challenge_t;
bool spotify_ap_parse(const uint8_t *packet, size_t size, spotify_ap_challenge_t *out);
bool spotify_ap_verify(const uint8_t *packet, size_t size);
#ifdef __cplusplus
}
#endif
