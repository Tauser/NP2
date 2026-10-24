#pragma once
#include <cstdint>
#include <vector>
/* Deliberate identity cipher: these tests exercise range/buffer ownership,
 * never Spotify decryption or the correctness of AES. */
class Crypto {
public:
    void aesCTRXcrypt(const std::vector<uint8_t>&,const std::vector<uint8_t>&,uint8_t*,size_t) {}
};
