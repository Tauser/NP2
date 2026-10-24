#include "TrackReference.h"
#include <cassert>
#include <iostream>
#include <mutex>
#include <pb_encode.h>
#include <pb_decode.h>
using References=std::vector<cspot::TrackReference,SpotifyAudioAllocator<cspot::TrackReference>>;
int main()
{
    References source,destination;std::mutex mutex;
    for(unsigned i=0;i<20;++i){
        cspot::TrackReference reference;
        reference.uri="spotify:track:0000000000000000000000";
        reference.context=SpotifyAudioString(80,static_cast<char>('a'+i));
        reference.queued=(i%2)==0;
        reference.gid.assign(16,static_cast<uint8_t>(i));
        source.push_back(reference);
    }
    cspot::TrackReference::LockedTrackList locked{&mutex,&source};
    Frame outgoing=Frame_init_zero,incoming=Frame_init_zero;
    outgoing.has_state=true;outgoing.state.track.funcs.encode=&cspot::TrackReference::pbEncodeTrackList;
    outgoing.state.track.arg=&locked;
    uint8_t wire[8192]{};auto encoded=pb_ostream_from_buffer(wire,sizeof(wire));
    assert(pb_encode(&encoded,Frame_fields,&outgoing));
    incoming.state.track.funcs.decode=&cspot::TrackReference::pbDecodeTrackList;
    incoming.state.track.arg=&destination;
    auto decoded=pb_istream_from_buffer(wire,encoded.bytes_written);
    assert(pb_decode(&decoded,Frame_fields,&incoming));assert(destination.size()==source.size());
    for(size_t i=0;i<source.size();++i){
        assert(destination[i].uri==source[i].uri && destination[i].context==source[i].context);
        assert(destination[i].gid==source[i].gid && destination[i].queued==source[i].queued);
    }
    pb_release(Frame_fields,&incoming);
    cspot::TrackReference uriOnly;uriOnly.uri="spotify:track:0000000000000000000000";
    uriOnly.decodeURI();assert(uriOnly.gid==SpotifyAudioBytes(16,0));
    std::cout<<"Spotify media references: real protobuf URI/context/GID/queued roundtrip and URI decoding PASS\n";
}
