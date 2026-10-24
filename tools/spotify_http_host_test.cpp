#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "HTTPClient.h"
static unsigned calls;
static int mode;
static int exchange(const spotify_http_request_t *r,np_spotify_http_reply_t *out)
{
    ++calls;
    spotify_http_policy_t p{};assert(spotify_http_validate_request(r,&p));
    out->status=p.kind==SPOTIFY_HTTP_AUDIO ? 206 : 200;
    out->size=p.kind==SPOTIFY_HTTP_AUDIO ? 8192 : 2;
    if (mode==1) out->status=302;
    if (mode==2) out->size=SPOTIFY_HTTP_AUDIO_MAX+1;
    out->capacity=out->size;
    out->body=static_cast<uint8_t*>(std::malloc(out->capacity));assert(out->body);
    std::memset(out->body,'x',out->size);
    if (out->size==2) std::memcpy(out->body,"{}",2);
    out->content_length=out->size;
    std::strcpy(out->content_range,"bytes 0-8191/50000");
    if (mode==3) std::strcpy(out->content_range,"bytes 1-8192/50000");
    if (mode==4) out->size=out->capacity+1;
    return 0;
}
template<class Fn> static void rejected(Fn fn)
{
    bool failed=false;
    try { fn(); } catch(const std::runtime_error& e) {
        failed=true;assert(!std::strstr(e.what(),"https"));assert(!std::strstr(e.what(),"private"));
    }
    assert(failed);
}
static void policy_tests()
{
    spotify_http_policy_t p{};
    spotify_http_request_t r{};r.url="https://apresolve.spotify.com/";
    assert(spotify_http_validate_request(&r,&p));
    assert(p.body_limit==4096);
    assert(!spotify_http_validate_response(&p,200,4097,"",0,false,nullptr));
    assert(spotify_http_validate_response(&p,200,2,"",2,true,nullptr));
    assert(!spotify_http_validate_response(&p,200,8193,"",0,false,nullptr));
    assert(!spotify_http_validate_response(&p,302,2,"",0,false,nullptr));
    assert(!spotify_http_validate_response(&p,200,2,"",1,true,nullptr));
    assert(spotify_http_validate_response(&p,200,-1,"",2,true,nullptr));
    r.url="https://accounts.spotify.com/api/token";r.post=true;
    r.content_type="application/x-www-form-urlencoded";
    const char form[]="grant_type=client_credentials&client_id=abcdefghijklmnop&client_secret=0123456789012345";
    r.body=reinterpret_cast<const uint8_t*>(form);r.body_size=sizeof(form)-1;
    assert(spotify_http_validate_request(&r,&p));
    assert(p.body_limit==4096);
    r.body_size--;assert(!spotify_http_validate_request(&r,&p));r.body_size++;
    r.authorization="Basic ";assert(!spotify_http_validate_request(&r,&p));
    r.authorization="Basic private";
    const char grant[]="grant_type=client_credentials";
    r.body=reinterpret_cast<const uint8_t*>(grant);r.body_size=sizeof(grant)-1;
    assert(spotify_http_validate_request(&r,&p));
    r={};
    const char *bad[]={"http://apresolve.spotify.com/","https://apresolve.spotify.com.evil/",
        "https://127.0.0.1/","https://audio.scdn.co@evil/a","https://audio.scdn.co:8443/a",
        "https://evilscdn.co/a","https://audio.scdn.co/a#fragment","https://audio.scdn.co/a b","https://audio.scdn.co/\r\nBad"};
    for(auto s:bad) { r.url=s;assert(!spotify_http_validate_request(&r,&p)); }
    r.url="https://api.spotify.com/v1/storage-resolve/files/audio/interactive/a";
    r.authorization="Bearer private";assert(!spotify_http_validate_request(&r,&p));
    r.url="https://api.spotify.com/v1/storage-resolve/files/audio/interactive/0123456789012345678901234567890123456789?alt=json&product=9";
    assert(spotify_http_validate_request(&r,&p));
    r.authorization="Bearer private\n";assert(!spotify_http_validate_request(&r,&p));
    r={};r.url="https://audio.scdn.co/a?private=signature";r.range="bytes=0-8191";
    assert(spotify_http_validate_request(&r,&p));
    uint64_t total=0;
    assert(spotify_http_validate_response(&p,206,8192,"bytes 0-8191/50000",8192,true,&total) && total==50000);
    assert(!spotify_http_validate_response(&p,200,8192,"bytes 0-8191/50000",8192,true,nullptr));
    assert(!spotify_http_validate_response(&p,206,8192,"bytes 1-8192/50000",8192,true,nullptr));
    assert(!spotify_http_validate_response(&p,206,8192,"bytes 0-8191/*",8192,true,nullptr));
    const char *ranges[]={"bytes=0-20000","bytes=1-0","bytes=0-1,4-5","bytes=0-","bytes=-0",
        "bytes=0-9999999999999999999","bytes=0-8\r\nBad","bytes=0-8191x"};
    for(auto s:ranges) { r.range=s;assert(!spotify_http_validate_request(&r,&p)); }
    r.range="bytes=-12288";assert(spotify_http_validate_request(&r,&p));
    assert(spotify_http_validate_response(&p,206,12288,"bytes 37712-49999/50000",12288,true,&total));
    r.range="bytes=49000-49999";assert(spotify_http_validate_request(&r,&p));
    assert(spotify_http_validate_response(&p,206,1000,"bytes 49000-49999/50000",1000,true,&total));
}
int main()
{
    policy_tests();
    rejected([]{bell::HTTPClient::get("https://apresolve.spotify.com/");});
    np_spotify_bind_http(exchange);
    auto json=bell::HTTPClient::get("https://apresolve.spotify.com/");
    assert(json->body()=="{}" && json->contentLength()==2);
    char text[3]={};json->stream().read(text,2);assert(!std::strcmp(text,"{}"));
    auto audio=bell::HTTPClient::get("https://audio.scdn.co/a?private=value",{bell::HTTPClient::RangeHeader::range(0,8191)});
    assert(audio->totalLength()==50000 && audio->contentLength()==8192);
    for(mode=1;mode<=4;++mode)
        rejected([]{bell::HTTPClient::get("https://audio.scdn.co/a?private=value",{bell::HTTPClient::RangeHeader::range(0,8191)});});
    mode=0;unsigned before=calls;
    rejected([]{bell::HTTPClient::get("https://evil.invalid/a");});assert(calls==before);
    rejected([]{bell::HTTPClient::get("https://apresolve.spotify.com/",{{"Authorization","private\r\nX"}});});
    assert(calls==before);
    rejected([]{bell::HTTPClient::RangeHeader::range(-1,8);});
    rejected([]{bell::HTTPClient::RangeHeader::last(14337);});
    std::cout<<"Spotify HTTP: URL/header/range bounds, redirects, malformed lengths, memory stream, unbound transport and secret-free errors PASS\n";
}
