#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../firmware/main/spotify_remote_credentials.c"
#include "../firmware/main/home_widget_preferences.c"
static uint8_t slots[2][sizeof(credential_record_t)], pending[sizeof(credential_record_t)];
static bool present[2], fail_commit, busy;
static unsigned pending_slot;
static flash_coordinator_status_t status;
static uint8_t home_slots[2][sizeof(record_t)], home_pending[sizeof(record_t)];
static bool home_present[2];
static unsigned home_pending_slot;
bool spotify_remote_provision_begin(void) { return !busy; }
void spotify_remote_provision_end(void) {}
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) { assert(caps & MALLOC_CAP_INTERNAL); return calloc(n,size); }
void heap_caps_free(void *p) { free(p); }
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out)
{ if(!strcmp(name,"np2_home")){*out=2;return mode==NVS_READONLY && !home_present[0] && !home_present[1]?ESP_ERR_NVS_NOT_FOUND:ESP_OK;} assert(!strcmp(name,"np2_spotify")); *out=1; return mode == NVS_READONLY && !present[0] && !present[1] ? ESP_ERR_NVS_NOT_FOUND : ESP_OK; }
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size)
{ if(h==2){unsigned i=!strcmp(key,"view1");if(!home_present[i])return ESP_ERR_NVS_NOT_FOUND;assert(*size>=sizeof(home_slots[i]));*size=sizeof(home_slots[i]);memcpy(out,home_slots[i],*size);return ESP_OK;} unsigned i=!strcmp(key,"auth1"); if (!present[i]) return ESP_ERR_NVS_NOT_FOUND; assert(*size >= sizeof(slots[i])); *size=sizeof(slots[i]); memcpy(out,slots[i],*size); return ESP_OK; }
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size)
{ if(h==2){home_pending_slot=!strcmp(key,"view1");assert(size==sizeof(home_pending));memcpy(home_pending,data,size);return ESP_OK;} pending_slot=!strcmp(key,"auth1"); assert(size==sizeof(pending)); memcpy(pending,data,size); return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t h)
{ if(fail_commit) return ESP_FAIL; if(h==2){memcpy(home_slots[home_pending_slot],home_pending,sizeof(home_pending));home_present[home_pending_slot]=true;return ESP_OK;} memcpy(slots[pending_slot],pending,sizeof(pending)); present[pending_slot]=true; return ESP_OK; }
void flash_coordinator_get_status(flash_coordinator_status_t *out) { *out=status; }
esp_err_t flash_coordinator_request_spotify_credentials_write(const spotify_remote_credentials_t *c)
{ if(busy) return ESP_ERR_INVALID_STATE; esp_err_t r=spotify_remote_credentials_owner_write(c); status.spotify_auth_result=r; ++status.spotify_auth_completion; return r; }
int main(void)
{
    assert(spotify_remote_credentials_owner_load()==ESP_ERR_NOT_FOUND);
    const char *json="{\"client_id\":\"0123456789abcdef0123456789abcdef\",\"refresh_token\":\"fixture-refresh\"}";
    assert(spotify_remote_provision_json(json,strlen(json))==ESP_OK && spotify_remote_linked());
    spotify_remote_credentials_t c;
    assert(spotify_remote_credentials_copy_private(&c)==ESP_OK && !strcmp(c.refresh_token,"fixture-refresh"));
    uint32_t first_version=spotify_remote_credentials_version();
    const char *duplicate="{\"client_id\":\"0123456789abcdef0123456789abcdef\",\"refresh_token\":\"a\",\"refresh_token\":\"b\"}";
    assert(spotify_remote_provision_json(duplicate,strlen(duplicate))==ESP_ERR_INVALID_ARG);
    const char *unknown="{\"client_id\":\"0123456789abcdef0123456789abcdef\",\"refresh_token\":\"a\",\"unknown\":1}";
    assert(spotify_remote_provision_json(unknown,strlen(unknown))==ESP_ERR_INVALID_ARG);
    strcpy(c.refresh_token,"rotated-fixture"); busy=true;
    assert(spotify_remote_credentials_rotate_private(&c)==ESP_OK);
    spotify_remote_credentials_tick(); assert(s_rotation_dirty && !s_rotation_inflight);
    busy=false; spotify_remote_credentials_tick(); spotify_remote_credentials_tick();
    assert(!s_rotation_dirty && spotify_remote_credentials_version()>first_version);
    strcpy(c.refresh_token,"failed-fixture"); fail_commit=true;
    assert(spotify_remote_credentials_owner_write(&c)==ESP_FAIL); fail_commit=false;
    assert(spotify_remote_credentials_copy_private(&c)==ESP_OK && !strcmp(c.refresh_token,"rotated-fixture"));
    slots[0][30]^=1; memset(&s_credentials,0,sizeof(s_credentials)); s_linked=false;
    assert(spotify_remote_credentials_owner_load()==ESP_OK);
    assert(spotify_remote_credentials_copy_private(&c)==ESP_OK && !strcmp(c.refresh_token,"fixture-refresh"));
    uint8_t widget=99;
    assert(home_widget_owner_load(&widget)==ESP_ERR_NOT_FOUND);
    assert(home_widget_owner_write(1)==ESP_OK && home_widget_owner_load(&widget)==ESP_OK && widget==1);
    assert(home_widget_owner_write(0)==ESP_OK && home_widget_owner_load(&widget)==ESP_OK && widget==0);
    fail_commit=true; assert(home_widget_owner_write(1)==ESP_FAIL); fail_commit=false;
    assert(home_widget_owner_load(&widget)==ESP_OK && widget==0);
    home_slots[0][sizeof(record_t)-1]^=1;
    assert(home_widget_owner_load(&widget)==ESP_OK && widget==1);
    assert(home_widget_owner_write(3)==ESP_ERR_INVALID_ARG);
    puts("Home widget: save/restore, commit failure and CRC fallback PASS");
    puts("Spotify vault: strict private provision, duplicate/unknown rejection, coordinated rotation retry, failed commit and CRC fallback PASS");
}
