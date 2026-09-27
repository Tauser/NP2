#include <assert.h>
#include <stdio.h>
#include "update_https_policy.h"
int main(void){update_https_endpoints_t e={"updates.np2.local","https://updates.np2.local/a.manifest","https://updates.np2.local/a.sig","https://updates.np2.local/a.bin"};assert(update_https_endpoints_validate(&e)==UPDATE_HTTPS_POLICY_OK);e.image_url="http://updates.np2.local/a.bin";assert(update_https_endpoints_validate(&e)==UPDATE_HTTPS_POLICY_MALFORMED_URL);e.image_url="https://other.local/a.bin";assert(update_https_endpoints_validate(&e)==UPDATE_HTTPS_POLICY_UNTRUSTED_HOST);e.image_url=e.manifest_url;assert(update_https_endpoints_validate(&e)==UPDATE_HTTPS_POLICY_DUPLICATE_URL);puts("update_https_policy_host_test: all checks passed");return 0;}
