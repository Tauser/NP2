#include "spotify_http_bridge.h"
#include "platform.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct test_semaphore { pthread_mutex_t lock; pthread_cond_t cond; bool ready; };
static atomic_int allocations, semaphores, active_http, http_calls, enqueue_calls;
static int mode, enqueue_result, failure_stage, busy_retries;
static esp_err_t prerequisite;
static pthread_t worker;
static bool worker_created;
static size_t offset, free_bytes = 300000, largest = 100000;
static size_t last_job_bytes;
static atomic_llong clock_us;
static esp_http_client_config_t config;
static char retained_url[SPOTIFY_HTTP_URL_MAX+1];
static char *mutate_input;
static atomic_bool entered_read, leave_read;

SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    if (failure_stage == 2) return NULL;
    SemaphoreHandle_t s=calloc(1,sizeof(*s));assert(s);
    pthread_mutex_init(&s->lock,NULL);pthread_cond_init(&s->cond,NULL);++semaphores;return s;
}
int xSemaphoreTake(SemaphoreHandle_t s, unsigned ticks)
{
    assert(ticks>0 && ticks<=25000);
    if (mode==8) return 0; /* Caller deadline, worker deliberately completes later. */
    if (mode==13) { while(!entered_read){} return 0; }
    pthread_mutex_lock(&s->lock);
    while(!s->ready) pthread_cond_wait(&s->cond,&s->lock);
    pthread_mutex_unlock(&s->lock);return 1;
}
void xSemaphoreGive(SemaphoreHandle_t s)
{ pthread_mutex_lock(&s->lock);s->ready=true;pthread_cond_signal(&s->cond);pthread_mutex_unlock(&s->lock); }
void vSemaphoreDelete(SemaphoreHandle_t s)
{ pthread_cond_destroy(&s->cond);pthread_mutex_destroy(&s->lock);free(s);--semaphores; }
void *heap_caps_calloc(size_t n,size_t size,unsigned caps)
{ assert(caps==3);last_job_bytes=n*size;if(failure_stage==1)return NULL;void *p=calloc(n,size);if(p)++allocations;return p; }
void *heap_caps_malloc(size_t size,unsigned caps)
{ assert(caps==3);if(failure_stage==3)return NULL;void *p=malloc(size);if(p)++allocations;return p; }
void heap_caps_free(void *p) { if(p){--allocations;free(p);} }
size_t heap_caps_get_free_size(unsigned caps) { assert(caps==3);return free_bytes; }
size_t heap_caps_get_largest_free_block(unsigned caps) { assert(caps==3);return largest; }
int64_t esp_timer_get_time(void) { return clock_us; }
void vTaskDelay(unsigned ticks) { assert(ticks==10);clock_us += (int64_t)ticks*1000; }
esp_err_t heap_caps_monitor_local_minimum_free_size_start(void) {return ESP_OK;}
esp_err_t heap_caps_monitor_local_minimum_free_size_stop(void) {return ESP_OK;}
size_t heap_caps_get_minimum_free_size(unsigned caps) {assert(caps==3);return free_bytes;}
unsigned uxTaskGetStackHighWaterMark(void *task) {(void)task;return 4096;}
esp_err_t esp_crt_bundle_attach(void *p) { (void)p;return ESP_OK; }
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c)
{
    assert(atomic_fetch_add(&active_http,1)==0);++http_calls;
    assert(c->crt_bundle_attach && c->disable_auto_redirect && !c->max_redirection_count);
    assert(c->buffer_size==512 && c->buffer_size_tx==4096);
    config=*c;strcpy(retained_url,c->url);offset=0;return &config;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *k,const char *v)
{ assert(c==&config && k && v);return ESP_OK; }
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t c,int ms)
{ assert(c==&config && ms>0 && ms<=5000);return ESP_OK; }
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n)
{ assert(c==&config && n==0);return mode==9 ? ESP_FAIL : ESP_OK; }
int esp_http_client_write(esp_http_client_handle_t c,const char *p,int n)
{ (void)c;(void)p;return n; }
static void header(const char *key,const char *value)
{
    esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_HEADER,.user_data=config.user_data,
        .header_key=(char*)key,.header_value=(char*)value};config.event_handler(&e);
}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c)
{
    assert(c==&config);
    if(mode==13){entered_read=true;while(!leave_read){};}
    if(mode==5)header("Content-Encoding","gzip");
    if(mode==6){header("Content-Range","bytes 0-1/10");header("Content-Range","bytes 0-1/10");}
    if(mode==10){char big[100];memset(big,'x',99);big[99]=0;header("Content-Range",big);}
    return mode==2 ? 8193 : mode==1 ? 0 : 2;
}
int esp_http_client_get_status_code(esp_http_client_handle_t c) { assert(c==&config);return mode==3 ? 302 : 200; }
bool esp_http_client_is_chunked_response(esp_http_client_handle_t c) { assert(c==&config);return mode==1; }
int esp_http_client_read(esp_http_client_handle_t c,char *p,int n)
{
    assert(c==&config);
    if(mode==7){clock_us=21000000;return 0;}
    size_t size=mode==4 ? 8193 : mode==11 ? 1 : 2;
    if(offset>=size)return 0;
    size_t count=size-offset;if(count>(size_t)n)count=(size_t)n;
    memset(p,'x',count);offset+=count;return (int)count;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) { assert(c==&config);return mode!=12; }
