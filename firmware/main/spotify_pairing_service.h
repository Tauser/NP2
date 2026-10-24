#pragma once
#include <stddef.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Laboratory pairing only, physically armed USB caller. RAM-only; at most
 * one attempt per boot. No audio/player capability is exposed by this API. */
esp_err_t spotify_pairing_provision_json(const char *json, size_t size);
#ifdef __cplusplus
}
#endif
