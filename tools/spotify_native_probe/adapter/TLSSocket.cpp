/* Direct TLS from foreign tasks is deliberately unavailable. All HTTPS must
 * use the bounded HTTP exchange backed by NP2's single network executor. */
#include "TLSSocket.h"
#include <stdexcept>
bell::TLSSocket::TLSSocket() { throw std::runtime_error("Direct Spotify TLS disabled"); }
void bell::TLSSocket::open(const std::string&,uint16_t) { throw std::runtime_error("Direct Spotify TLS disabled"); }
size_t bell::TLSSocket::read(uint8_t*,size_t) { return 0; }
size_t bell::TLSSocket::write(uint8_t*,size_t) { return 0; }
size_t bell::TLSSocket::poll() { return 0; }
bool bell::TLSSocket::isOpen() { return false; }
void bell::TLSSocket::close() {}