esp_err_t esp_http_client_close(esp_http_client_handle_t c) { assert(c==&config);return ESP_OK; }
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) { assert(c==&config);--active_http;return ESP_OK; }
static void *run_worker(void *p) { (void)p;spotify_http_bridge_process(prerequisite);return NULL; }
esp_err_t network_validation_service_request_spotify_http(void)
{
    ++enqueue_calls;
    if(busy_retries>0){--busy_retries;return ESP_ERR_TIMEOUT;}
    if(enqueue_result!=ESP_OK)return enqueue_result;
    if(mutate_input)strcpy(mutate_input,"https://evil.invalid/");
    if(mode!=8){worker_created=true;assert(!pthread_create(&worker,NULL,run_worker,NULL));}
    return ESP_OK;
}
static void finish()
{
    if(worker_created){pthread_join(worker,NULL);worker_created=false;}
    assert(!allocations && !semaphores && !active_http);
}
static int request(void)
{
    spotify_http_request_t r={.url="https://apresolve.spotify.com/"};np_spotify_http_reply_t out;
    int result=spotify_http_bridge_exchange(&r,&out);
    if(result==ESP_OK){assert(last_job_bytes<1024);assert(out.body && out.size==2 && out.capacity==4096);heap_caps_free(out.body);}
    else assert(!out.body && !out.capacity && !out.size);
    return result;
}
int main(void)
{
    for(mode=0;mode<=13;++mode){
        clock_us=0;
        int result=request();
        if(mode==0 || mode==1){assert(result==ESP_OK);assert(!strcmp(retained_url,"https://apresolve.spotify.com/"));}
        else assert(result!=ESP_OK);
        if(mode==8){
            assert(allocations==1 && semaphores==1);
            assert(request()==ESP_ERR_TIMEOUT); /* Late worker owns slot until its ref is released. */
            assert(spotify_http_bridge_process(ESP_OK)==ESP_ERR_TIMEOUT);
        }
        if(mode==13){assert(request()==ESP_ERR_TIMEOUT);leave_read=true;}
        finish();
    }
    mode=0;clock_us=0;
    prerequisite=ESP_FAIL;int before=http_calls;assert(request()==ESP_FAIL);finish();assert(http_calls==before);
    prerequisite=ESP_OK;
    enqueue_result=ESP_ERR_INVALID_STATE;assert(request()==ESP_ERR_INVALID_STATE);finish();
    clock_us=0;enqueue_result=ESP_ERR_TIMEOUT;before=enqueue_calls;
    assert(request()==ESP_ERR_TIMEOUT && clock_us==20000000 && enqueue_calls-before==2000);finish();
    enqueue_result=ESP_OK;
    clock_us=0;busy_retries=3;before=enqueue_calls;
    assert(request()==ESP_OK && clock_us==30000 && enqueue_calls-before==4);finish();
    for(failure_stage=1;failure_stage<=3;++failure_stage){assert(request()==ESP_ERR_NO_MEM);finish();}
    failure_stage=0;
    free_bytes=100000;before=http_calls;assert(request()==ESP_ERR_NO_MEM);finish();assert(http_calls==before);
    free_bytes=151551;assert(request()==ESP_ERR_NO_MEM);finish();assert(http_calls==before);
    free_bytes=154619;assert(request()==ESP_OK);finish(); /* Measured P4 retry case. */
    before=http_calls;
    free_bytes=300000;largest=30000;assert(request()==ESP_ERR_NO_MEM);finish();assert(http_calls==before);
    largest=100000;assert(request()==ESP_OK);finish();
    char input[64]="https://apresolve.spotify.com/";
    mutate_input=input;
    spotify_http_request_t r={.url=input};np_spotify_http_reply_t out;
    assert(spotify_http_bridge_exchange(&r,&out)==ESP_OK);
    assert(!strcmp(retained_url,"https://apresolve.spotify.com/"));
    heap_caps_free(out.body);finish();mutate_input=NULL;
    assert(spotify_http_bridge_process(ESP_OK)==ESP_ERR_INVALID_STATE);
    puts("Spotify bridge: deadlines/late worker, single slot, DNS failure, memory admission, TLS failure, headers, chunked/oversize/truncated responses and ownership PASS");
}
