#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/main/spotify_remote.c"
static int64_t clock_us = 1000000;
static size_t free_bytes = 400000;
static unsigned allocations;
static int64_t header_length=-1;
static size_t response_sizes[8];
static unsigned active_http, request_count, queued_count, response_index, response_count;
static bool truncated, oversize;
static uint32_t vault_version = 1;
static const char *responses[8];
static int statuses[8];
static char last_url[512];
static bool authorization_sent;
static struct { esp_http_client_config_t config; const char *body; int status; size_t at,total; } client;
static spotify_remote_credentials_t vault = {.client_id="0123456789abcdef0123456789abcdef", .refresh_token="fixture-refresh"};
bool spotify_remote_linked(void) { return true; }
void spotify_remote_credentials_tick(void) {}
bool spotify_remote_credentials_settled(void) { return true; }
uint32_t spotify_remote_credentials_version(void) { return vault_version; }
esp_err_t spotify_remote_credentials_copy_private(spotify_remote_credentials_t *out) { *out=vault; return ESP_OK; }
esp_err_t spotify_remote_credentials_rotate_private(const spotify_remote_credentials_t *in) { vault=*in; ++vault_version; return ESP_OK; }
esp_err_t network_validation_service_request_spotify_remote(void) { ++queued_count; return ESP_OK; }
esp_err_t network_validation_service_request_spotify_interactive(void) { ++queued_count; return ESP_OK; }
int64_t esp_timer_get_time(void) { return clock_us; }
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) { assert(caps & MALLOC_CAP_INTERNAL); ++allocations; return calloc(n,size); }
void heap_caps_free(void *p) { if(p) { assert(allocations>0); --allocations; } free(p); }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return free_bytes; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return free_bytes; }
esp_err_t esp_crt_bundle_attach(void *p) { (void)p; return ESP_OK; }
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    assert(active_http++ == 0 && response_index < response_count);
    assert(config->crt_bundle_attach && config->disable_auto_redirect && config->max_redirection_count == 0);
    client.config=*config; client.at=0; client.body=responses[response_index]; client.total=response_sizes[response_index]; client.status=statuses[response_index++];
    snprintf(last_url,sizeof(last_url),"%s",config->url);
    authorization_sent=false;
    ++request_count; return &client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *k,const char *v)
{ (void)c; assert(k && v && !strchr(v,'\n')); if(!strcmp(k,"Authorization")) {assert(!spotify_cover_url_allowed(last_url));authorization_sent=true;} return ESP_OK; }
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t c,int ms)
{ (void)c; assert(ms > 0 && ms <= 8000); return ESP_OK; }
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n) { (void)c; (void)n; return ESP_OK; }
int esp_http_client_write(esp_http_client_handle_t c,const char *body,int n) { (void)c; assert(body); return n > 7 ? 7 : n; }
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c)
{
    (void)c;
    if (client.status == 429) { esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_HEADER,.header_key="Retry-After",.header_value="120"}; client.config.event_handler(&e); }
    return oversize ? BODY_CAP+1 : header_length>=0 ? header_length : (int64_t)client.total;
}
int esp_http_client_get_status_code(esp_http_client_handle_t c) { (void)c; return client.status; }
int esp_http_client_read(esp_http_client_handle_t c,char *out,int cap)
{
    (void)c; size_t n=client.total-client.at;
    if (n > 13) n=13;
    if (n > (size_t)cap) n=(size_t)cap;
    memcpy(out,client.body+client.at,n); client.at+=n; return (int)n;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) { (void)c; return !truncated; }
