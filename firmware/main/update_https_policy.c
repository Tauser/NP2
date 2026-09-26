#include "update_https_policy.h"

#include <stddef.h>
#include <string.h>

#define UPDATE_HTTPS_HOST_MAX_BYTES 96U

static bool parse_host(const char *url, const char **out_host, size_t *out_host_bytes)
{
    static const char prefix[] = "https://";
    if (url == NULL || out_host == NULL || out_host_bytes == NULL ||
        strncmp(url, prefix, sizeof(prefix) - 1U) != 0) {
        return false;
    }
    const char *const host = url + sizeof(prefix) - 1U;
    const char *const path = strchr(host, '/');
    if (path == NULL || path == host || path[1] == '\0' || strchr(host, '@') != NULL ||
        strchr(url, '?') != NULL || strchr(url, '#') != NULL) {
        return false;
    }
    const size_t host_bytes = (size_t)(path - host);
    if (host_bytes == 0U || host_bytes > UPDATE_HTTPS_HOST_MAX_BYTES ||
        memchr(host, ':', host_bytes) != NULL) {
        return false;
    }
    *out_host = host;
    *out_host_bytes = host_bytes;
    return true;
}

update_https_policy_result_t update_https_endpoints_validate(
    const update_https_endpoints_t *endpoints)
{
    if (endpoints == NULL || endpoints->allowed_host == NULL ||
        endpoints->allowed_host[0] == '\0') {
        return UPDATE_HTTPS_POLICY_INVALID_ARGUMENT;
    }
    const char *urls[] = {endpoints->manifest_url, endpoints->signature_url, endpoints->image_url};
    for (size_t index = 0U; index < sizeof(urls) / sizeof(urls[0]); ++index) {
        const char *host = NULL;
        size_t host_bytes = 0U;
        if (!parse_host(urls[index], &host, &host_bytes)) return UPDATE_HTTPS_POLICY_MALFORMED_URL;
        if (strlen(endpoints->allowed_host) != host_bytes ||
            memcmp(endpoints->allowed_host, host, host_bytes) != 0) {
            return UPDATE_HTTPS_POLICY_UNTRUSTED_HOST;
        }
        for (size_t prior = 0U; prior < index; ++prior) {
            if (strcmp(urls[index], urls[prior]) == 0) return UPDATE_HTTPS_POLICY_DUPLICATE_URL;
        }
    }
    return UPDATE_HTTPS_POLICY_OK;
}
