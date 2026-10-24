#pragma once
#include <string>
#include <string_view>
#include "spotify_response_codec.h"
namespace np2 {
/* Only the returned small private string survives the internal workspace. */
std::string spotifyResponse(spotify_http_kind_t kind, std::string_view body, uint32_t *expires=nullptr);
void spotifyScrub(std::string& text);
}
