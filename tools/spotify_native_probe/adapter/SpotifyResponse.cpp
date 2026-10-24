#include "SpotifyResponse.h"
#include <memory>
#include <stdexcept>
#include "esp_heap_caps.h"
namespace np2 {
void spotifyScrub(std::string& text)
{ volatile char *p=text.data();for(size_t i=0;i<text.size();++i)p[i]=0;text.clear(); }
struct WorkspaceDelete {
    void operator()(spotify_response_workspace_t *w) const {
        if (!w)return;
        volatile uint8_t *p=reinterpret_cast<uint8_t*>(w);
        for(size_t i=0;i<sizeof(*w);++i)p[i]=0;
        heap_caps_free(w);
    }
};
std::string spotifyResponse(spotify_http_kind_t kind,std::string_view body,uint32_t *expires)
{
    constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    if(heap_caps_get_free_size(caps)<80U*1024U+sizeof(spotify_response_workspace_t))
        throw std::runtime_error("Spotify response memory unavailable");
    std::unique_ptr<spotify_response_workspace_t,WorkspaceDelete> w(
        static_cast<spotify_response_workspace_t*>(heap_caps_calloc(1,sizeof(spotify_response_workspace_t),caps)));
    if(!w || !spotify_response_decode(w.get(),kind,body.data(),body.size()))
        throw std::runtime_error("Spotify response invalid");
    if(expires)*expires=w->expires_s;
    return w->text;
}
}