esp_err_t esp_http_client_close(esp_http_client_handle_t c) { (void)c; return ESP_OK; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) { (void)c; assert(active_http-- == 1); return ESP_OK; }
static void schedule(const char *body,int status)
{ responses[response_count]=body; response_sizes[response_count]=strlen(body); statuses[response_count++]=status; }
static void reset_responses(void) { response_index=response_count=0; }
static void complete(spotify_remote_projection_t *p)
{
    esp_err_t result=spotify_remote_process(ESP_OK);
    assert(result == s_mailbox.result);
    clock_us+=1000000;
    spotify_remote_tick(p,true,true,(uint64_t)clock_us/1000);
}
int main(int argc,char **argv)
{
    assert(argc==3); unsigned char jpeg[8192], hq[8192];
    FILE *file=fopen(argv[1],"rb"); assert(file);
    size_t jpeg_size=fread(jpeg,1,sizeof(jpeg),file); assert(feof(file)); fclose(file);
    file=fopen(argv[2],"rb"); assert(file);
    size_t hq_size=fread(hq,1,sizeof(hq),file); assert(feof(file)); fclose(file);
    spotify_remote_projection_t p={.linked=true};
    spotify_remote_command_t job={.action=SPOTIFY_REMOTE_REFRESH};
    schedule("{\"access_token\":\"fixture-access\",\"token_type\":\"Bearer\",\"expires_in\":3600}",200);
    schedule("{\"device\":{\"id\":\"fixture-device\",\"name\":\"Desktop\",\"is_restricted\":false,\"supports_volume\":true,\"volume_percent\":40},\"progress_ms\":12000,\"is_playing\":true,\"item\":{\"name\":\"Faixa \\u00e7\",\"duration_ms\":180000,\"artists\":[{\"name\":\"Artista\"}]}}",200);
    spotify_remote_command(&p,&job); assert(p.busy); complete(&p);
    assert(p.available && p.playing && p.volume==40 && p.progress_ms==12000 && !strcmp(p.title,"Faixa ç"));
    assert(request_count==2 && active_http==0 && !p.busy);
    reset_responses(); schedule("",204); job.action=SPOTIFY_REMOTE_PAUSE;
    spotify_remote_command(&p,&job); complete(&p);
    assert(strstr(last_url,"device_id=fixture-device") != NULL);
    assert(request_count==3); /* cached access, no extra token request */
    reset_responses(); schedule("{\"devices\":[{\"id\":\"speaker\",\"name\":\"Sala\",\"is_restricted\":false,\"supports_volume\":false},{\"id\":null,\"name\":\"Inválido\"}]}",200);
    job.action=SPOTIFY_REMOTE_DEVICES_REQUEST; spotify_remote_command(&p,&job); complete(&p);
    assert(p.device_count==1 && !strcmp(p.devices[0].name,"Sala"));
    job=(spotify_remote_command_t){.action=SPOTIFY_REMOTE_TRANSFER,.revision=p.revision-1};
    spotify_remote_command(&p,&job); assert(!p.busy && p.result==ESP_ERR_INVALID_ARG);
    reset_responses(); schedule("",204); job.revision=p.revision;
    spotify_remote_command(&p,&job); complete(&p);
    assert(!p.available && !p.device_id[0] && !strcmp(p.device_name,"Sala"));
    reset_responses(); schedule("{}",429); job.action=SPOTIFY_REMOTE_REFRESH;
    spotify_remote_command(&p,&job); complete(&p);
    assert(p.result==SPOTIFY_REMOTE_RATE_LIMIT && s_not_before >= (uint64_t)clock_us/1000+120000);
    unsigned queued=queued_count; spotify_remote_command(&p,&job); assert(queued_count==queued);
    clock_us+=121000000; reset_responses(); schedule("{}",401);
    spotify_remote_command(&p,&job); complete(&p); assert(p.auth_required);
    clock_us+=40000000; queued=queued_count; job.action=SPOTIFY_REMOTE_VISIBLE; spotify_remote_command(&p,&job);
    spotify_remote_tick(&p,true,true,(uint64_t)clock_us/1000); assert(queued_count==queued);
    ++vault_version; spotify_remote_tick(&p,true,true,(uint64_t)clock_us/1000); assert(p.busy && !p.auth_required);
    free_bytes=100000; assert(spotify_remote_process(ESP_OK)==ESP_ERR_NO_MEM); free_bytes=400000;
    spotify_remote_tick(&p,false,true,(uint64_t)clock_us/1000); assert(!p.busy);
    clock_us+=40000000; job.action=SPOTIFY_REMOTE_HIDDEN; spotify_remote_command(&p,&job);
    reset_responses(); schedule("{\"access_token\":\"fixture-access\",\"token_type\":\"Bearer\",\"expires_in\":3600}",200); schedule("",204);
    job.action=SPOTIFY_REMOTE_REFRESH; spotify_remote_command(&p,&job); complete(&p); assert(!p.available && !p.playing);
    clock_us+=1000000; reset_responses(); schedule("{}",200); truncated=true;
    spotify_remote_command(&p,&job); complete(&p); assert(p.result==ESP_ERR_INVALID_RESPONSE); truncated=false;
    clock_us+=40000000; reset_responses(); schedule("{}",200); oversize=true;
    spotify_remote_command(&p,&job); complete(&p); assert(p.result==ESP_ERR_INVALID_SIZE); oversize=false;
    assert(active_http==0);
    assert(spotify_cover_url_allowed("https://i.scdn.co/image/0123456789abcdef0123456789abcdef01234567"));
    assert(!spotify_cover_url_allowed("http://i.scdn.co/image/0123456789abcdef0123456789abcdef01234567"));
    assert(!spotify_cover_url_allowed("https://i.scdn.co.evil/image/0123456789abcdef0123456789abcdef01234567"));
    assert(!spotify_cover_url_allowed("https://i.scdn.co/image/0123456789abcdef0123456789abcdef01234567?token=x"));
    clock_us+=40000000; reset_responses();
    schedule("{\"device\":{\"id\":\"fixture\",\"name\":\"Desktop\"},\"item\":{\"name\":\"Track\",\"album\":{\"images\":[{\"width\":300,\"height\":300,\"url\":\"https://evil.example/large\"},{\"width\":64,\"height\":64,\"url\":\"https://i.scdn.co/image/0123456789abcdef0123456789abcdef01234567\"}]}}}",200);
    schedule("invalid jpeg",200); spotify_remote_command(&p,&job); complete(&p);
    assert(spotify_cover_url_allowed(last_url) && !authorization_sent && p.result==ESP_OK);
    const char *images="{\"device\":{\"id\":\"fixture\",\"name\":\"Desktop\"},\"item\":{\"name\":\"Track\",\"album\":{\"images\":[{\"width\":64,\"height\":64,\"url\":\"https://i.scdn.co/image/1111111111111111111111111111111111111111\"},{\"width\":300,\"height\":300,\"url\":\"https://i.scdn.co/image/2222222222222222222222222222222222222222\"}]}}}";
    static char large[10001]; memset(large,'X',sizeof(large)-1);
    clock_us+=61000000; reset_responses(); unsigned before=request_count;
    schedule(images,200); schedule(large,200);
    responses[response_count]=(const char *)jpeg; response_sizes[response_count]=jpeg_size; statuses[response_count++]=200;
    spotify_remote_command(&p,&job); complete(&p);
    assert(request_count==before+3 && p.result==ESP_OK && !authorization_sent);
    assert(strstr(s_cover_url,"2222222222")); /* cache fallback for this desired image */
    int8_t lease=-1; const spotify_cover_t *art=spotify_cover_acquire(p.cover_generation,&lease);
    assert(art && art->width==64); spotify_cover_release(lease);
    clock_us+=10000000; reset_responses(); before=request_count; schedule(images,200);
    spotify_remote_command(&p,&job); complete(&p); assert(request_count==before+1);
    uint32_t kept=p.cover_generation;
    clock_us+=61000000; reset_responses(); before=request_count;
    schedule(images,200); schedule(large,200);
    spotify_remote_command(&p,&job); complete(&p);
    assert(request_count==before+2 && p.cover_generation==kept && s_cover_low_quality);
    art=spotify_cover_acquire(kept,&lease); assert(art && art->width==64); spotify_cover_release(lease);
    clock_us+=61000000; reset_responses(); before=request_count; schedule(images,200);
    responses[response_count]=(const char *)hq; response_sizes[response_count]=hq_size; statuses[response_count++]=200;
    spotify_remote_command(&p,&job); complete(&p);
    assert(request_count==before+2 && !s_cover_low_quality && p.cover_generation!=kept);
    art=spotify_cover_acquire(p.cover_generation,&lease); assert(art && art->width==300); spotify_cover_release(lease);
    clock_us+=61000000; reset_responses(); before=request_count; schedule(images,200);
    spotify_remote_command(&p,&job); complete(&p); assert(request_count==before+1);
    clock_us+=1000000; reset_responses(); schedule("",204);
    spotify_remote_command(&p,&job); complete(&p); assert(!p.available && !s_cover_attempt[0]);
    uint32_t cleared=p.cover_generation;
    clock_us+=1000000; reset_responses(); before=request_count; schedule(images,200);
    responses[response_count]=(const char *)hq; response_sizes[response_count]=hq_size; statuses[response_count++]=200;
    spotify_remote_command(&p,&job); complete(&p);
    assert(request_count==before+2 && p.cover_generation!=cleared && !s_cover_low_quality);
    art=spotify_cover_acquire(p.cover_generation,&lease); assert(art && art->width==300); spotify_cover_release(lease);
    /* A provider Retry-After survives a temporary player 204. */
    s_cover_url[0]=0; clock_us+=1000000; reset_responses(); schedule(images,200); schedule("{}",429);
    spotify_remote_command(&p,&job); complete(&p); assert(s_cover_rate_limited);
    clock_us+=1000000; reset_responses(); schedule("",204);
    spotify_remote_command(&p,&job); complete(&p); assert(s_cover_attempt[0]);
    clock_us+=1000000; reset_responses(); before=request_count; schedule(images,200);
    spotify_remote_command(&p,&job); complete(&p); assert(request_count==before+1);
    clock_us+=121000000; reset_responses(); schedule(images,200);
    responses[response_count]=(const char *)hq; response_sizes[response_count]=hq_size; statuses[response_count++]=200;
    spotify_remote_command(&p,&job); complete(&p); assert(!s_cover_rate_limited);
    assert(allocations==0);
    /* Larger bodies remain SRAM-only and require the actual post-TLS floor. */
    s_http=heap_caps_calloc(1,sizeof(*s_http),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    s_http->deadline=clock_us+18000000; reset_responses(); schedule(large,200);
    assert(request("https://i.scdn.co/image/3333333333333333333333333333333333333333",HTTP_METHOD_GET,NULL,false)==ESP_OK);
    assert(s_http->received==10000 && s_http->cover_body && !authorization_sent);
    free_bytes=80U*1024U+10000U; reset_responses(); schedule(large,200);
    assert(request("https://i.scdn.co/image/3333333333333333333333333333333333333333",HTTP_METHOD_GET,NULL,false)==ESP_ERR_NO_MEM);
    assert(!s_http->cover_body); free_bytes=400000;
    header_length=32769; reset_responses(); schedule(large,200);
    assert(request("https://i.scdn.co/image/3333333333333333333333333333333333333333",HTTP_METHOD_GET,NULL,false)==ESP_ERR_INVALID_SIZE);
    header_length=0; reset_responses(); schedule(large,200);
    assert(request("https://i.scdn.co/image/3333333333333333333333333333333333333333",HTTP_METHOD_GET,NULL,false)==ESP_OK);
    assert(s_http->cover_body && s_http->received==10000);
    heap_caps_free(s_http->cover_body); heap_caps_free(s_http); s_http=NULL; header_length=-1;
    assert(allocations==0 && active_http==0);
    puts("Spotify remote: PKCE refresh, cached access, fragmented HTTP, playback/device parsing, stale transfer, 204/401/429, backoff, memory admission, truncation/oversize PASS");
}
