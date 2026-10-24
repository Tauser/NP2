#pragma once
#include <memory>
#include "LoginBlob.h"
/* Private C++ integration boundary, only the Spotify worker calls it. */
bool spotify_playback_run(std::shared_ptr<cspot::LoginBlob>&,const char *client_id,
    const char *client_secret,void (*authenticated)(void));
