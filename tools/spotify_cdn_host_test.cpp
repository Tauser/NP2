#include "CDNAudioFile.h"
#include "HTTPClient.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

static size_t file_size=50003;
static int mode;
static unsigned requests;
static int exchange(const spotify_http_request_t *r,np_spotify_http_reply_t *out)
{
    ++requests;spotify_http_policy_t p{};assert(spotify_http_validate_request(r,&p));
    assert(p.kind==SPOTIFY_HTTP_AUDIO);
    const size_t total=mode==1 && requests>1 ? file_size+16 : file_size;
    const size_t first=p.suffix ? total-std::min(total,static_cast<size_t>(p.last)) : p.first;
    const size_t last=p.suffix ? total-1 : std::min(total-1,static_cast<size_t>(p.last));
    assert(first<=last);
    out->size=last-first+1;out->capacity=out->size;
    out->body=static_cast<uint8_t*>(std::malloc(out->size));assert(out->body);
    for(size_t i=0;i<out->size;++i)out->body[i]=(i+first)%251;
    out->status=mode==2 && requests>2 ? 302 : 206;
    out->content_length=out->size;
    std::snprintf(out->content_range,sizeof(out->content_range),"bytes %zu-%zu/%zu",first,last,total);
    return 0;
}
template<class Fn> static void rejected(Fn fn)
{ bool failed=false;try{fn();}catch(const std::runtime_error&){failed=true;}assert(failed); }
int main()
{
    np_spotify_bind_http(exchange);
    const std::vector<uint8_t> key(16);
    rejected([]{cspot::CDNAudioFile f("https://audio.scdn.co/a",{1,2});});
    cspot::CDNAudioFile file("https://audio.scdn.co/a",key);file.openStream();
    assert(file.getSize()==file_size-167 && requests==2);
    uint8_t buffer[20000];
    size_t position=0;
    for(size_t want : {100U,9000U,5000U,20000U,20000U}){
        size_t n=file.readBytes(buffer,want);assert(n && n<=want);
        for(size_t i=0;i<n;++i)assert(buffer[i]==(position+i+167)%251);
        position+=n;assert(file.getPosition()==position);
    }
    file.seek(file.getSize()-10);assert(file.readBytes(buffer,100)==10);
    for(size_t i=0;i<10;++i)assert(buffer[i]==(file_size-10+i)%251);
    assert(file.readBytes(buffer,1)==0);
    file.seek(std::numeric_limits<size_t>::max());assert(file.readBytes(buffer,1)==0);
    file.seek(9000);assert(file.readBytes(nullptr,10)==0 && file.getPosition()==9000);
    file_size=8000;requests=0;
    rejected([&]{cspot::CDNAudioFile small("https://audio.scdn.co/a",key);small.openStream();});
    file_size=10000;requests=0;
    cspot::CDNAudioFile shortFile("https://audio.scdn.co/a",key);shortFile.openStream();
    shortFile.seek(shortFile.getSize()-10);assert(shortFile.readBytes(buffer,100)==10);
    file_size=50003;requests=0;mode=1;
    rejected([&]{cspot::CDNAudioFile changed("https://audio.scdn.co/a",key);changed.openStream();});
    requests=0;mode=2;
    cspot::CDNAudioFile redirect("https://audio.scdn.co/a",key);redirect.openStream();redirect.seek(9000);
    assert(redirect.readBytes(buffer,100)==0);
    std::cout<<"CDN audio control flow: header/footer bounds, changed size, range cache, final bytes, invalid seek/key and failed fetch PASS (identity cipher; no decoder/audio proof)\n";
}
