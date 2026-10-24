#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../firmware/main/google_agenda_credentials.c"
static uint8_t slots[2][sizeof(credential_record_t)],pending[sizeof(credential_record_t)];
static bool present[2],fail_commit;
static unsigned pending_slot;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out)
{ assert(!strcmp(name,"np2_google")); *out=1; return mode == NVS_READONLY && !present[0] && !present[1] ? ESP_ERR_NVS_NOT_FOUND : ESP_OK; }
void nvs_close(nvs_handle_t handle) { (void)handle; }
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *out,size_t *size)
{ (void)handle; unsigned i=!strcmp(key,"auth1"); if (!present[i]) return ESP_ERR_NVS_NOT_FOUND; assert(*size >= sizeof(slots[i])); *size=sizeof(slots[i]); memcpy(out,slots[i],*size); return ESP_OK; }
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size)
{ (void)handle; pending_slot=!strcmp(key,"auth1"); assert(size == sizeof(pending)); memcpy(pending,data,size); return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t handle)
{ (void)handle; if (fail_commit) return ESP_FAIL; memcpy(slots[pending_slot],pending,sizeof(pending)); present[pending_slot]=true; return ESP_OK; }
esp_err_t flash_coordinator_request_google_credentials_write(const google_agenda_credentials_t *credentials)
{ return google_agenda_credentials_owner_write(credentials); }
int main(void)
{
    assert(google_agenda_credentials_owner_load() == ESP_ERR_NOT_FOUND && !google_agenda_linked());
    const char *first="{\"client_id\":\"fixture.apps.googleusercontent.com\",\"client_secret\":\"fixture-secret\",\"refresh_token\":\"fixture-refresh\",\"subject\":\"12345\"}";
    assert(google_agenda_provision_json(first,strlen(first)) == ESP_OK && google_agenda_linked());
    google_agenda_credentials_t copy;
    assert(google_agenda_credentials_copy_private(&copy) == ESP_OK && !strcmp(copy.subject,"12345"));
    const char *other="{\"client_id\":\"fixture.apps.googleusercontent.com\",\"client_secret\":\"fixture-secret\",\"refresh_token\":\"fixture-other\",\"subject\":\"99999\"}";
    assert(google_agenda_provision_json(other,strlen(other)) == ESP_ERR_INVALID_STATE);
    const char *nested="{\"a\":[[[[[]]]]]}";
    assert(google_agenda_provision_json(nested,strlen(nested)) == ESP_ERR_INVALID_ARG);
    const char *duplicate="{\"client_id\":\"fixture.apps.googleusercontent.com\",\"client_secret\":\"fixture-secret\",\"refresh_token\":\"fixture-other\",\"refresh_token\":\"duplicate\"}";
    assert(google_agenda_provision_json(duplicate,strlen(duplicate)) == ESP_ERR_INVALID_ARG);
    strcpy(copy.refresh_token,"renewed-fixture"); fail_commit=true;
    assert(google_agenda_credentials_owner_write(&copy) == ESP_FAIL); fail_commit=false;
    assert(google_agenda_credentials_owner_load() == ESP_OK);
    assert(google_agenda_credentials_copy_private(&copy) == ESP_OK && !strcmp(copy.refresh_token,"fixture-refresh"));
    strcpy(copy.refresh_token,"renewed-fixture"); assert(google_agenda_credentials_owner_write(&copy) == ESP_OK);
    assert(google_agenda_credentials_copy_private(&copy) == ESP_OK && !strcmp(copy.refresh_token,"renewed-fixture"));
    slots[0][30]^=1; memset(&s_credentials,0,sizeof(s_credentials)); s_linked=false;
    assert(google_agenda_credentials_owner_load() == ESP_OK);
    assert(google_agenda_credentials_copy_private(&copy) == ESP_OK && !strcmp(copy.refresh_token,"fixture-refresh"));
    puts("Google vault: provision, reauthorize same account, reject account change, malformed input, commit failure, CRC fallback PASS");
    return 0;
}
