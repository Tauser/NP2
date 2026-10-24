"""Lifecycle/concurrency copies for the bounded, one-session Connect runtime."""
from pathlib import Path
import argparse
import re
from prepare_compat import replace_once


def generate(source: Path, output: Path):
    def read(name): return (output/name).read_text()
    def save(name,text): (output/name).write_text(text)
    text=read("TimeProvider.h")
    text="#include <atomic>\n"+text
    text=replace_once(text,"  unsigned long long timestampDiff;","  std::atomic<unsigned long long> timestampDiff{0};")
    save("TimeProvider.h",text)
    text=read("Session.h")
    text=replace_once(text,"  void close();", "  void close();\n  void releaseHandshakeState();")
    save("Session.h",text)
    text=read("MercurySession.h")
    text=replace_once(text,"  void handlePacket();",'''  bool start() {
    isRunning = true;
    if (!bell::Task::startTask()) { isRunning = false; return false; }
    return true;
  }
  void handlePacket();''')
    text=replace_once(text,"  std::mutex isRunningMutex, connectingMutex;","  std::mutex isRunningMutex, connectingMutex, pendingMutex;")
    text=replace_once(text,"  unsigned long long lastPingTimestamp = -1;","  std::atomic<unsigned long long> lastPingTimestamp{0};")
    save("MercurySession.h",text)
    text=read("MercurySession.cpp")
    text='extern "C" void np_spotify_task_fault(void);\n'+text
    text=replace_once(text,"  isRunning = true;\n","")
    start=text.index("    } catch (const std::runtime_error& e) {")
    end=text.index("\n    }",start)+6
    text=text[:start]+'''    } catch (...) {
      packetQueue.clear();
      const bool unexpected = isRunning.exchange(false);
      if (unexpected) np_spotify_task_fault();
      return; // Bounded prototype: no background reconnect/new session loop.
    }'''+text[end:]
    for signature in ("void MercurySession::unregister(uint64_t sequenceId) {",
                      "void MercurySession::unregisterAudioKey(uint32_t sequenceId) {"):
        text=replace_once(text,signature,signature+"\n  std::scoped_lock pending(pendingMutex);")
    start=text.index("      if (this->audioKeyCallbacks.count(seqId) > 0) {")
    end=text.index("\n\n      break;",start)
    text=text[:start]+'''      AudioKeyCallback callback;
      {
        std::scoped_lock pending(pendingMutex);
        const auto found = audioKeyCallbacks.find(seqId);
        if (found != audioKeyCallbacks.end()) {
          callback = std::move(found->second); audioKeyCallbacks.erase(found);
        }
      }
      if (callback) callback(static_cast<RequestType>(packet.command) ==
          RequestType::AUDIO_KEY_SUCCESS_RESPONSE, packet.data);'''+text[end:]
    start=text.index("      if (this->callbacks.count(response.sequenceId) > 0) {")
    end=text.index("\n      break;",start)
    text=text[:start]+'''      ResponseCallback callback;
      {
        std::scoped_lock pending(pendingMutex);
        const auto found = callbacks.find(response.sequenceId);
        if (found != callbacks.end()) {
          callback = std::move(found->second); callbacks.erase(found);
        }
      }
      if (callback) callback(response);'''+text[end:]
    start=text.index("      if (this->subscriptions.count(uri) > 0) {")
    end=text.index("\n      break;",start)
    text=text[:start]+'''      ResponseCallback callback;
      {
        std::scoped_lock pending(pendingMutex);
        const auto found = subscriptions.find(uri);
        if (found != subscriptions.end()) callback = found->second;
      }
      if (callback) callback(response);'''+text[end:]
    text=replace_once(text,'''  if (method == RequestType::SUB) {
    this->subscriptions.insert({uri, subscription});
  }''', '''  {
    std::scoped_lock pending(pendingMutex);
    if (callbacks.size() >= 16 || uri.size() > 256 || payload.size() > 16)
      throw std::runtime_error("Spotify pending requests full");
    if (method == RequestType::SUB) {
      if (subscriptions.size() >= 2 && subscriptions.find(uri) == subscriptions.end())
        throw std::runtime_error("Spotify subscriptions full");
      subscriptions[uri] = subscription;
    }
  }''')
    text=replace_once(text,"  this->callbacks.insert({sequenceId, callback});",'''  {
    std::scoped_lock pending(pendingMutex);
    this->callbacks.insert({sequenceId, callback});
  }''')
    text=replace_once(text,"  this->audioKeyCallbacks.insert({this->audioKeySequence, audioCallback});",'''  {
    std::scoped_lock pending(pendingMutex);
    if (audioKeyCallbacks.size() >= 4 || trackId.size() != 16 || fileId.size() != 20)
      throw std::runtime_error("Spotify audio request invalid");
    this->audioKeyCallbacks.insert({this->audioKeySequence, audioCallback});
  }''')
    text=replace_once(text,"    // @TODO: handle disconnect", "    unregister(this->sequenceId - 1); throw;")
    text=replace_once(text,"    // @TODO: Handle disconnect", "    unregisterAudioKey(this->audioKeySequence - 1); throw;")
    # failAllPending is no longer called by the reader. It must not execute
    # callbacks while another task owns/rebuilds the pending maps.
    start=text.index("void MercurySession::failAllPending() {")
    end=text.index("MercurySession::Response MercurySession::decodeResponse",start)
    text=text[:start]+'''void MercurySession::failAllPending() {
  std::scoped_lock pending(pendingMutex);
  callbacks.clear(); subscriptions.clear(); audioKeyCallbacks.clear();
}

'''+text[end:]
    text=replace_once(text,"  std::scoped_lock lock(this->isRunningMutex);\n}\n\nstd::string MercurySession::getCountryCode", "  std::scoped_lock lock(this->isRunningMutex);\n  failAllPending();\n}\n\nstd::string MercurySession::getCountryCode")
    save("MercurySession.cpp",text)

    text=read("TrackQueue.h")
    text=replace_once(text,"class QueuedTrack {","class QueuedTrack : public std::enable_shared_from_this<QueuedTrack> {")
    text=replace_once(text,"  State state = State::QUEUED;","  std::atomic<State> state = State::QUEUED;")
    text=replace_once(text,"  bool loading = false;","  std::atomic<bool> loading = false;")
    text=replace_once(text,"  uint32_t requestedPosition;","  std::atomic<uint32_t> requestedPosition{0};")
    text=replace_once(text,"  bool isRunning = false;","  std::atomic<bool> isRunning = false;")
    text=replace_once(text,"  void runTask() override;",'''  bool start() {
    isRunning = true;
    if (!bell::Task::startTask()) { isRunning = false; return false; }
    return true;
  }
  void runTask() override;''')
    save("TrackQueue.h",text)
    text=read("TrackQueue.cpp")
    text=replace_once(text,"  startTask();","  // Runtime starts only after all callbacks and collaborators are ready.")
    text=replace_once(text,"  isRunning = true;\n","")
    text=replace_once(text,"[this, &trackListMutex, updateSemaphore]", "[this, weak = weak_from_this(), &trackListMutex, updateSemaphore]")
    text=replace_once(text,"[this, pbTrack, pbEpisode, &trackListMutex,", "[this, weak = weak_from_this(), pbTrack, pbEpisode, &trackListMutex,")
    text=replace_once(text,"bool success, const std::vector<uint8_t>& audioKey) {", "bool success, const std::vector<uint8_t>& audioKey) {\n        auto alive = weak.lock(); if (!alive) return;")
    text=replace_once(text,"updateSemaphore](MercurySession::Response& res) {", "updateSemaphore](MercurySession::Response& res) {\n    auto alive = weak.lock(); if (!alive) return;")
    text=replace_once(text,"    stepParseMetadata(pbTrack, pbEpisode);",'''    try { stepParseMetadata(pbTrack, pbEpisode); }
    catch (...) {
      pb_release(Track_fields, pbTrack); *pbTrack = Track_init_zero;
      pb_release(Episode_fields, pbEpisode); *pbEpisode = Episode_init_zero;
      throw;
    }
    // Identifiers/display metadata were copied above. Do not retain the raw
    // protobuf heap while a different track starts its HTTPS/TLS request.
    pb_release(Track_fields, pbTrack); *pbTrack = Track_init_zero;
    pb_release(Episode_fields, pbEpisode); *pbEpisode = Episode_init_zero;''')
    text=replace_once(text,"  pendingMercuryRequest = ctx->session->execute(","  state = State::PENDING_META;\n  pendingMercuryRequest = ctx->session->execute(")
    text=replace_once(text,"  // Set the state to pending\n  state = State::PENDING_META;","")
    text=replace_once(text,"  this->pendingAudioKeyRequest = ctx->session->requestAudioKey(","  state = State::PENDING_KEY;\n  this->pendingAudioKeyRequest = ctx->session->requestAudioKey(")
    text=replace_once(text,"\n  state = State::PENDING_KEY;\n}","\n}")
    text=replace_once(text,"  uint16_t countryList_length = strlen(countryList);",'''  if (!countryList || !country || strlen(country) != 2) return false;
  size_t countryList_length = strlen(countryList);
  if (countryList_length % 2) return false;''')
    text=replace_once(text,"  name = std::string(pbTrack->name);",'  name = pbTrack->name ? pbTrack->name : "";')
    text=replace_once(text,"    artist = std::string(pbTrack->artist[0].name);",'    artist = pbTrack->artist[0].name ? pbTrack->artist[0].name : "";')
    text=replace_once(text,"    album = std::string(pbTrack->album.name);",'    album = pbTrack->album.name ? pbTrack->album.name : "";')
    text=replace_once(text,"  name = std::string(pbEpisode->name);",'  name = pbEpisode->name ? pbEpisode->name : "";')
    text=replace_once(text,"  if (pbEpisode->covers->image_count > 0)","  if (pbEpisode->covers && pbEpisode->covers->image_count > 0)")
    text=replace_once(text,"  currentTracks = playbackState->remoteTracks;",'''  if (playbackState->remoteTracks.size() > 128 ||
      playbackState->remoteFrame.state.playing_track_index >= playbackState->remoteTracks.size())
    throw std::runtime_error("Spotify track index invalid");
  currentTracks = playbackState->remoteTracks;''')
    text=replace_once(text,"  int alternativeCount, filesCount = 0;\n  bool canPlay = false;", "  int filesCount = 0;")
    save("TrackQueue.cpp",text)

    text=read("MercurySession.cpp")
    text='#include "SpotifyMediaDiagnostics.h"\n'+text
    text=replace_once(text,"  pbDecode(response.mercuryHeader, Header_fields, headerBytes);",'''  int32_t diagnosticStatus=0;
  if(spotify_mercury_status(headerBytes.data(),headerBytes.size(),&diagnosticStatus))
    spotifyMediaDiagnostic(3,diagnosticStatus,static_cast<unsigned>(response.parts.size()));
  pbDecode(response.mercuryHeader, Header_fields, headerBytes);''')
    save("MercurySession.cpp",text)
    text=read("SpircHandler.cpp")
    text='#include "SpotifyMediaDiagnostics.h"\n'+text
    text=replace_once(text,"  playbackState->decodeRemoteFrame(data);",'''  playbackState->decodeRemoteFrame(data);
  spotifyMediaDiagnostic(1,static_cast<long>(playbackState->remoteFrame.typ),
    static_cast<unsigned>(playbackState->remoteTracks.size()));''')
    save("SpircHandler.cpp",text)
    text=read("TrackQueue.h")
    text=replace_once(text,"  TrackReference ref;", "  int diagnosticState=-1;\n  TrackReference ref;")
    save("TrackQueue.h",text)
    text=read("TrackQueue.cpp")
    text='#include "SpotifyMediaDiagnostics.h"\n'+text
    text=replace_once(text,"  switch (track->state) {",'''  const int diagnosticState=static_cast<int>(track->state.load());
  if(track->diagnosticState!=diagnosticState){
    track->diagnosticState=diagnosticState;spotifyMediaDiagnostic(2,diagnosticState,0);
  }
  switch (track->state) {''')
    text=replace_once(text,"    // Parse the metadata",'''    spotifyMediaDiagnostic(5,static_cast<long>(res.parts.size()),
      res.parts.empty()?0U:static_cast<unsigned>(res.parts[0].size()));
    // Parse the metadata''')
    text=replace_once(text,"        if (success) {",'''        spotifyMediaDiagnostic(6,success?1:0,static_cast<unsigned>(audioKey.size()));
        if(!success) spotifyMediaDiagnostic(9,audioKey.size()==6 ?
          static_cast<long>((static_cast<unsigned>(audioKey[4])<<8U)|audioKey[5]) : -1,0);
        if (success) {''')
    text=replace_once(text,"  // No viable files found for playback",'''  unsigned formats=0;
  for(int i=0;i<filesCount;++i) if(static_cast<unsigned>(selectedFiles[i].format)<32)
    formats|=1U<<static_cast<unsigned>(selectedFiles[i].format);
  spotifyMediaDiagnostic(4,filesCount,formats);
  // No viable files found for playback''')
    save("TrackQueue.cpp",text)
    text=read("TrackPlayer.cpp")
    text='#include <chrono>\n#include "SpotifyMediaDiagnostics.h"\n'+text
    text=replace_once(text,"      track->loadedSemaphore->twait(5000);",'''      const auto loadDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
      while(track->state!=QueuedTrack::State::READY && track->state!=QueuedTrack::State::FAILED &&
            isRunning && !pendingReset && std::chrono::steady_clock::now()<loadDeadline)
        track->loadedSemaphore->twait(100);
      if(!isRunning || pendingReset) continue;
      if(track->state!=QueuedTrack::State::READY)
        spotifyMediaDiagnostic(7,static_cast<long>(track->state.load()),0);''')
    text=replace_once(text,'        CSPOT_LOG(error, "Failed to open audio stream: %s", e.what());',
        '        spotifyMediaDiagnostic(8,1,0);')
    save("TrackPlayer.cpp",text)

    text=read("SpircHandler.h")
    text="#include <mutex>\n"+text
    text=replace_once(text,"  EventHandler eventHandler = nullptr;","  EventHandler eventHandler = nullptr;\n  std::recursive_mutex controlMutex;")
    save("SpircHandler.h",text)
    text=read("SpircHandler.cpp")
    text=replace_once(text,'    this->ctx->config.countryCode = this->ctx->session->getCountryCode();',
        '    if (ctx->config.countryCode.empty()) ctx->config.countryCode = ctx->session->getCountryCode();')
    guarded={"subscribeToMercury","notifyAudioEnded","notifyAudioReachedPlayback","updatePositionMs",
             "handleFrame","setRemoteVolume","notify","sendCmd","setEventHandler","skipSong",
             "previousSong","nextSong","setPause","sendEvent"}
    pattern=re.compile(r"((?:void|bool) SpircHandler::(\w+)\([^{}]*?\)\s*\{)",re.S)
    text=pattern.sub(lambda m:m[0]+("\n  std::scoped_lock control(controlMutex);" if m[2] in guarded else ""),text)
    text=replace_once(text,"                                    bool paused = false) {", "                                    bool paused = false) {\n    std::scoped_lock control(controlMutex);")
    save("SpircHandler.cpp",text)

    text=read("TrackPlayer.h")
    text=replace_once(text,"  std::atomic<bool> isRunning = false;", "  std::atomic<bool> isRunning = false;\n  std::atomic<bool> workerStarted = false, destroying = false;")
    text=replace_once(text,"  std::atomic<bool> currentSongPlaying;", "  std::atomic<bool> currentSongPlaying{false};")
    save("TrackPlayer.h",text)
    text=read("TrackPlayer.cpp")
    text=replace_once(text,"      if (r != 0) { currentSongPlaying = false; continue; }",'''      if (r == SPOTIFY_VORBIS_RESOURCE) throw std::runtime_error("Spotify decoder resource failure");
      if (r != 0) { currentSongPlaying = false; continue; }''')
    text=replace_once(text,"        const vorbis_info *sectionFormat",'''        if (ret == SPOTIFY_VORBIS_RESOURCE) throw std::runtime_error("Spotify decoder resource failure");
        const vorbis_info *sectionFormat''')
    text=replace_once(text,"  catch (...) { currentSongPlaying = false; return 0; }", "  catch (...) { currentSongPlaying = false; np_spotify_task_fault(); return 0; }")
    text=replace_once(text,"TrackPlayer::~TrackPlayer() {\n  isRunning = false;", "TrackPlayer::~TrackPlayer() {\n  destroying = true;\n  isRunning = false;")
    start=text.index("void TrackPlayer::start() {")
    end=text.index("void TrackPlayer::resetState(",start)
    text=text[:start]+'''void TrackPlayer::start() {
  isRunning = true;
  if (!workerStarted.exchange(true) && !startTask()) {
    isRunning = false;
    throw std::runtime_error("Spotify decoder task unavailable");
  }
}
void TrackPlayer::stop() {
  isRunning = false;
  resetState(true); // Never join while Spirc holds its control mutex.
}

'''+text[end:]
    text=replace_once(text,"  while (isRunning) {",'''  while (!destroying) {
    if (!isRunning) { BELL_SLEEP_MS(50); continue; }''')
    save("TrackPlayer.cpp",text)

    text=(source/"src/PlaybackState.cpp").read_text()
    text="#include <stdexcept>\n"+text
    old='''  innerFrame.state.context_uri = (char*)realloc(
      innerFrame.state.context_uri, strlen(remoteFrame.state.context_uri) + 1);

  strcpy(innerFrame.state.context_uri, remoteFrame.state.context_uri);'''
    text=replace_once(text,old,'''  const char *value = remoteFrame.state.context_uri ? remoteFrame.state.context_uri : "";
  const size_t length = strlen(value);
  if (length > 512) throw std::runtime_error("Spotify context invalid");
  char *replacement = static_cast<char*>(realloc(innerFrame.state.context_uri, length + 1));
  if (!replacement) throw std::bad_alloc();
  innerFrame.state.context_uri = replacement;
  memcpy(replacement, value, length + 1);''')
    save("PlaybackState.cpp",text)
    text=(source/"src/TrackReference.cpp").read_text()
    text="#include <stdexcept>\n"+text
    text=replace_once(text,"  // Push a new reference",'''  if (trackQueue->size() >= 128) return false;
  // Push a new reference''')
    text=replace_once(text,"          uint8_t* destBuffer = nullptr;",'''          if ((tag == TrackRef_gid_tag && substream.bytes_left != 16) ||
              (tag != TrackRef_gid_tag && substream.bytes_left > 512)) return false;
          uint8_t* destBuffer = nullptr;''')
    text=replace_once(text,"    gid = {0};",'''    if (idString.size() != 22 || (uri.find("spotify:track:") != 0 && uri.find("spotify:episode:") != 0))
      throw std::runtime_error("Spotify track URI invalid");
    gid = {0};''')
    text=replace_once(text,"      size_t d = alphabet.find(idString[x]);",'''      size_t d = alphabet.find(idString[x]);
      if (d == std::string_view::npos) throw std::runtime_error("Spotify track URI invalid");''')
    text=replace_once(text,"    if (uri.find(\"episode:\")",'''    if (gid.size() > 16) throw std::runtime_error("Spotify GID invalid");
    gid.insert(gid.begin(), 16 - gid.size(), 0);
    if (uri.find("episode:")''')
    text=replace_once(text,"          pb_skip_field(stream, wire_type);", "          if (!pb_skip_field(stream, wire_type)) return false;")
    save("TrackReference.cpp",text)
    text=read("Session.cpp")
    text=replace_once(text,"  this->conn->close();","  if (this->conn) this->conn->close();")
    text=replace_once(text,"  authBlob = blob;", "  authBlob = blob;\n  deviceId = blob->getDeviceId();")
    text=replace_once(text,"  // Send login request",'''  struct WipeAuthRequest {
    std::vector<uint8_t>& bytes;
    ~WipeAuthRequest() { volatile uint8_t *p = bytes.data(); for (size_t i=0;i<bytes.size();++i) p[i]=0; }
  } wipeRequest{data};
  // Send login request''')
    text += '''\nvoid Session::releaseHandshakeState() {
  challenges.reset(); authBlob.reset();
}
'''
    save("Session.cpp",text)
    text=read("CDNAudioFile.h")
    text=replace_once(text,"  const int OPUS_FOOTER_PREFFERED = 1024 * 12;", "  const int OPUS_FOOTER_PREFFERED = 1024 * 8;")
    text=replace_once(text,"  const int HTTP_BUFFER_SIZE = 1024 * 14;", "  const int HTTP_BUFFER_SIZE = 1024 * 8;")
    save("CDNAudioFile.h",text)

    # Decoded playlist containers and URI/context text are bounded media
    # metadata, not wire HTTP bodies, JSON, TLS or account credentials.
    for name in ("TrackReference.h","TrackQueue.h","PlaybackState.h","TrackReference.cpp"):
        text=read(name)
        text='#include "SpotifyAudioAllocator.h"\n'+text
        text=replace_once(text,"std::vector<TrackReference>",
            "std::vector<TrackReference, SpotifyAudioAllocator<TrackReference>>")
        if name=="TrackReference.h":
            text=replace_once(text,"  std::string uri, context;","  SpotifyAudioString uri, context;")
            text=replace_once(text,"  std::vector<uint8_t> gid;","  SpotifyAudioBytes gid;")
        if name=="TrackReference.cpp":
            text=replace_once(text,"  msg.context.funcs.encode = &bell::nanopb::encodeString;",'''  auto encodeMediaText = +[](pb_ostream_t* stream,const pb_field_t* field,void* const* arg) -> bool {
    const auto& value=*static_cast<const SpotifyAudioString*>(*arg);
    return value.empty() || (pb_encode_tag_for_field(stream,field) &&
      pb_encode_string(stream,reinterpret_cast<const pb_byte_t*>(value.data()),value.size()));
  };
  msg.context.funcs.encode = encodeMediaText;''')
            text=replace_once(text,"  msg.uri.funcs.encode = &bell::nanopb::encodeString;", "  msg.uri.funcs.encode = encodeMediaText;")
            text=replace_once(text,"  msg.gid.funcs.encode = &bell::nanopb::encodeVector;",'''  msg.gid.funcs.encode = +[](pb_ostream_t* stream,const pb_field_t* field,void* const* arg) -> bool {
    const auto& value=*static_cast<const SpotifyAudioBytes*>(*arg);
    return value.empty() || (pb_encode_tag_for_field(stream,field) &&
      pb_encode_string(stream,value.data(),value.size()));
  };''')
            text=replace_once(text,"    gid = {0};","    std::vector<uint8_t> decodedGid = {0};")
            text=replace_once(text,"      gid = bigNumMultiply(gid, 62);","      decodedGid = bigNumMultiply(decodedGid, 62);")
            text=replace_once(text,"      gid = bigNumAdd(gid, d);","      decodedGid = bigNumAdd(decodedGid, d);")
            text=replace_once(text,"    if (gid.size() > 16)","    gid.assign(decodedGid.begin(),decodedGid.end());\n    if (gid.size() > 16)")
            text=replace_once(text,"  for (auto trackRef : trackQueue)","  for (const auto& trackRef : trackQueue)")
            for member in ("gid","uri","context","queued"):
                text=replace_once(text,f"    msg.{member}.arg = &trackRef.{member};",
                    f"    msg.{member}.arg = const_cast<void*>(static_cast<const void*>(&trackRef.{member}));")
        save(name,text)
    text=read("TrackPlayer.h")
    text=replace_once(text,"  std::vector<uint8_t> pcmBuffer = std::vector<uint8_t>(1024);",
        "  SpotifyAudioBytes pcmBuffer = SpotifyAudioBytes(1024);")
    save("TrackPlayer.h",text)
    text=read("TrackQueue.cpp")
    text=replace_once(text,"      bytesToHexString(ref.gid).c_str());",
        "      bytesToHexString(std::vector<uint8_t>(ref.gid.begin(),ref.gid.end())).c_str());")
    save("TrackQueue.cpp",text)


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path);parser.add_argument("output",type=Path)
    args=parser.parse_args();generate(args.source,args.output)
