#include "HTTPClient.h"
#include <cstdlib>
#include <stdexcept>
#include <cstring>
static np_spotify_http_exchange_t s_exchange;
extern "C" void np_spotify_bind_http(np_spotify_http_exchange_t exchange) { s_exchange=exchange; }
static void reject() { throw std::runtime_error("Spotify HTTP request failed"); }
void bell::HTTPClient::Response::clear()
{
    buffer.reset(nullptr,0);input.clear();
    if (reply.body) {
        volatile uint8_t *p=reply.body;
        for (size_t i=0;i<reply.capacity;++i) p[i]=0;
        std::free(reply.body);
    }
    reply={};total=0;
}
void bell::HTTPClient::Response::perform(const std::string& url,bool post,
    const Headers& headers,const std::vector<uint8_t>& body)
{
    clear();
    spotify_http_request_t request{};
    request.url=url.c_str();request.post=post;request.body=body.data();request.body_size=body.size();
    if (headers.size()>3 || url.find('\0')!=std::string::npos) reject();
    for (const auto& [name,value]:headers) {
        if (name.find('\0')!=std::string::npos || value.find('\0')!=std::string::npos) reject();
        const char **target=nullptr;
        if (name=="Authorization") target=&request.authorization;
        else if (name=="Content-Type") target=&request.content_type;
        else if (name=="Range") target=&request.range;
        else reject();
        if (*target) reject();
        *target=value.c_str();
    }
    spotify_http_policy_t policy{};
    if (!s_exchange || !spotify_http_validate_request(&request,&policy)) reject();
    int result=s_exchange(&request,&reply);
    reply.content_range[sizeof(reply.content_range)-1]=0;
    if (result || !reply.body || reply.size>reply.capacity || !spotify_http_validate_response(&policy,reply.status,reply.content_length,
        reply.content_range,reply.size,true,&total)) { clear();reject(); }
    if (policy.kind!=SPOTIFY_HTTP_AUDIO) total=reply.size;
    buffer.reset(reply.body,reply.size);
}
bell::HTTPClient::ValueHeader bell::HTTPClient::RangeHeader::range(int32_t from,int32_t to)
{
    if (from<0 || to<from || static_cast<uint32_t>(to-from)>=SPOTIFY_HTTP_AUDIO_MAX) reject();
    return {"Range","bytes="+std::to_string(from)+"-"+std::to_string(to)};
}
bell::HTTPClient::ValueHeader bell::HTTPClient::RangeHeader::last(int32_t count)
{
    if (count<=0 || count>static_cast<int32_t>(SPOTIFY_HTTP_AUDIO_MAX)) reject();
    return {"Range","bytes=-"+std::to_string(count)};
}
