#include "AccessKeyFetcher.h"
#include "CSpotContext.h"
#include "HTTPClient.h"
#include "SpotifyResponse.h"
using namespace cspot;
AccessKeyFetcher::AccessKeyFetcher(std::shared_ptr<Context> context) : ctx(std::move(context)), expiresAt(0) {}
bool AccessKeyFetcher::isExpired()
{ return accessKey.empty() || ctx->timeProvider->getSyncedTimestamp()>=expiresAt; }
std::string AccessKeyFetcher::getAccessKey()
{
    /* The TrackQueue owner is the only caller. A rejected request waits 30 s
     * before another attempt instead of retrying in a network failure loop. */
    if(ctx->timeProvider->getSyncedTimestamp()<expiresAt)return accessKey;
    updateAccessKey();return accessKey;
}
void AccessKeyFetcher::updateAccessKey()
{
    if(keyPending.exchange(true))return;
    struct Reset { std::atomic<bool>& flag; ~Reset(){flag=false;} } reset{keyPending};
    struct Body {
        std::string text;std::vector<uint8_t> bytes;
        ~Body(){np2::spotifyScrub(text);volatile uint8_t *p=bytes.data();for(size_t i=0;i<bytes.size();++i)p[i]=0;}
    } form;
    np2::spotifyScrub(accessKey);
    expiresAt=ctx->timeProvider->getSyncedTimestamp()+30000;
    try {
        form.text="grant_type=client_credentials&client_id="+ctx->config.clientId+"&client_secret="+ctx->config.clientSecret;
        form.bytes.assign(form.text.begin(),form.text.end());
        auto response=bell::HTTPClient::post("https://accounts.spotify.com/api/token",
            {{"Content-Type","application/x-www-form-urlencoded"}},form.bytes);
        uint32_t expires=0;
        accessKey=np2::spotifyResponse(SPOTIFY_HTTP_TOKEN,response->body(),&expires);
        expiresAt=ctx->timeProvider->getSyncedTimestamp()+static_cast<int64_t>(expires)*1000;
    } catch(...) { /* Fixed backoff; no credentials, response or URL in logs. */ }
}
