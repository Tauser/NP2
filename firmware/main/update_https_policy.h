#pragma once

#include <stdbool.h>

typedef struct {
    const char *allowed_host;
    const char *manifest_url;
    const char *signature_url;
    const char *image_url;
} update_https_endpoints_t;

typedef enum {
    UPDATE_HTTPS_POLICY_OK = 0,
    UPDATE_HTTPS_POLICY_INVALID_ARGUMENT,
    UPDATE_HTTPS_POLICY_MALFORMED_URL,
    UPDATE_HTTPS_POLICY_UNTRUSTED_HOST,
    UPDATE_HTTPS_POLICY_DUPLICATE_URL,
} update_https_policy_result_t;

/* Accepts only simple https://host/path release endpoints. */
update_https_policy_result_t update_https_endpoints_validate(
    const update_https_endpoints_t *endpoints);
