#include "ApResolve.h"
#include "HTTPClient.h"
#include "SpotifyResponse.h"
#include <stdexcept>
using namespace cspot;
ApResolve::ApResolve(std::string override) : apOverride(std::move(override)) {}
std::string ApResolve::fetchFirstApAddress()
{
    /* No arbitrary endpoint override in the product adapter. */
    if(!apOverride.empty())throw std::runtime_error("Spotify AP override unavailable");
    auto response=bell::HTTPClient::get("https://apresolve.spotify.com/");
    return np2::spotifyResponse(SPOTIFY_HTTP_RESOLVE,response->body());
}
