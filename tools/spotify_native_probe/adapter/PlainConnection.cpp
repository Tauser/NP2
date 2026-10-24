#include "PlainConnection.h"
#include "spotify_packet_policy.h"
#include "Utils.h"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/select.h>
#include <sys/socket.h>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
using namespace cspot;
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto transferTimeout = std::chrono::seconds(15);
[[noreturn]] void fail(unsigned operation,int code=0) {
#ifdef ESP_PLATFORM
    ESP_LOGI("np2_spotify","socket operation=%u code=%d",operation,code);
#else
    (void)operation;(void)code;
#endif
    if(operation==5) throw std::runtime_error("Spotify packet invalid");
    if(operation==2) throw std::runtime_error("Spotify response invalid");
    throw std::runtime_error("Spotify socket failed");
}
bool elapsed(Clock::time_point deadline) { return Clock::now() >= deadline; }
void waitSocket(int fd, bool write, Clock::time_point deadline) {
    while (!elapsed(deadline)) {
        fd_set descriptors; FD_ZERO(&descriptors); FD_SET(fd, &descriptors);
        timeval timeout{0, 200000};
        int result = select(fd + 1, write ? nullptr : &descriptors,
                            write ? &descriptors : nullptr, nullptr, &timeout);
        if (result > 0) return;
        if (result < 0 && errno != EINTR) fail(1,errno);
    }
    fail(1,ETIMEDOUT);
}
bool endpoint(const std::string& address, std::string& host, std::string& port) {
    const auto split = address.find(':');
    if (split == std::string::npos || address.size() > 260) return false;
    host = address.substr(0, split); port = address.substr(split + 1);
    constexpr const char suffix[] = ".spotify.com";
    if (host.size() <= sizeof(suffix) - 1 ||
        host.compare(host.size() - sizeof(suffix) + 1, sizeof(suffix) - 1, suffix)) return false;
    for (char c : host) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.')) return false;
    return port == "4070" || port == "443" || port == "80";
}
}
PlainConnection::PlainConnection() : apSock(-1) {}
PlainConnection::~PlainConnection() { close(); }
void PlainConnection::connect(const std::string& address) {
    close();
    std::string host, port;
    if (!endpoint(address, host, port)) fail(2);
    addrinfo hints{}, *root = nullptr;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP;
    /* lwIP resolver has its configured finite retry limit. Socket deadline
     * begins before DNS, so resolution cannot grant a fresh connect budget. */
    const auto deadline = Clock::now() + transferTimeout;
    const int dns=getaddrinfo(host.c_str(), port.c_str(), &hints, &root);
    if (dns || !root) fail(3,dns);
    int last_error=ETIMEDOUT;
    for (auto ai = root; ai && !elapsed(deadline); ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {last_error=errno;continue;}
        if (fd>=FD_SETSIZE) {last_error=EMFILE;::close(fd);continue;}
        const int flags = fcntl(fd, F_GETFL, 0);
        bool success = flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
        if (!success) last_error=errno;
        if (success) {
            const int result = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
            success = result == 0;
            if (!success) last_error=errno;
            if (!success && errno == EINPROGRESS) {
                try {
                    waitSocket(fd, true, deadline);
                    int error = 0; socklen_t size = sizeof(error);
                    const int checked=getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size);
                    success = checked == 0 && error == 0;
                    if(!success) last_error=checked ? errno : error;
                } catch (...) { success = false;last_error=ETIMEDOUT; }
            }
        }
        if (success) {
            int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            apSock = fd; break;
        }
        ::close(fd);
    }
    freeaddrinfo(root);
    if (apSock < 0) fail(4,last_error);
}
std::vector<uint8_t> PlainConnection::recvPacket() {
    std::vector<uint8_t> result(4);
    readBlock(result.data(), result.size());
    uint32_t total = (uint32_t)result[0] << 24U | (uint32_t)result[1] << 16U |
                     (uint32_t)result[2] << 8U | result[3];
    if (!spotify_packet_size(total)) fail(5);
    result.resize(total); readBlock(result.data() + 4, total - 4); return result;
}
std::vector<uint8_t> PlainConnection::sendPrefixPacket(const std::vector<uint8_t>& prefix,
    const std::vector<uint8_t>& data) {
    if (prefix.size() > 2 || data.size() > SPOTIFY_PACKET_MAX - 6U) fail(5);
    auto result = pack<uint32_t>(htonl(static_cast<uint32_t>(prefix.size() + data.size() + 4)));
    result.insert(result.begin(), prefix.begin(), prefix.end());
    result.insert(result.end(), data.begin(), data.end()); writeBlock(result); return result;
}
void PlainConnection::readBlock(const uint8_t *dst, size_t size) {
    if ((!dst && size) || size > SPOTIFY_PACKET_MAX || apSock < 0) fail(6);
    auto deadline = Clock::now() + (size == 3 ? std::chrono::seconds(130) : transferTimeout);
    size_t offset = 0;
    while (offset < size) {
        if (elapsed(deadline)) fail(6,ETIMEDOUT);
        const int n = recv(apSock, const_cast<uint8_t*>(dst) + offset, size - offset, 0);
        if (n > 0) {
            if (!offset) deadline = Clock::now() + transferTimeout;
            offset += static_cast<size_t>(n); continue;
        }
        if (!n) fail(7); /* EOF never retries with stale errno. */
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) fail(6,errno);
        waitSocket(apSock, false, deadline);
    }
}
size_t PlainConnection::writeBlock(const std::vector<uint8_t>& data) {
    if (data.size() > SPOTIFY_PACKET_MAX + 3U || apSock < 0) fail(8);
    const auto deadline = Clock::now() + transferTimeout;
    size_t offset = 0;
    while (offset < data.size()) {
        if (elapsed(deadline)) fail(8,ETIMEDOUT);
        const int n = send(apSock, data.data() + offset, data.size() - offset, 0);
        if (n > 0) { offset += static_cast<size_t>(n); continue; }
        if (!n) fail(8);
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) fail(8,errno);
        waitSocket(apSock, true, deadline);
    }
    return data.size();
}
void PlainConnection::close() {
    if (apSock < 0) return;
    shutdown(apSock, SHUT_RDWR); ::close(apSock); apSock = -1;
}
