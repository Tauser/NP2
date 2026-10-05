#include "np_ewelink_auth.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "mbedtls/base64.h"
#include "mbedtls/md.h"

#define REGION_REPR_MAX_BYTES 8192U
#define REGION_B64_MAX_BYTES 11000U
#define DERIVED_KEY_MAX_BYTES 64U

typedef struct {
    const char *country_code;
    const char *country_name;
    const char *region;
} region_entry_t;

static const region_entry_t s_regions[] = {
#include "ewelink_regions.inc"
};

static const char NP_EWELINK_APP_ID[] = "R8Oq3y0eSZSYdKccHlrQzT1ACCOUT9Gv";
static const char NP_EWELINK_KEY_INDEX_B64[] =
    "L8KDAMO6wpomxpYZwrHhu4AuEjQKBy8nwoMHNB7DmwoWwrvCsSYGw4wDAxs=";

static char *s_region_repr;
static unsigned char *s_region_b64;
static unsigned char s_key_indices[128];
static unsigned char s_derived_key[DERIVED_KEY_MAX_BYTES];

static void secure_zero(void *buffer, size_t length)
{
    volatile unsigned char *bytes = buffer;
    while (length-- > 0U) *bytes++ = 0U;
}

const char *np_ewelink_auth_region_for_country(const char *country_code)
{
    if (country_code == NULL) return NULL;
    for (size_t index = 0U; index < sizeof(s_regions) / sizeof(s_regions[0]); ++index) {
        if (strcmp(country_code, s_regions[index].country_code) == 0) {
            return s_regions[index].region;
        }
    }
    return NULL;
}

const char *np_ewelink_auth_app_id(void)
{
    return NP_EWELINK_APP_ID;
}

const char *np_ewelink_auth_api_host(const char *region)
{
    if (region == NULL) return NULL;
    if (strcmp(region, "cn") == 0) return "https://cn-apia.coolkit.cn";
    if (strcmp(region, "as") == 0) return "https://as-apia.coolkit.cc";
    if (strcmp(region, "us") == 0) return "https://us-apia.coolkit.cc";
    if (strcmp(region, "eu") == 0) return "https://eu-apia.coolkit.cc";
    return NULL;
}

static esp_err_t make_python_regions_repr(char *buffer, size_t buffer_size,
                                           size_t *out_length)
{
    if (buffer == NULL || buffer_size == 0U || out_length == NULL)
        return ESP_ERR_INVALID_ARG;
    size_t used = 0U;
    int written = snprintf(buffer, buffer_size, "{");
    if (written < 0 || (size_t)written >= buffer_size) return ESP_ERR_INVALID_SIZE;
    used = (size_t)written;
    for (size_t index = 0U; index < sizeof(s_regions) / sizeof(s_regions[0]); ++index) {
        const region_entry_t *const entry = &s_regions[index];
        written = snprintf(buffer + used, buffer_size - used,
                           "%s'%s': ('%s', '%s')",
                           index == 0U ? "" : ", ", entry->country_code,
                           entry->country_name, entry->region);
        if (written < 0 || (size_t)written >= buffer_size - used) {
            secure_zero(buffer, buffer_size);
            return ESP_ERR_INVALID_SIZE;
        }
        used += (size_t)written;
    }
    if (used + 2U > buffer_size) {
        secure_zero(buffer, buffer_size);
        return ESP_ERR_INVALID_SIZE;
    }
    buffer[used++] = '}';
    buffer[used] = '\0';
    *out_length = used;
    return ESP_OK;
}

static bool next_utf8_codepoint(const unsigned char *input, size_t length,
                                size_t *offset, uint32_t *out_codepoint)
{
    if (input == NULL || offset == NULL || out_codepoint == NULL || *offset >= length)
        return false;
    const unsigned char first = input[(*offset)++];
    if (first < 0x80U) {
        *out_codepoint = first;
        return true;
    }
    unsigned int continuation_count;
    uint32_t codepoint;
    if ((first & 0xe0U) == 0xc0U) { continuation_count = 1U; codepoint = first & 0x1fU; }
    else if ((first & 0xf0U) == 0xe0U) { continuation_count = 2U; codepoint = first & 0x0fU; }
    else if ((first & 0xf8U) == 0xf0U) { continuation_count = 3U; codepoint = first & 0x07U; }
    else return false;
    if (continuation_count > length - *offset) return false;
    for (unsigned int index = 0U; index < continuation_count; ++index) {
        const unsigned char next = input[(*offset)++];
        if ((next & 0xc0U) != 0x80U) return false;
        codepoint = (codepoint << 6U) | (next & 0x3fU);
    }
    if ((continuation_count == 1U && codepoint < 0x80U) ||
        (continuation_count == 2U && codepoint < 0x800U) ||
        (continuation_count == 3U && codepoint < 0x10000U) ||
        codepoint > 0x10ffffU || (codepoint >= 0xd800U && codepoint <= 0xdfffU)) return false;
    *out_codepoint = codepoint;
    return true;
}

