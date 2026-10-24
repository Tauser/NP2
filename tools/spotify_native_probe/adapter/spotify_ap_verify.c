#include "spotify_ap_verify.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha1.h"
/* Public trust anchor, key id 0, exponent 65537. Same public modulus used by
 * librespot core/src/connection/handshake.rs. This is public protocol data,
 * never a user/application secret. Verify before deriving keys or sending login.
 * Source: https://github.com/librespot-org/librespot/blob/dev/core/src/connection/handshake.rs */
static const char modulus[] =
    "ACE0460BFFC230AFF46BFEC3BFBF863DA191C6CC336C93A14FB3B01612ACAC6A"
    "F180E7F614D9429DBE2E346643E362D2327A1A0D923BAEDD1402B18155056104"
    "D52C96A44C1ECC024AD4B20C001F17EDC22FC43521C8F0CBAED2ADD72B0F9DB3"
    "C5321A2AFE59F35A0DAC68F1FA621EFB2C8D0CB7392D9247E3D7351A6DBD24C2"
    "AE255B88FFAB73298A0BCCCD0C58673189E8BD3480784A5FC96B899D956BFC86"
    "D74F33A6781796C9C32D0D32A5ABCD0527E2F710A39613C42F99C027BFED049C"
    "3C275804B6B219F9C12F02E94863ECA1B642A09D4825F8B39DD0E86AF9484DA1"
    "C2BA863042EA9DB3086C190E48B39D66EB0006A25AEEA11B13873CD719E655BD";
bool spotify_ap_verify(const uint8_t *packet, size_t size)
{
    spotify_ap_challenge_t challenge;
    if (!spotify_ap_parse(packet, size, &challenge)) return false;
    mbedtls_rsa_context rsa;
    mbedtls_mpi n, e;
    mbedtls_rsa_init(&rsa); mbedtls_mpi_init(&n); mbedtls_mpi_init(&e);
    uint8_t digest[20];
    const bool ok = mbedtls_mpi_read_string(&n, 16, modulus) == 0 &&
        mbedtls_mpi_lset(&e, 65537) == 0 &&
        mbedtls_rsa_import(&rsa, &n, NULL, NULL, NULL, &e) == 0 &&
        mbedtls_rsa_complete(&rsa) == 0 && mbedtls_rsa_check_pubkey(&rsa) == 0 &&
        mbedtls_sha1(challenge.key, 96, digest) == 0 &&
        mbedtls_rsa_pkcs1_verify(&rsa, MBEDTLS_MD_SHA1, sizeof(digest), digest, challenge.signature) == 0;
    mbedtls_rsa_free(&rsa); mbedtls_mpi_free(&n); mbedtls_mpi_free(&e);
    return ok;
}
