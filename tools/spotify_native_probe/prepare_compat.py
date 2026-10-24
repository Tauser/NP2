"""Generate bounded compatibility copies; never mutate the pinned checkout."""
import argparse
from pathlib import Path


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("Pinned Spotify source no longer matches a patch anchor")
    return text.replace(old, new, 1)


def generate(source: Path, output: Path):
    output.mkdir(parents=True, exist_ok=True)
    # Copy all mutually including cspot headers so quote-includes cannot mix
    # different class layouts after changing private audio member allocators.
    for header in (source / "include").glob("*.h"):
        (output / header.name).write_text(header.read_text(encoding="utf-8"), encoding="utf-8")
    text = (output / "CDNAudioFile.h").read_text(encoding="utf-8")
    text = '#include "SpotifyAudioAllocator.h"\n' + text
    text = text.replace('std::vector<uint8_t> header = std::vector<uint8_t>', 'SpotifyAudioBytes header = SpotifyAudioBytes')
    text = text.replace('std::vector<uint8_t> footer;', 'SpotifyAudioBytes footer;')
    text = text.replace('std::vector<uint8_t> httpBuffer = std::vector<uint8_t>', 'SpotifyAudioBytes httpBuffer = SpotifyAudioBytes')
    (output / "CDNAudioFile.h").write_text(text, encoding="utf-8")
    text = (output / "TrackPlayer.h").read_text(encoding="utf-8")
    text = '#include "spotify_vorbis_guard.h"\n' + text
    text = replace_once(text, '  OggVorbis_File vorbisFile;', '  OggVorbis_File vorbisFile;\n  spotify_vorbis_guard_t *decoder = nullptr;')
    (output / "TrackPlayer.h").write_text(text, encoding="utf-8")
    # A private header overlay is used by every foreign caller. Core firmware
    # does not inherit these exception-based nanopb wrappers.
    helper = source / "bell/main/utilities"
    text = (helper / "include/NanoPBHelper.h").read_text(encoding="utf-8")
    text = '#include <stdexcept>\n#include <cstring>\n' + text
    text = text.replace('&data[0]', 'data.data()')
    text = text.replace('''    printf("Decode failed: %s\\n", PB_GET_ERROR(&stream));''', '''    pb_release(fields, &result);
    std::memset(&result, 0, sizeof(result));
    throw std::runtime_error("Spotify protobuf invalid");''')
    text = replace_once(text, 'void pbPutString(const std::string& stringToPack, char* dst);', '''template<size_t N> void pbPutString(const std::string& value, char (&dst)[N]) {
  if (value.size() >= N || value.find('\\0') != std::string::npos)
    throw std::runtime_error("Spotify protobuf field invalid");
  std::memcpy(dst, value.data(), value.size()); dst[value.size()] = 0;
}''')
    (output / "NanoPBHelper.h").write_text(text, encoding="utf-8")
    text = (helper / "NanoPBHelper.cpp").read_text(encoding="utf-8")
    text = replace_once(text, '  dest->insert(dest->end(), buf, buf + count);', '''  if (count > 8192 || dest->size() > 8192 - count) return false;
  dest->insert(dest->end(), buf, buf + count);''')
    text = replace_once(text, '  stream.max_size = 100000;', '  stream.max_size = 8192;')
    text = replace_once(text, '  pb_encode(&stream, fields, src_struct);', '''  if (!pb_encode(&stream, fields, src_struct))
    throw std::runtime_error("Spotify protobuf encode failed");''')
    text = replace_once(text, '  return std::vector<uint8_t>(pbArray->bytes, pbArray->bytes + pbArray->size);', '''  if (!pbArray || pbArray->size > 8192) throw std::runtime_error("Spotify protobuf bytes invalid");
  return std::vector<uint8_t>(pbArray->bytes, pbArray->bytes + pbArray->size);''')
    start = text.index('void pbPutString(')
    end = text.index('void pbPutCharArray(', start)
    text = text[:start] + text[end:]
    (output / "NanoPBHelper.cpp").write_text(text, encoding="utf-8")

    text = (helper / "Crypto.cpp").read_text(encoding="utf-8")
    text = replace_once(text, 'CryptoMbedTLS::CryptoMbedTLS() {}', '''CryptoMbedTLS::CryptoMbedTLS() { mbedtls_md_init(&sha1Context); }''')
    text = replace_once(text, 'CryptoMbedTLS::~CryptoMbedTLS() {', '''CryptoMbedTLS::~CryptoMbedTLS() {
  mbedtls_md_free(&sha1Context);
  volatile uint8_t *secret = privateKey.data();
  for (size_t i = 0; i < privateKey.size(); ++i) secret[i] = 0;''')
    text = replace_once(text, '''  mbedtls_md_setup(&sha1Context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_starts(&sha1Context);''', '''  if (mbedtls_md_setup(&sha1Context, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1) != 0 ||
      mbedtls_md_starts(&sha1Context) != 0) {
    mbedtls_md_free(&sha1Context); throw std::runtime_error("Spotify digest unavailable");
  }''')
    text = replace_once(text, '  mbedtls_md_update(&sha1Context, vec.data(), vec.size());', '''  if (mbedtls_md_update(&sha1Context, vec.data(), vec.size()) != 0) {
    mbedtls_md_free(&sha1Context); throw std::runtime_error("Spotify digest unavailable");
  }''')
    text = replace_once(text, '''  mbedtls_md_finish(&sha1Context, digest.data());
  mbedtls_md_free(&sha1Context);''', '''  const int result = mbedtls_md_finish(&sha1Context, digest.data());
  mbedtls_md_free(&sha1Context);
  if (result) throw std::runtime_error("Spotify digest unavailable");''')
    text = replace_once(text, '''  mbedtls_md_hmac_starts(&sha1Context, inputKey.data(), inputKey.size());
  mbedtls_md_hmac_update(&sha1Context, message.data(), message.size());
  mbedtls_md_hmac_finish(&sha1Context, digest.data());
  mbedtls_md_free(&sha1Context);''', '''  const bool valid = mbedtls_md_hmac_starts(&sha1Context, inputKey.data(), inputKey.size()) == 0 &&
      mbedtls_md_hmac_update(&sha1Context, message.data(), message.size()) == 0 &&
      mbedtls_md_hmac_finish(&sha1Context, digest.data()) == 0;
  mbedtls_md_free(&sha1Context);
  if (!valid) throw std::runtime_error("Spotify MAC unavailable");''')
    text = replace_once(text, '''  if (!aesCtxInitialized) {''', '''  if (key.size() != 16 || iv.size() != 16 || (!buffer && nbytes))
    throw std::runtime_error("Spotify AES input invalid");
  if (!aesCtxInitialized) {''')
    text = replace_once(text, '''  struct AES_ctx aesCtr;''', '''  if (key.size() != 24 || data.empty() || data.size() % 16)
    throw std::runtime_error("Spotify AES input invalid");
  struct AES_ctx aesCtr;''')
    text = replace_once(text, '''  mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, password.data(),
                                password.size(), salt.data(), salt.size(),
                                iterations, digestSize, digest.data());''', '''  if (mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA1, password.data(),
                                password.size(), salt.data(), salt.size(),
                                iterations, digestSize, digest.data()) != 0)
    throw std::runtime_error("Spotify derivation unavailable");''')
    start = text.index('  // Calculate max decode length')
    end = text.index('\n}\n', start)
    text = text[:start] + '''  if (data.empty() || data.size() > 2800) throw std::runtime_error("Spotify base64 invalid");
  size_t required = 0;
  const int check = mbedtls_base64_decode(nullptr, 0, &required,
      reinterpret_cast<const unsigned char*>(data.data()), data.size());
  if (check != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || !required || required > 2048)
    throw std::runtime_error("Spotify base64 invalid");
  std::vector<uint8_t> output(required); size_t written = 0;
  if (mbedtls_base64_decode(output.data(), output.size(), &written,
      reinterpret_cast<const unsigned char*>(data.data()), data.size()) != 0 || written != required)
    throw std::runtime_error("Spotify base64 invalid");
  return output;''' + text[end:]
    text = replace_once(text, '''  mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                        (const unsigned char*)pers, 7);

  // Generate random bytes
  mbedtls_ctr_drbg_random(&ctrDrbg, randomVector.data(), length);''', '''  const int seeded = mbedtls_ctr_drbg_seed(&ctrDrbg, mbedtls_entropy_func, &entropy,
                        (const unsigned char*)pers, 7);
  const int generated = seeded ? seeded : mbedtls_ctr_drbg_random(&ctrDrbg, randomVector.data(), length);''')
    text = replace_once(text, '''  return randomVector;''', '''  if (generated) throw std::runtime_error("Spotify entropy unavailable");
  return randomVector;''')
    start = text.index('  // Read bin into big num mpi', text.index('void CryptoMbedTLS::dhInit()'))
    end = text.index('\n}\n', start)
    text = text[:start] + '''  const bool valid = mbedtls_mpi_read_binary(&prime, DHPrime, sizeof(DHPrime)) == 0 &&
      mbedtls_mpi_read_binary(&generator, DHGenerator, sizeof(DHGenerator)) == 0 &&
      mbedtls_mpi_read_binary(&privKey, privateKey.data(), DH_KEY_SIZE) == 0 &&
      mbedtls_mpi_exp_mod(&res, &generator, &privKey, &prime, NULL) == 0;
  publicKey.resize(DH_KEY_SIZE);
  const int written = valid ? mbedtls_mpi_write_binary(&res, publicKey.data(), DH_KEY_SIZE) : -1;
  mbedtls_mpi_free(&prime); mbedtls_mpi_free(&generator);
  mbedtls_mpi_free(&privKey); mbedtls_mpi_free(&res);
  if (written) throw std::runtime_error("Spotify key generation failed");''' + text[end:]
    start = text.index('  // initialize big num', text.index('CryptoMbedTLS::dhCalculateShared('))
    end = text.index('\n}\n', start)
    text = text[:start] + '''  if (remoteKey.size() != DH_KEY_SIZE || privateKey.size() != DH_KEY_SIZE)
    throw std::runtime_error("Spotify remote key invalid");
  mbedtls_mpi prime, remote, result, privateValue;
  mbedtls_mpi_init(&prime); mbedtls_mpi_init(&remote);
  mbedtls_mpi_init(&result); mbedtls_mpi_init(&privateValue);
  bool valid = mbedtls_mpi_read_binary(&prime, DHPrime, sizeof(DHPrime)) == 0 &&
      mbedtls_mpi_read_binary(&remote, remoteKey.data(), remoteKey.size()) == 0 &&
      mbedtls_mpi_cmp_int(&remote, 1) > 0 && mbedtls_mpi_sub_int(&result, &prime, 1) == 0 &&
      mbedtls_mpi_cmp_mpi(&remote, &result) < 0 &&
      mbedtls_mpi_read_binary(&privateValue, privateKey.data(), privateKey.size()) == 0 &&
      mbedtls_mpi_exp_mod(&result, &remote, &privateValue, &prime, NULL) == 0;
  std::vector<uint8_t> shared(DH_KEY_SIZE);
  const int written = valid ? mbedtls_mpi_write_binary(&result, shared.data(), shared.size()) : -1;
  mbedtls_mpi_free(&prime); mbedtls_mpi_free(&remote);
  mbedtls_mpi_free(&result); mbedtls_mpi_free(&privateValue);
  if (written) throw std::runtime_error("Spotify remote key invalid");
  return shared;''' + text[end:]
    (output / "Crypto.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/AuthChallenges.cpp").read_text(encoding="utf-8")
    text = '#include "spotify_ap_verify.h"\n#include <stdexcept>\n' + text
    text = replace_once(text, '''  // Decode the response''', '''  if (!spotify_ap_verify(data.data(), data.size()))
    throw std::runtime_error("Spotify server authentication failed");
  // Decode the response''')
    text = replace_once(text, '''  std::copy(authData.begin(), authData.end(),''', '''  if (authData.empty() || authData.size() > sizeof(authRequest.login_credentials.auth_data.bytes))
    throw std::runtime_error("Spotify authentication invalid");
  std::copy(authData.begin(), authData.end(),''')
    (output / "AuthChallenges.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/Session.cpp").read_text(encoding="utf-8")
    text = replace_once(text, '      APWelcome welcome;', '      APWelcome welcome{};')
    (output / "Session.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/ShannonConnection.cpp").read_text(encoding="utf-8")
    text = '#include "spotify_packet_policy.h"\n#include <stdexcept>\n' + text
    text = replace_once(text, '''  auto packetData = std::vector<uint8_t>(readSize);''', '''  if (readSize > SPOTIFY_PACKET_MAX) throw std::runtime_error("Spotify packet invalid");
  auto packetData = std::vector<uint8_t>(readSize);''')
    text = replace_once(text, '''  if (mac != mac2) {
    CSPOT_LOG(error, "Shannon read: Mac doesn't match");
  }''', '''  if (!spotify_packet_mac(mac.data(), mac2.data(), mac.size()))
    throw std::runtime_error("Spotify packet invalid");''')
    text = replace_once(text, '''  auto sizeRaw = pack<uint16_t>(htons(uint16_t(data.size())));''', '''  if (data.size() > SPOTIFY_PACKET_MAX) throw std::runtime_error("Spotify packet invalid");
  auto sizeRaw = pack<uint16_t>(htons(uint16_t(data.size())));''')
    (output / "ShannonConnection.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/MercurySession.cpp").read_text(encoding="utf-8")
    text = '#include "spotify_packet_policy.h"\n' + text
    text = replace_once(text, '''        timeProvider->syncWithPingPacket(packet.data);''', '''        if (packet.data.size() != 4) throw std::runtime_error("Spotify packet invalid");
        timeProvider->syncWithPingPacket(packet.data);''')
    text = replace_once(text, '''      this->countryCode = std::string();''', '''      if (packet.data.size() != 2) throw std::runtime_error("Spotify packet invalid");
      this->countryCode = std::string();''')
    text = replace_once(text, '''      auto seqId = ntohl(extract<uint32_t>(packet.data, 0));''', '''      if (packet.data.size() < 4 ||
          (static_cast<RequestType>(packet.command) == RequestType::AUDIO_KEY_SUCCESS_RESPONSE && packet.data.size() != 20))
        throw std::runtime_error("Spotify packet invalid");
      auto seqId = ntohl(extract<uint32_t>(packet.data, 0));''')
    start = text.index('  auto sequenceLength =')
    end = text.index('  pbDecode(response.mercuryHeader', start)
    text = text[:start] + '''  spotify_mercury_layout_t layout{};
  if (!spotify_mercury_layout(data.data(), data.size(), &layout))
    throw std::runtime_error("Spotify packet invalid");
  response.sequenceId = layout.sequence;
  auto headerBytes = std::vector<uint8_t>(data.begin() + layout.header.offset,
      data.begin() + layout.header.offset + layout.header.size);
  for (size_t i = 0; i < layout.count; ++i) {
    const auto part = layout.parts[i];
    response.parts.emplace_back(data.begin() + part.offset, data.begin() + part.offset + part.size);
  }

''' + text[end:]
    # A full mailbox invalidates the connection and discards stale old packets.
    text = replace_once(text, '      failAllPending();', '      packetQueue.clear();\n      failAllPending();')
    (output / "MercurySession.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/SpircHandler.cpp").read_text(encoding="utf-8")
    text = text.replace('std::make_shared<PlaybackState>', 'std::allocate_shared<PlaybackState>')
    text = text.replace('std::allocate_shared<PlaybackState>(ctx)', 'std::allocate_shared<PlaybackState>(SpotifyAudioAllocator<PlaybackState>{}, ctx)')
    text = text.replace('std::make_shared<cspot::TrackQueue>(ctx, playbackState)', 'std::allocate_shared<cspot::TrackQueue>(SpotifyAudioAllocator<cspot::TrackQueue>{}, ctx, playbackState)')
    text = replace_once(text, '    this->handleFrame(res.parts[0]);', '''    if (res.parts.size() != 1 || res.parts[0].empty()) return;
    this->handleFrame(res.parts[0]);''')
    (output / "SpircHandler.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/TimeProvider.cpp").read_text(encoding="utf-8")
    text = '#include <stdexcept>\n' + text
    text = replace_once(text, '  // Spotify\'s timestamp', '''  if (pongPacket.size() != 4) throw std::runtime_error("Spotify packet invalid");
  // Spotify's timestamp''')
    (output / "TimeProvider.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/LoginBlob.cpp").read_text(encoding="utf-8")
    text = '#include "spotify_login_policy.h"\n#include <stdexcept>\n' + text
    text = replace_once(text, '''  // 0:16 - iv; 17:-20 - blob; -20:0 - checksum''', '''  if (blob.size() < 37 || blob.size() > 2048 || sharedKey.empty() || sharedKey.size() > 128)
    throw std::runtime_error("Spotify pairing invalid");
  // 0:16 - iv; 17:-20 - blob; -20:0 - checksum''')
    text = replace_once(text, '''  if (mac != checksum) {
    CSPOT_LOG(error, "Mac doesn't match!");
  }''', '''  if (!spotify_login_mac(mac.data(), mac.size(), checksum.data(), checksum.size()))
    throw std::runtime_error("Spotify pairing invalid");''')
    text = replace_once(text, '''  auto blobData = crypto->base64Decode(encryptedString);''', '''  if (blob.size() > 2048) throw std::runtime_error("Spotify pairing invalid");
  auto blobData = crypto->base64Decode(encryptedString);
  if (blobData.size() < 16 || blobData.size() > 2048 || blobData.size() % 16)
    throw std::runtime_error("Spotify pairing invalid");''')
    first = text.index("  // Parse blob\n")
    end = text.index("\n}\n", first)
    text = text[:first] + '''  spotify_login_auth_t fields{};
  if (!spotify_login_username(username.data(), username.size()) ||
      !spotify_login_auth(loginData.data(), loginData.size(), &fields))
    throw std::runtime_error("Spotify pairing invalid");
  this->authType = fields.type;
  this->username = username;
  this->authData = std::vector<uint8_t>(loginData.begin() + fields.offset,
                                      loginData.begin() + fields.offset + fields.size);''' + text[end:]
    text = replace_once(text, '''  cJSON_AddStringToObject(json_obj, "availability", this->username.c_str());''',
                        '''  cJSON_AddStringToObject(json_obj, "availability", "");''')
    text = replace_once(text, '''  cJSON_AddStringToObject(json_obj, "deviceType", "deviceType");''',
                        '''  cJSON_AddStringToObject(json_obj, "deviceType", "SPEAKER");''')
    (output / "LoginBlob.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/TrackQueue.cpp").read_text(encoding="utf-8")
    text = '#include "SpotifyResponse.h"\n' + text
    text = replace_once(text, '''    cJSON* jsonResult = cJSON_ParseWithLength(result.data(), result.size());
    cdnUrl = cJSON_GetArrayItem(cJSON_GetObjectItem(jsonResult, "cdnurl"), 0)
                 ->valuestring;
    cJSON_Delete(jsonResult);''', '''    cdnUrl = np2::spotifyResponse(SPOTIFY_HTTP_STORAGE, result);''')
    text = replace_once(text, 'CSPOT_LOG(info, "Received CDN URL, %s", cdnUrl.c_str());',
                        'CSPOT_LOG(info, "CDN location received");')
    (output / "TrackQueue.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/CDNAudioFile.cpp").read_text(encoding="utf-8")
    text = '#include <algorithm>\n' + text
    text = replace_once(text, 'this->footer = std::vector<uint8_t>(', 'this->footer = SpotifyAudioBytes(')
    text = replace_once(text, '  this->crypto = std::make_unique<Crypto>();', '''  if (audioKey.size() != 16) throw std::runtime_error("Spotify audio key invalid");
  this->crypto = std::make_unique<Crypto>();''')
    text = replace_once(text, '  this->position = newPos;',
                        '  this->position = std::min(newPos, this->totalFileSize);')
    text = replace_once(text, 'CSPOT_LOG(info, "Opening HTTP stream to %s", this->cdnUrl.c_str());',
                        'CSPOT_LOG(info, "Opening audio stream");')
    text = replace_once(text, '''  this->totalFileSize =
      this->httpConnection->totalLength() - SPOTIFY_OPUS_HEADER;''', '''  const size_t fullSize = this->httpConnection->totalLength();
  if (this->httpConnection->stream().gcount() != OPUS_HEADER_SIZE ||
      fullSize < static_cast<size_t>(SPOTIFY_OPUS_HEADER + OPUS_FOOTER_PREFFERED + 16))
    throw std::runtime_error("Spotify audio size invalid");
  this->totalFileSize = fullSize - SPOTIFY_OPUS_HEADER;''')
    text = replace_once(text, '''  this->decrypt(footer.data(), footer.size(), footerStartLocation);''', '''  if (this->httpConnection->totalLength() != fullSize ||
      this->httpConnection->contentLength() != footer.size() ||
      this->httpConnection->stream().gcount() != static_cast<std::streamsize>(footer.size()))
    throw std::runtime_error("Spotify audio footer invalid");
  this->decrypt(footer.data(), footer.size(), footerStartLocation);''')
    text = replace_once(text, '''  size_t offsetPosition = position + SPOTIFY_OPUS_HEADER;''', '''  if (!dst || !bytes || position >= totalFileSize) return 0;
  bytes = std::min(bytes, totalFileSize - position);
  size_t offsetPosition = position + SPOTIFY_OPUS_HEADER;''')
    text = replace_once(text, '''  if (position + bytes >= this->totalFileSize) {
    return 0;
  }''', '')
    text = replace_once(text, 'offsetPosition < OPUS_HEADER_SIZE &&',
                        'offsetPosition < static_cast<size_t>(OPUS_HEADER_SIZE) &&')
    text = replace_once(text, 'bytes + offsetPosition <= OPUS_HEADER_SIZE)',
                        'bytes + offsetPosition <= static_cast<size_t>(OPUS_HEADER_SIZE))')
    text = replace_once(text, 'requestPosition > SEEK_MARGIN_SIZE)',
                        'requestPosition > static_cast<size_t>(SEEK_MARGIN_SIZE))')
    text = replace_once(text, '''      readCapacity = this->httpConnection->contentLength();''', '''      readCapacity = this->httpConnection->contentLength();
      if (!readCapacity || readCapacity > this->httpBuffer.size() ||
          this->httpConnection->totalLength() != actualFileSize)
        throw std::runtime_error("Spotify audio range invalid");''')
    (output / "CDNAudioFile.cpp").write_text(text, encoding="utf-8")

    text = (source / "src/TrackPlayer.cpp").read_text(encoding="utf-8")
    text = '#include <limits>\n#include <stdexcept>\n' + text
    text = replace_once(text, '  vorbisFile = {};', '''  decoder = spotify_vorbis_create(SPOTIFY_VORBIS_BYTES_MAX);
  if (!decoder) throw std::bad_alloc();
  vorbisFile = {};''')
    text = replace_once(text, '''TrackPlayer::~TrackPlayer() {
  isRunning = false;
  resetState();
  std::scoped_lock lock(runningMutex);
}''', '''TrackPlayer::~TrackPlayer() {
  isRunning = false;
  resetState();
  std::scoped_lock lock(runningMutex);
  spotify_vorbis_close(decoder, &vorbisFile);
  spotify_vorbis_destroy(decoder);
}''')
    text = replace_once(text, '''          ov_open_callbacks(this, &vorbisFile, NULL, 0, vorbisCallbacks);

      if (pendingSeekPositionMs > 0) {''', '''          ov_open_callbacks(this, &vorbisFile, NULL, 0, vorbisCallbacks);
      if (r != 0) { currentSongPlaying = false; continue; }
      const vorbis_info *format = ov_info(&vorbisFile, -1);
      if (!format || format->rate != 44100 || format->channels != 2) {
        ov_clear(&vorbisFile);
        currentSongPlaying = false;
        continue;
      }
      if (pendingSeekPositionMs > 0) {''')
    text = replace_once(text, '''        if (ret == 0) {''', '''        const vorbis_info *sectionFormat = ov_info(&vorbisFile, -1);
        if (ret > static_cast<long>(pcmBuffer.size()) ||
            (ret > 0 && (ret % 4 || !sectionFormat || sectionFormat->rate != 44100 || sectionFormat->channels != 2))) {
          currentSongPlaying = false;
          break;
        }
        if (ret == 0) {''')
    text = replace_once(text, '              int written = 0;', '              size_t written = 0;')
    text = replace_once(text, '''              if (written == 0) {''', '''              if (written > static_cast<size_t>(toWrite) || written % 4) {
                currentSongPlaying = false; break;
              }
              if (written == 0) {''')
    start = text.index('size_t TrackPlayer::_vorbisRead(')
    end = text.index('void TrackPlayer::setDataCallback', start)
    text = text[:start] + '''size_t TrackPlayer::_vorbisRead(void* ptr, size_t size, size_t nmemb) {
  if (!currentTrackStream || !ptr || !size || nmemb > std::numeric_limits<size_t>::max() / size) return 0;
  try { return currentTrackStream->readBytes(static_cast<uint8_t*>(ptr), size * nmemb) / size; }
  catch (...) { currentSongPlaying = false; return 0; }
}
size_t TrackPlayer::_vorbisClose() { return 0; }
int TrackPlayer::_vorbisSeek(int64_t offset, int whence) {
  if (!currentTrackStream) return -1;
  try {
    int64_t base = 0;
    if (whence == 1) base = currentTrackStream->getPosition();
    else if (whence == 2) base = currentTrackStream->getSize();
    else if (whence != 0) return -1;
    const int64_t limit = currentTrackStream->getSize();
    if (offset < -base || offset > limit - base) return -1;
    currentTrackStream->seek(static_cast<size_t>(base + offset));
    return 0;
  } catch (...) { currentSongPlaying = false; return -1; }
}
long TrackPlayer::_vorbisTell() {
  if (!currentTrackStream) return -1;
  const auto pos = currentTrackStream->getPosition();
  return pos > static_cast<size_t>(std::numeric_limits<long>::max()) ? -1 : static_cast<long>(pos);
}

''' + text[end:]
    text = text.replace('ov_open_callbacks(this, &vorbisFile, NULL, 0, vorbisCallbacks)',
                        'spotify_vorbis_open(decoder, this, &vorbisFile, vorbisCallbacks)')
    text = text.replace('ov_clear(&vorbisFile)', 'spotify_vorbis_close(decoder, &vorbisFile)')
    text = text.replace('VORBIS_SEEK(&vorbisFile,', 'spotify_vorbis_seek(decoder, &vorbisFile,')
    text = text.replace('VORBIS_READ(&vorbisFile,', 'spotify_vorbis_read(decoder, &vorbisFile,')
    (output / "TrackPlayer.cpp").write_text(text, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.source, args.output)