static esp_err_t derive_signing_key(size_t *out_key_length)
{
    if (out_key_length == NULL) return ESP_ERR_INVALID_ARG;
    s_region_repr = heap_caps_calloc(1U, REGION_REPR_MAX_BYTES,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_region_b64 = heap_caps_calloc(1U, REGION_B64_MAX_BYTES,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_region_repr == NULL || s_region_b64 == NULL) {
        if (s_region_repr != NULL) heap_caps_free(s_region_repr);
        if (s_region_b64 != NULL) heap_caps_free(s_region_b64);
        s_region_repr = NULL;
        s_region_b64 = NULL;
        return ESP_ERR_NO_MEM;
    }
    size_t repr_length = 0U;
    esp_err_t result = make_python_regions_repr(s_region_repr, REGION_REPR_MAX_BYTES,
                                                &repr_length);
    if (result != ESP_OK) goto cleanup;

    size_t encoded_length = 0U;
    result = mbedtls_base64_encode(s_region_b64, REGION_B64_MAX_BYTES,
                                   &encoded_length,
                                   (const unsigned char *)s_region_repr, repr_length);
    secure_zero(s_region_repr, REGION_REPR_MAX_BYTES);
    if (result != 0) { result = ESP_FAIL; goto cleanup; }

    size_t index_bytes = 0U;
    result = mbedtls_base64_decode(s_key_indices, sizeof(s_key_indices),
                                   &index_bytes,
                                   (const unsigned char *)NP_EWELINK_KEY_INDEX_B64,
                                   sizeof(NP_EWELINK_KEY_INDEX_B64) - 1U);
    if (result != 0 || index_bytes == 0U) { result = ESP_FAIL; goto cleanup; }

    size_t offset = 0U;
    size_t key_length = 0U;
    while (offset < index_bytes) {
        uint32_t codepoint = 0U;
        if (!next_utf8_codepoint(s_key_indices, index_bytes, &offset, &codepoint) ||
            codepoint >= encoded_length || key_length >= sizeof(s_derived_key)) {
            secure_zero(s_region_b64, REGION_B64_MAX_BYTES);
            secure_zero(s_key_indices, sizeof(s_key_indices));
            result = ESP_ERR_INVALID_CRC;
            goto cleanup;
        }
        s_derived_key[key_length++] = s_region_b64[codepoint];
    }
    secure_zero(s_region_b64, REGION_B64_MAX_BYTES);
    secure_zero(s_key_indices, sizeof(s_key_indices));
    *out_key_length = key_length;
    result = ESP_OK;
cleanup:
    if (s_region_repr != NULL) {
        secure_zero(s_region_repr, REGION_REPR_MAX_BYTES);
        heap_caps_free(s_region_repr);
        s_region_repr = NULL;
    }
    if (s_region_b64 != NULL) {
        secure_zero(s_region_b64, REGION_B64_MAX_BYTES);
        heap_caps_free(s_region_b64);
        s_region_b64 = NULL;
    }
    if (result != ESP_OK) {
        secure_zero(s_key_indices, sizeof(s_key_indices));
        secure_zero(s_derived_key, sizeof(s_derived_key));
    }
    return result;
}

esp_err_t np_ewelink_auth_sign_json(const uint8_t *json, size_t json_size,
                                    char *out_signature, size_t out_size)
{
    if (json == NULL || json_size == 0U || out_signature == NULL || out_size == 0U)
        return ESP_ERR_INVALID_ARG;
    size_t key_length = 0U;
    esp_err_t result = derive_signing_key(&key_length);
    if (result != ESP_OK) return result;

    const mbedtls_md_info_t *const md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    unsigned char digest[32] = {0};
    if (md == NULL || mbedtls_md_hmac(md, s_derived_key, key_length,
                                     json, json_size, digest) != 0) {
        secure_zero(s_derived_key, sizeof(s_derived_key));
        return ESP_FAIL;
    }
    size_t encoded_length = 0U;
    result = mbedtls_base64_encode((unsigned char *)out_signature, out_size,
                                   &encoded_length, digest, sizeof(digest));
    secure_zero(s_derived_key, sizeof(s_derived_key));
    secure_zero(digest, sizeof(digest));
    if (result != 0 || encoded_length + 1U > out_size) {
        secure_zero(out_signature, out_size);
        return ESP_ERR_INVALID_SIZE;
    }
    out_signature[encoded_length] = '\0';
    return ESP_OK;
}
