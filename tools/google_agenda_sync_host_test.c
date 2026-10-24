#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static time_t wall_now;
static time_t fixture_time(time_t *out) { if (out) *out=wall_now; return wall_now; }
#define time fixture_time
#include "../firmware/main/google_agenda_sync.c"
#undef time

typedef struct { esp_http_client_method_t method; const char *url; int status; const char *body; esp_err_t result; } response_t;
static response_t responses[32];
static unsigned response_count, response_index, post_count;
static size_t free_heap=512U*1024U;
static size_t largest_heap=512U*1024U;
static esp_http_client_config_t current;
static char submitted[2048];
static bool has_match;
static size_t read_position, write_position;
static bool chunked, expire_on_read, truncated;
static int64_t fake_time=1000;
bool google_agenda_linked(void) { return true; }
esp_err_t google_agenda_credentials_copy_private(google_agenda_credentials_t *out)
{ memset(out,0,sizeof(*out)); strcpy(out->client_id,"test.apps.googleusercontent.com"); strcpy(out->client_secret,"fixture"); strcpy(out->refresh_token,"fixture"); return ESP_OK; }
esp_err_t network_validation_service_request_google_sync(void) { return ESP_OK; }
int64_t esp_timer_get_time(void) { return fake_time; }
esp_err_t esp_efuse_mac_get_default(uint8_t *out) { for (unsigned i=0;i<6;++i) out[i]=(uint8_t)i; return ESP_OK; }
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) { (void)caps; return calloc(n,size); }
void heap_caps_free(void *p) { free(p); }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return free_heap; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return largest_heap; }
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{ current=*config; submitted[0]=0; has_match=false; assert(config->disable_auto_redirect); return &current; }
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *key,const char *value)
{ (void)c; assert(strchr(value,'\r') == NULL && strchr(value,'\n') == NULL); if (!strcmp(key,"If-Match")) has_match=true; return ESP_OK; }
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t c,const char *body,int size)
{ (void)c; assert(size > 0 && (unsigned)size < sizeof(submitted)); memcpy(submitted,body,(size_t)size); submitted[size]=0; return ESP_OK; }
esp_err_t esp_http_client_perform(esp_http_client_handle_t c)
{
    (void)c; assert(response_index < response_count);
    response_t *r=&responses[response_index++];
    assert(current.method == r->method && strstr(current.url,r->url) != NULL);
    if (strstr(current.url,"/primary/events?")) {
        assert(strstr(current.url,"timeMin=2026-10-01T03:00:00Z"));
        assert(strstr(current.url,"timeMax=2026-11-01T03:00:00Z"));
        assert(strstr(current.url,"singleEvents=true") && strstr(current.url,"maxResults=10"));
    }
    if (current.method == HTTP_METHOD_POST && strstr(current.url,"oauth2") == NULL) ++post_count;
    if (current.method == HTTP_METHOD_PATCH || current.method == HTTP_METHOD_DELETE) assert(has_match);
    return r->result;
}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t c,int timeout)
{ (void)c; assert(timeout > 0 && timeout <= 10000); return ESP_OK; }
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int bytes)
{ (void)bytes; read_position=0U; write_position=0U; return esp_http_client_perform(c); }
int esp_http_client_write(esp_http_client_handle_t c,const char *data,int bytes)
{ (void)c; assert(bytes > 0 && write_position+(unsigned)bytes < sizeof(submitted)); memcpy(submitted+write_position,data,(size_t)bytes); write_position+=(size_t)bytes; submitted[write_position]=0; return bytes; }
long long esp_http_client_fetch_headers(esp_http_client_handle_t c)
{ (void)c; const char *body=responses[response_index-1U].body; return chunked || body == NULL ? 0 : (long long)strlen(body); }
int esp_http_client_read(esp_http_client_handle_t c,char *out,int bytes)
{
    (void)c; const char *body=responses[response_index-1U].body;
    size_t remaining=body == NULL ? 0U : strlen(body)-read_position;
    size_t count=remaining < (size_t)bytes ? remaining : (size_t)bytes;
    if (count) memcpy(out,body+read_position,count);
    if (expire_on_read) fake_time+=121LL*1000000LL;
    read_position+=count; return (int)count;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c)
{ (void)c; const char *body=responses[response_index-1U].body; return !truncated && (body == NULL || read_position == strlen(body)); }
esp_err_t esp_http_client_close(esp_http_client_handle_t c) { (void)c; return ESP_OK; }
int esp_http_client_get_status_code(esp_http_client_handle_t c) { (void)c; return responses[response_index-1U].status; }
void esp_http_client_cleanup(esp_http_client_handle_t c) { (void)c; }

