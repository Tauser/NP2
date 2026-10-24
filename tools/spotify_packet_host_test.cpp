#include "spotify_packet_policy.h"
#include "spotify_ap_verify.h"
#include "Queue.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <thread>
static void vint(std::vector<uint8_t>& v, uint32_t n) {
    do { const uint8_t byte = n & 127U; n >>= 7U; v.push_back(byte | (n ? 128U : 0U)); } while(n);
}
static std::vector<uint8_t> field(unsigned tag, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> result; vint(result, tag << 3U | 2U); vint(result, body.size());
    result.insert(result.end(), body.begin(), body.end()); return result;
}
static std::vector<uint8_t> challenge(std::vector<uint8_t> dh) {
    auto v = field(10, field(10, field(10, dh)));
    v.insert(v.begin(), 4, 0); const uint32_t n = v.size();
    v[0] = n >> 24U; v[1] = n >> 16U; v[2] = n >> 8U; v[3] = n; return v;
}
int main() {
    assert(!spotify_packet_size(0) && !spotify_packet_size(3) && spotify_packet_size(4));
    assert(spotify_packet_size(8192) && !spotify_packet_size(8193) && !spotify_packet_size(UINT32_MAX));
    uint8_t mac[4]{}; assert(spotify_packet_mac(mac, mac, 4));
    for (unsigned i=0; i<4; ++i) { uint8_t bad[4]{}; bad[i] = 1; assert(!spotify_packet_mac(mac,bad,4)); }
    assert(!spotify_packet_mac(mac,mac,3));
    std::vector<uint8_t> mercury = {0,8,0,0,0,0,0,0,0,42,1,0,2,0,1,0x52,0,3,1,2,3};
    spotify_mercury_layout_t layout;
    assert(spotify_mercury_layout(mercury.data(),mercury.size(),&layout));
    assert(layout.sequence == 42 && layout.count == 1 && layout.parts[0].size == 3);
    const uint8_t ack[]={0,8,0,0,0,0,0,0,0,42,1,0,1,0,0};
    assert(spotify_mercury_layout(ack,sizeof(ack),&layout) && !layout.header.size && !layout.count);
    for(size_t n=0;n<mercury.size();++n) assert(!spotify_mercury_layout(mercury.data(),n,&layout));
    for(auto index : {0,1,10,11,12,13,14,16,17}) {
        auto bad=mercury;bad[index]=255;assert(!spotify_mercury_layout(bad.data(),bad.size(),&layout));
    }
    auto dh=field(10,std::vector<uint8_t>(96,1));vint(dh,20U << 3U);dh.push_back(0);
    const auto signature=field(30,std::vector<uint8_t>(256,2));dh.insert(dh.end(),signature.begin(),signature.end());
    auto packet=challenge(dh);spotify_ap_challenge_t parsed{};
    assert(spotify_ap_parse(packet.data(),packet.size(),&parsed));
    assert(parsed.key[0] == 1 && parsed.signature[0] == 2);
    for(size_t n=0;n<packet.size();++n) assert(!spotify_ap_parse(packet.data(),n,&parsed));
    auto duplicate=dh;duplicate.insert(duplicate.end(),signature.begin(),signature.end());
    auto bad=challenge(duplicate);assert(!spotify_ap_parse(bad.data(),bad.size(),&parsed));
    auto unknownKey=dh;unknownKey[100]=1;bad=challenge(unknownKey);
    assert(!spotify_ap_parse(bad.data(),bad.size(),&parsed));
    int32_t status=0;
    const uint8_t statusOk[]={0x20,0x90,0x03};
    const uint8_t statusMissing[]={0x0a,0x01,'x'};
    const uint8_t duplicateStatus[]={0x20,0x90,0x03,0x20,0x90,0x03};
    const uint8_t statusNegative[]={0x20,0x01};
    assert(spotify_mercury_status(statusOk,sizeof(statusOk),&status) && status==200);
    assert(spotify_mercury_status(statusNegative,sizeof(statusNegative),&status) && status==-1);
    assert(!spotify_mercury_status(statusMissing,sizeof(statusMissing),&status));
    assert(!spotify_mercury_status(duplicateStatus,sizeof(duplicateStatus),&status));
    for(size_t n=0;n<sizeof(statusOk);++n) assert(!spotify_mercury_status(statusOk,n,&status));
    uint32_t rng=777;
    for(unsigned trial=0;trial<20000;++trial) {
        auto mutated=packet;rng=rng*1664525U+1013904223U;
        const size_t index=rng%mutated.size();rng=rng*1664525U+1013904223U;mutated[index]^=uint8_t(rng>>24U);
        if(spotify_ap_parse(mutated.data(),mutated.size(),&parsed)) {
            assert(parsed.key >= mutated.data() && parsed.key+96 <= mutated.data()+mutated.size());
            assert(parsed.signature >= mutated.data() && parsed.signature+256 <= mutated.data()+mutated.size());
        }
        (void)spotify_mercury_layout(mutated.data(),mutated.size(),&layout);
        (void)spotify_mercury_status(mutated.data(),mutated.size(),&status);
    }
    bell::Queue<int> queue;queue.push(1);queue.push(2);bool full=false;
    try {queue.push(3);}catch(const std::runtime_error&){full=true;}
    assert(full && queue.size()==2);int value=0;assert(queue.pop(value)&&value==1);
    queue.clear();assert(queue.isExit()&&queue.isEmpty()&&!queue.wtpop(value,1));
    queue.push(4);assert(!queue.isExit()&&queue.wpop(value)&&value==4);
    std::thread producer([&]{queue.push(5);});assert(queue.wtpop(value,1000)&&value==5);producer.join();
    puts("Spotify AP/Mercury: truncation, duplicate/unknown key, 20k mutations, MAC and bounded concurrent queue PASS");
}