static void add(esp_http_client_method_t method,const char *url,int status,const char *body,esp_err_t result)
{ responses[response_count++]=(response_t){method,url,status,body,result}; }
static void reset(void) { response_count=0; response_index=0; fake_time=1000; chunked=false; expire_on_read=false; truncated=false; }
static void auth(void) { add(HTTP_METHOD_POST,"oauth2.googleapis.com/token",200,"{\"access_token\":\"fixture-access\"}",ESP_OK); }
static void lists(const char *tasks) {
    add(HTTP_METHOD_GET,"tasks/v1/lists/@default/tasks?",200,tasks,ESP_OK);
    add(HTTP_METHOD_GET,"calendar/v3/calendars/primary/events?",200,"{}",ESP_OK);
}
static const char *task="{\"id\":\"task123\",\"etag\":\"\\\"t1\\\"\",\"title\":\"Comprar peças\",\"status\":\"needsAction\"}";
static bool run(agenda_data_t *data,esp_err_t expected)
{
    assert(google_agenda_request_sync(data) == ESP_OK);
    assert(google_agenda_process_sync() == expected);
    esp_err_t result; bool changed;
    assert(google_agenda_take_result(data,&result,&changed) && result == expected);
    assert(response_index == response_count && agenda_data_valid(data));
    assert(!google_agenda_take_result(data,&result,&changed)); return changed;
}
static void monthly(void)
{
    agenda_data_t data; agenda_init(&data);
    const char *early="{\"items\":[{\"id\":\"early\",\"etag\":\"e1\",\"summary\":\"Passado no mês\",\"start\":{\"dateTime\":\"2026-10-01T12:00:00Z\"},\"end\":{\"dateTime\":\"2026-10-01T13:00:00Z\"}}],\"nextPageToken\":\"next/a+b=\"}";
    const char *birthday="{\"items\":[{\"id\":\"birthday_20261031\",\"etag\":\"e2\",\"summary\":\"Aniversário\",\"start\":{\"date\":\"2026-10-31\"},\"end\":{\"date\":\"2026-11-01\"}}]}";
    for (unsigned repeat=0U;repeat<2U;++repeat) {
        reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
        add(HTTP_METHOD_GET,"/primary/events?",200,early,ESP_OK);
        add(HTTP_METHOD_GET,"pageToken=next%2Fa%2Bb%3D",200,birthday,ESP_OK);
        const bool changed=run(&data,ESP_OK);
        assert(changed == (repeat == 0U) && data.count == 2U && data.items[1].reserved[0]);
    }
    /* A failed later page must not treat unobserved cached events as deleted. */
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
    add(HTTP_METHOD_GET,"/primary/events?",200,early,ESP_OK);
    add(HTTP_METHOD_GET,"pageToken=next",503,"{}",ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_RESPONSE) && data.count == 2U);
    /* Absence is reconciled only after a successful complete monthly list. */
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
    add(HTTP_METHOD_GET,"/primary/events?",200,birthday,ESP_OK);
    assert(run(&data,ESP_OK) && data.count == 1U && data.items[0].reserved[0]);
    agenda_init(&data);
    static char pages[7][4096];
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
    for (unsigned page=0U;page<7U;++page) {
        size_t used=(size_t)snprintf(pages[page],sizeof(pages[page]),"{\"items\":[");
        for (unsigned j=0U;j<10U && page*10U+j<AGENDA_MAX_ITEMS;++j) {
            unsigned i=page*10U+j;
            used+=(size_t)snprintf(pages[page]+used,sizeof(pages[page])-used,
                "%s{\"id\":\"event%u\",\"etag\":\"e1\",\"summary\":\"Evento %u\",\"start\":{\"dateTime\":\"2026-10-15T12:00:00Z\"},\"end\":{\"dateTime\":\"2026-10-15T13:00:00Z\"}}",j ? "," : "",i,i);
        }
        snprintf(pages[page]+used,sizeof(pages[page])-used,page<6U ? "],\"nextPageToken\":\"page%u\"}" : "]}",page+1U);
        add(HTTP_METHOD_GET,"/primary/events?",200,pages[page],ESP_OK);
    }
    assert(run(&data,ESP_OK) && data.count == AGENDA_MAX_ITEMS);
    /* Full cache replacement must not get stuck when Google deletes one item
     * and adds another; a 65th live item fails without dropping the cache. */
    char *old_id=strstr(pages[0],"event0"); assert(old_id); memcpy(old_id,"new000",6U);
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
    for (unsigned i=0U;i<7U;++i) add(HTTP_METHOD_GET,"/primary/events?",200,pages[i],ESP_OK);
    assert(run(&data,ESP_OK) && data.count == AGENDA_MAX_ITEMS && !strcmp(data.remote[0].id,"new000"));
    char *end=pages[6]+strlen(pages[6])-1U;
    strcpy(end,",\"nextPageToken\":\"overflow\"}");
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{}",ESP_OK);
    for (unsigned i=0U;i<7U;++i) add(HTTP_METHOD_GET,"/primary/events?",200,pages[i],ESP_OK);
    add(HTTP_METHOD_GET,"pageToken=overflow",200,birthday,ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_SIZE) && data.count == AGENDA_MAX_ITEMS && !strcmp(data.remote[0].id,"new000"));
}
int main(void)
{
    _putenv("TZ=BRT3"); _tzset();
    uint32_t wall; assert(google_agenda_parse_time("2026-10-21T12:00:00Z",&wall)); wall_now=(time_t)wall;
    agenda_data_t data; agenda_init(&data);
    agenda_command_t c={.request_id=1,.action=AGENDA_UPSERT,.item={.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE}};
    strcpy(c.item.title,"Comprar peças"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    reset(); auth(); lists("{}"); add(HTTP_METHOD_POST,"tasks/v1/lists/@default/tasks",200,task,ESP_OK);
    assert(run(&data,ESP_OK) && post_count == 1U && !strcmp(data.remote[0].id,"task123") && !data.remote[0].dirty);
    reset(); auth();
    char list[512]; snprintf(list,sizeof(list),"{\"items\":[%s]}",task); lists(list);
    assert(!run(&data,ESP_OK)); /* unchanged polling must not cause flash writes */
    c.item=data.items[0]; strcpy(c.item.title,"Alteração local"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    const char *remote="{\"id\":\"task123\",\"etag\":\"\\\"t2\\\"\",\"title\":\"Alteração Google\",\"status\":\"needsAction\"}";
    reset(); auth(); add(HTTP_METHOD_GET,"/tasks/task123",200,remote,ESP_OK);
    assert(run(&data,GOOGLE_CONFLICT) && data.remote[0].reserved[0] && !strcmp(data.items[0].title,"Alteração local"));
    c.item=data.items[0]; c.action=AGENDA_USE_GOOGLE; assert(agenda_apply(&data,&c) == AGENDA_OK);
    reset(); auth();
    snprintf(list,sizeof(list),"{\"items\":[%s]}",remote); lists(list);
    assert(run(&data,ESP_OK) && !strcmp(data.items[0].title,"Alteração Google"));
    /* POST response lost: recover by durable installation/id marker, then PATCH. */
    agenda_init(&data); c.action=AGENDA_UPSERT; c.item=(agenda_item_t){.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE};
    strcpy(c.item.title,"Comprar peças"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    reset(); auth(); lists("{}"); add(HTTP_METHOD_POST,"tasks/v1/lists/@default/tasks",0,NULL,ESP_FAIL);
    assert(!run(&data,ESP_FAIL)); unsigned posts_before=post_count;
    reset(); auth(); lists("{\"items\":[{\"id\":\"task123\",\"etag\":\"\\\"t1\\\"\",\"title\":\"Comprar peças\",\"status\":\"needsAction\",\"notes\":\"NP2:000102030405:00000001:00000001\"}]}");
    add(HTTP_METHOD_PATCH,"/tasks/task123",200,task,ESP_OK);
    assert(run(&data,ESP_OK) && post_count == posts_before);
    reset(); auth(); lists("{}");
    assert(run(&data,ESP_OK) && data.count == 0U);
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,"{\"nextPageToken\":\"more\",\"items\":[]}",ESP_OK);
    add(HTTP_METHOD_GET,"pageToken=more",200,"{\"nextPageToken\":\"more\",\"items\":[]}",ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_RESPONSE));
    reset(); auth(); responses[0].status=400; assert(!run(&data,ESP_ERR_INVALID_STATE));
    reset(); add(HTTP_METHOD_POST,"oauth2.googleapis.com/token",400,"{\"error\":\"invalid_grant\"}",ESP_OK);
    assert(!run(&data,AGENDA_SYNC_AUTH_REQUIRED));
    reset(); add(HTTP_METHOD_POST,"oauth2.googleapis.com/token",500,"{\"error\":\"invalid_grant\"}",ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_STATE));
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",401,"{}",ESP_OK);
    assert(!run(&data,AGENDA_SYNC_AUTH_REQUIRED));
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",403,"{}",ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_RESPONSE));
    /* Calendar create/read/delete uses the deterministic ID and If-Match. */
    c=(agenda_command_t){.request_id=2,.action=AGENDA_UPSERT,.item={.kind=AGENDA_EVENT,
        .start_unix_s=1800000000U,.end_unix_s=1800003600U,.reminder_minutes=15U}};
    strcpy(c.item.title,"Reunião"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    char event_reply[768],from[21],to[21];
    google_agenda_format_time(c.item.start_unix_s,from); google_agenda_format_time(c.item.end_unix_s,to);
    snprintf(event_reply,sizeof(event_reply),"{\"id\":\"np20001020304050000000100000002\",\"etag\":\"\\\"e1\\\"\",\"summary\":\"Reunião\",\"start\":{\"dateTime\":\"%s\"},\"end\":{\"dateTime\":\"%s\"},\"reminders\":{\"overrides\":[{\"method\":\"popup\",\"minutes\":15}]}}",from,to);
    reset(); auth(); lists("{}"); add(HTTP_METHOD_POST,"calendar/v3/",200,event_reply,ESP_OK);
    assert(run(&data,ESP_OK) && !data.remote[0].dirty);
    c.item=data.items[0]; c.action=AGENDA_DELETE; assert(agenda_apply(&data,&c) == AGENDA_OK);
    reset(); auth(); add(HTTP_METHOD_GET,"/events/np2",200,event_reply,ESP_OK); lists("{}");
    add(HTTP_METHOD_DELETE,"/events/np2",204,NULL,ESP_OK);
    assert(run(&data,ESP_OK) && data.count == 0U);
    /* A malformed create response cannot partially corrupt durable metadata. */
    c=(agenda_command_t){.request_id=3,.action=AGENDA_UPSERT,
        .item={.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE}};
    strcpy(c.item.title,"Resposta inválida"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    char long_id[130],bad_reply[256]; memset(long_id,'a',sizeof(long_id)-1U); long_id[sizeof(long_id)-1U]=0;
    snprintf(bad_reply,sizeof(bad_reply),"{\"id\":\"%s\",\"etag\":\"t1\"}",long_id);
    reset(); auth(); lists("{}"); add(HTTP_METHOD_POST,"tasks/v1/",200,bad_reply,ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_RESPONSE) && data.remote[0].id[0] == 0 && data.remote[0].dirty);
    static char oversized[BODY_LIMIT+2U]; memset(oversized,'x',sizeof(oversized)-1U);
    reset(); auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,oversized,ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_SIZE) && read_position == 0U);
    reset(); chunked=true; auth(); add(HTTP_METHOD_GET,"tasks/v1/",200,oversized,ESP_OK);
    assert(!run(&data,ESP_ERR_INVALID_SIZE) && read_position == BODY_LIMIT+1U);
    reset(); expire_on_read=true; auth(); assert(!run(&data,ESP_ERR_TIMEOUT));
    reset(); truncated=true; auth(); assert(!run(&data,ESP_ERR_INVALID_RESPONSE));
    reset(); free_heap=1024; assert(!run(&data,ESP_ERR_NO_MEM));
    /* Admission boundaries: preserve the floor and contiguous-block check;
     * a rejected request must not even begin token HTTP. */
    agenda_init(&data);
    const size_t required=GOOGLE_INTERNAL_FLOOR_BYTES+GOOGLE_TLS_ENVELOPE_BYTES+sizeof(http_workspace_t)+4096U;
    reset(); free_heap=required-1U; assert(!run(&data,ESP_ERR_NO_MEM) && response_index == 0U);
    reset(); free_heap=required; auth(); lists("{}"); assert(!run(&data,ESP_OK));
    reset(); free_heap=512U*1024U; largest_heap=32767U;
    assert(!run(&data,ESP_ERR_NO_MEM) && response_index == 0U);
    reset(); largest_heap=32768U; auth(); lists("{}"); assert(!run(&data,ESP_OK));
    monthly();
    puts("Google sync: monthly bounds, 64 items/7 pages, birthdays, safe reconciliation, CRUD/conflicts, auth/memory failure PASS");
    return 0;
}
