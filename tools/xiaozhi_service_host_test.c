/* Executes the production session service with bounded transport/task fakes. */
#define NP2_XIAOZHI_HOST_TEST 1
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/main/xiaozhi_service.c"

struct fake_task { eTaskState state; } task;
struct fake_events { unsigned bits; } events;
struct fake_ws { bool connected; } websocket;
static int64_t clock_us;
static void (*session_hook)(void);
static bool omit_hello, omit_hello_once;
static unsigned live_allocations,http_calls,job_requests,ws_calls;
static int fail_allocation=-1,task_mode;
static bool enqueue_ok=true,finish_early,release_voice_when_stopped,send_handover,keep_listening;
static size_t internal_free=200000;
static const char *reply="{}";
static size_t reply_offset;
static int http_status=200;

void *heap_caps_malloc(size_t n,unsigned caps) {
    (void)caps;if(fail_allocation==0)return NULL;if(fail_allocation>0)--fail_allocation;
    void *p=malloc(n);assert(p);++live_allocations;return p;
}
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) {
    void *p=heap_caps_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;
}
void *heap_caps_realloc(void *p,size_t n,unsigned caps) {(void)caps;if(!p)++live_allocations;return realloc(p,n);}
void heap_caps_free(void *p) {if(p){assert(live_allocations);--live_allocations;free(p);}}
size_t heap_caps_get_free_size(unsigned caps){(void)caps;return internal_free;}
size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 100000;}
size_t heap_caps_get_minimum_free_size(unsigned caps){(void)caps;return internal_free;}
unsigned uxTaskGetStackHighWaterMark(void *h){(void)h;return 4096;}
int64_t esp_timer_get_time(void){return clock_us;}
void vTaskDelay(unsigned ticks){clock_us+=(int64_t)ticks*1000;voice_capture_owner_maintain();music_audio_owner_maintain();
    if(release_voice_when_stopped && s_stop)music_audio_stop();
    if(session_hook)session_hook();}
EventGroupHandle_t xEventGroupCreate(void){events.bits=0;return &events;}
EventBits_t xEventGroupSetBits(EventGroupHandle_t e,unsigned b){return e->bits|=b;}
EventBits_t xEventGroupClearBits(EventGroupHandle_t e,unsigned b){unsigned old=e->bits;e->bits&=~b;return old;}
EventBits_t xEventGroupGetBits(EventGroupHandle_t e){return e->bits;}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t e,unsigned b,int c,int all,unsigned t){(void)c;(void)all;if(!(e->bits&b))vTaskDelay(t);return e->bits&b;}
int xTaskCreatePinnedToCoreWithCaps(void (*fn)(void *),const char *n,unsigned stack,void *arg,unsigned p,TaskHandle_t *out,int core,unsigned caps){
    (void)fn;(void)n;(void)stack;(void)arg;(void)p;(void)core;(void)caps;
    if(task_mode==1)return 0;
    *out=&task;task.state=task_mode==2 ? eRunning : eSuspended;
    s_codec_ready=task_mode==0;s_codec_done=task_mode!=2;s_codec_result=task_mode==3 ? ESP_FAIL : ESP_OK;
    return pdPASS;
}
void vTaskDeleteWithCaps(TaskHandle_t h){assert(h==&task && task.state==eSuspended);}
void vTaskSuspend(TaskHandle_t h){(void)h;assert(!"codec fake must not execute task body");}
eTaskState eTaskGetState(TaskHandle_t h){assert(h==&task);return task.state;}
const char *esp_err_to_name(esp_err_t r){return r==ESP_OK ? "ESP_OK" : "ESP_FAIL";}
esp_err_t app_state_request_refresh(void){return ESP_OK;}
esp_err_t network_validation_service_request_xiaozhi(void){++job_requests;if(finish_early){s_busy=false;s_live.state=XIAOZHI_STATE_ERROR;++s_live.revision;}return enqueue_ok ? ESP_OK : ESP_ERR_TIMEOUT;}
const esp_app_desc_t *esp_app_get_description(void){static esp_app_desc_t app={"host-test"};return &app;}
int esp_app_get_elf_sha256(char *s,size_t n){snprintf(s,n,"abcd");return 4;}
esp_err_t esp_flash_get_size(void *f,uint32_t *n){(void)f;*n=33554432;return ESP_OK;}
esp_err_t esp_wifi_get_mac(int iface,uint8_t *m){(void)iface;memset(m,1,6);return ESP_OK;}
int mbedtls_sha256(const unsigned char *s,size_t n,unsigned char *d,int mode){(void)s;(void)n;(void)mode;memset(d,2,32);return 0;}
esp_err_t esp_crt_bundle_attach(void *p){(void)p;return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c){assert(c->disable_auto_redirect);++http_calls;reply_offset=0;return (void*)1;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char *n,const char *v){(void)c;(void)n;(void)v;return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n){(void)c;(void)n;return ESP_OK;}
int esp_http_client_write(esp_http_client_handle_t c,const char *b,int n){(void)c;(void)b;return n;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){(void)c;return (int64_t)strlen(reply);}
int esp_http_client_get_status_code(esp_http_client_handle_t c){(void)c;return http_status;}
int esp_http_client_read(esp_http_client_handle_t c,char *b,int n){(void)c;size_t left=strlen(reply)-reply_offset;if((size_t)n>left)n=(int)left;memcpy(b,reply+reply_offset,n);reply_offset+=n;return n;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c){(void)c;return reply_offset==strlen(reply);}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){(void)c;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){(void)c;return ESP_OK;}
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *c){assert(strncmp(c->uri,"wss://",6)==0);++ws_calls;websocket.connected=false;return &websocket;}
esp_err_t esp_websocket_register_events(esp_websocket_client_handle_t w,int e,void (*cb)(void *,esp_event_base_t,int32_t,void *),void *arg){(void)w;(void)e;(void)cb;(void)arg;return ESP_OK;}
esp_err_t esp_websocket_client_start(esp_websocket_client_handle_t w){w->connected=true;ws_event(NULL,NULL,WEBSOCKET_EVENT_CONNECTED,NULL);return ESP_OK;}
esp_err_t esp_websocket_client_stop(esp_websocket_client_handle_t w){w->connected=false;return ESP_OK;}
esp_err_t esp_websocket_client_destroy(esp_websocket_client_handle_t w){assert(!w->connected);return ESP_OK;}
bool esp_websocket_client_is_connected(esp_websocket_client_handle_t w){return w->connected;}
int esp_websocket_client_send_text(esp_websocket_client_handle_t w,const char *text,int n,unsigned t){(void)w;(void)t;
    if(strstr(text,"\"hello\"")) {
        if(omit_hello_once) omit_hello_once=false;
        else if(!omit_hello) {
            const char *hello="{\"type\":\"hello\",\"transport\":\"websocket\"}";
            inbox_push(hello,strlen(hello));
        }
    }
    if(!keep_listening && strstr(text,"\"listen\"") && strstr(text,"\"state\":\"start\"")){if(send_handover)xiaozhi_service_yield_network();else s_stop=true;}
    return n;
}
int esp_websocket_client_send_bin(esp_websocket_client_handle_t w,const char *b,int n,unsigned t){(void)w;(void)b;(void)t;return n;}
esp_audio_err_t esp_opus_enc_open(void *c,unsigned n,void **h){(void)c;(void)n;*h=(void*)1;return 0;}
esp_audio_err_t esp_opus_dec_open(void *c,unsigned n,void **h){return esp_opus_enc_open(c,n,h);}
esp_audio_err_t esp_opus_enc_process(void *h,esp_audio_enc_in_frame_t *i,esp_audio_enc_out_frame_t *o){(void)h;(void)i;o->encoded_bytes=1;return 0;}
esp_audio_err_t esp_opus_dec_decode(void *h,esp_audio_dec_in_raw_t *i,esp_audio_dec_out_frame_t *o,esp_audio_dec_info_t *info){(void)h;(void)i;(void)info;o->decoded_size=0;return 0;}
void esp_opus_enc_close(void *h){(void)h;}
esp_audio_err_t esp_opus_dec_close(void *h){(void)h;return 0;}
esp_audio_err_t esp_opus_dec_reset(void *h){(void)h;return 0;}

static void idle_cleanup(void){voice_capture_owner_maintain();music_audio_owner_maintain();assert(!live_allocations);}
static void start(void){xiaozhi_projection_t p={0};xiaozhi_command_t c={XIAOZHI_START};xiaozhi_command(&p,&c);xiaozhi_tick(&p,true,true);assert(s_busy);}
static void test_activation(void){
    reply="{\"activation\":{\"code\":\"123456\",\"challenge\":\"x\"}}";
    start();int64_t before=clock_us;assert(xiaozhi_service_process(ESP_OK)==ESP_OK);
    assert(!s_busy && s_activation_pending && s_live.state==XIAOZHI_STATE_ACTIVATION);
    assert(clock_us==before && http_calls==1 && !ws_calls);idle_cleanup();
    xiaozhi_projection_t p={0};unsigned jobs=job_requests;xiaozhi_tick(&p,true,true);assert(job_requests==jobs);
    clock_us+=ACTIVATION_POLL_US;xiaozhi_tick(&p,true,true);assert(job_requests==jobs+1 && s_busy);
    http_status=202;assert(xiaozhi_service_process(ESP_OK)==ESP_OK);assert(s_activation_pending && http_calls==2);idle_cleanup();
    xiaozhi_command_t stop={XIAOZHI_STOP};xiaozhi_command(&p,&stop);assert(!s_activation_pending && s_live.state==XIAOZHI_STATE_IDLE);
    http_status=200;
}
static void test_codec_quarantine(void){
    reply="{\"websocket\":{\"url\":\"wss://example.test/\"}}";task_mode=2;
    start();assert(xiaozhi_service_process(ESP_OK)==XIAOZHI_ERR_CODEC_STUCK);
    assert(s_codec_quarantined && s_codec && s_up.data && s_down.data && s_codec_task);
    void *work=s_codec;unsigned calls=ws_calls;start();assert(xiaozhi_service_process(ESP_OK)==XIAOZHI_ERR_CODEC_STUCK);
    assert(s_codec==work && ws_calls==calls);
    s_codec_done=true;task.state=eSuspended;assert(xiaozhi_service_process(ESP_ERR_INVALID_STATE)==ESP_ERR_INVALID_STATE);
    assert(!s_codec_quarantined && !s_codec && !s_codec_task);idle_cleanup();
}
static void test_control_and_fragments(void){
    s_inbox=heap_caps_calloc(INBOX_SLOTS,INBOX_BYTES,MALLOC_CAP_INTERNAL);s_inbox_count=s_inbox_head=0;
    s_assembly=heap_caps_malloc(INBOX_BYTES,MALLOC_CAP_INTERNAL);s_control_overflow=false;s_assembly_opcode=0;
    const char *msg="{\"type\":\"tts\",\"state\":\"stop\"}";int length=(int)strlen(msg);
    esp_websocket_event_data_t e={.op_code=1,.data_ptr=msg,.data_len=9,.payload_len=9,.fin=false};ws_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&e);
    assert(!s_inbox_count);e.op_code=0;e.data_ptr=msg+9;e.data_len=e.payload_len=length-9;e.fin=true;ws_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&e);
    char out[INBOX_BYTES];assert(inbox_pop(out)==(size_t)length && !strcmp(out,msg));
    for(unsigned i=0;i<INBOX_SLOTS;++i)inbox_push(msg,length);
    assert(!s_control_overflow);
    inbox_push(msg,length);assert(s_control_overflow);
    heap_caps_free(s_inbox);heap_caps_free(s_assembly);s_inbox=NULL;s_assembly=NULL;s_inbox_count=0;idle_cleanup();
}
static void test_admission_and_start_race(void){
    internal_free=100000;start();assert(xiaozhi_service_process(ESP_OK)==ESP_ERR_NO_MEM);idle_cleanup();internal_free=200000;
    finish_early=true;xiaozhi_projection_t p={0};xiaozhi_command_t c={XIAOZHI_START};xiaozhi_command(&p,&c);xiaozhi_tick(&p,true,true);
    assert(!s_busy && s_live.state==XIAOZHI_STATE_ERROR);finish_early=false;
    enqueue_ok=false;xiaozhi_command(&p,&c);xiaozhi_tick(&p,true,true);assert(!s_busy && s_start_wanted);enqueue_ok=true;
    c.action=XIAOZHI_STOP;xiaozhi_command(&p,&c);assert(!s_start_wanted);
}
static void test_failures_and_handover(void){
    task_mode=0;reply="{\"websocket\":{\"url\":\"wss://example.test/\"}}";
    for(int i=0;i<7;++i){forget_endpoint();fail_allocation=i;start();assert(xiaozhi_service_process(ESP_OK)==ESP_ERR_NO_MEM);fail_allocation=-1;idle_cleanup();}
    /* Cached endpoints must never retry a transport with partially allocated queues. */
    for(int i=0;i<5;++i){s_cached=true;strcpy(s_cached_url,"wss://example.test/");fail_allocation=i;
        unsigned calls=ws_calls;start();assert(xiaozhi_service_process(ESP_OK)==ESP_ERR_NO_MEM);
        assert(ws_calls==calls);fail_allocation=-1;idle_cleanup();}
    task_mode=3;start();assert(xiaozhi_service_process(ESP_OK)==ESP_FAIL);idle_cleanup();
    task_mode=0;send_handover=true;start();assert(xiaozhi_service_process(ESP_OK)==ESP_OK);assert(!s_busy && !s_codec_task);idle_cleanup();send_handover=false;
    uint32_t music=music_audio_begin();start();assert(xiaozhi_service_process(ESP_OK)==XIAOZHI_ERR_AUDIO_BUSY);
    music_audio_status_t status;music_audio_get_status(&status);assert(status.active && status.generation==music);
    music_audio_stop();idle_cleanup();
    fail_allocation=0;voice_capture_begin();voice_capture_owner_maintain();
    voice_capture_status_t capture;voice_capture_get_status(&capture);assert(capture.result==ESP_ERR_NO_MEM && !capture.active);
    fail_allocation=-1;
    uint32_t old=voice_capture_begin();voice_capture_owner_maintain();uint32_t fresh=voice_capture_begin();
    int16_t pcm[256]={0},out[256];voice_capture_owner_push(old,pcm,256);assert(!voice_capture_read(fresh,out,256));
    voice_capture_owner_push(fresh,pcm,256);assert(voice_capture_read(fresh,out,256)==256);voice_capture_stop();idle_cleanup();
}

#define taskENTER_CRITICAL portENTER_CRITICAL
#define taskEXIT_CRITICAL portEXIT_CRITICAL
static portMUX_TYPE s_status_lock=portMUX_INITIALIZER_UNLOCKED;
static bool s_started=true,s_request_pending;
static unsigned s_request_mode;
typedef unsigned network_validation_mode_t;
#define NETWORK_VALIDATION_MODE_NORMAL 0U
#define NETWORK_VALIDATION_MODE_SPOTIFY_REMOTE 7U
typedef struct {bool online;} connectivity_diagnostic_status_t;
static bool station_online=true;
static void connectivity_diagnostic_get_status(connectivity_diagnostic_status_t *c){c->online=station_online;}
static struct {bool busy,time_synced;int dns_result,ntp_result,https_result;unsigned last_duration_ms,completed_checks;} s_status;
#include "xiaozhi_executor_under_test.inc"
static void test_executor_handover(void){
    s_xiaozhi_active=true;s_status.busy=true;s_busy=true;s_yield=false;
    assert(network_validation_service_request_spotify_remote()==ESP_ERR_TIMEOUT && !s_yield);
    assert(network_validation_service_request_spotify_interactive()==ESP_OK && s_yield);
    assert(s_request_pending && s_request_mode==NETWORK_VALIDATION_MODE_SPOTIFY_REMOTE);
    assert(network_validation_service_request_spotify_interactive()==ESP_ERR_TIMEOUT);
    publish_results(ESP_OK,ESP_OK,ESP_OK,100);assert(s_status.busy);
    network_validation_mode_t mode;
    assert(take_requested_work(&mode) && mode==NETWORK_VALIDATION_MODE_SPOTIFY_REMOTE);
    s_xiaozhi_active=false;s_busy=false;s_yield=false;
    publish_results(ESP_OK,ESP_OK,ESP_OK,100);assert(!s_status.busy);
    s_request_mode=99U;
    assert(!take_requested_work(&mode) && mode==NETWORK_VALIDATION_MODE_NORMAL);
    station_online=false;assert(network_validation_service_request_spotify_interactive()==ESP_ERR_INVALID_STATE);
}
#define DEVICE_CONTROL_MIC_DISCARD_READS 16U
static bool s_microphone_open=true;
static int s_active_sound=-1;
static void *s_microphone=(void*)1;
static esp_err_t owner_read_result;
static size_t owner_read_bytes;
static esp_err_t audio_capture_io_read(void *codec,void *pcm,size_t capacity,size_t *received){
    (void)codec;assert(owner_read_bytes<=capacity);*received=owner_read_bytes;
    memset(pcm,0,owner_read_bytes);return owner_read_result;
}
#include "capture_microphone_under_test.inc"
static void test_owner_transient_reads(void){
    voice_capture_begin();voice_capture_owner_maintain();owner_read_result=ESP_ERR_TIMEOUT;owner_read_bytes=0;
    for(unsigned i=0;i<20;++i){assert(capture_microphone_chunk());clock_us+=20000;}
    voice_capture_status_t status;voice_capture_get_status(&status);assert(status.active && !status.captured_samples);
    owner_read_result=ESP_OK;owner_read_bytes=8;
    for(unsigned i=0;i<17;++i)assert(capture_microphone_chunk());
    voice_capture_get_status(&status);assert(status.active && status.captured_samples==4 && status.buffered_samples==4);
    voice_capture_stop();voice_capture_owner_maintain();
    voice_capture_begin();voice_capture_owner_maintain();owner_read_result=ESP_ERR_TIMEOUT;owner_read_bytes=0;
    assert(capture_microphone_chunk());clock_us+=1000001;
    assert(!capture_microphone_chunk());voice_capture_get_status(&status);assert(!status.active && status.result==ESP_ERR_TIMEOUT);
    voice_capture_owner_maintain();idle_cleanup();
}
static void test_listen_requires_microphone_samples(void){
    s_http=heap_caps_calloc(1,sizeof(*s_http),MALLOC_CAP_INTERNAL);s_ws=&websocket;websocket.connected=true;
    keep_listening=true;s_stop=s_yield=false;s_capture_enabled=false;
    s_capture_generation=voice_capture_begin();voice_capture_owner_maintain();
    assert(begin_listening("s",true)==XIAOZHI_ERR_MICROPHONE && !s_capture_enabled);
    int16_t pcm[4]={0};voice_capture_owner_push(s_capture_generation,pcm,4);
    assert(begin_listening("s",true)==ESP_OK && s_capture_enabled && s_live.state==XIAOZHI_STATE_LISTENING);
    pause_listening();keep_listening=false;s_ws=NULL;heap_caps_free(s_http);s_http=NULL;idle_cleanup();
}
static int script_mode;
static bool script_sent, script_stopped;
static int64_t script_start, script_audio;
static void session_script(void)
{
    int16_t pcm[4] = {0};
    voice_capture_owner_push(s_capture_generation, pcm, 4);
    if (script_mode >= 5) {
        if (!script_sent && s_live.state == XIAOZHI_STATE_LISTENING) {
            if (script_mode == 7) {
                const char *begin = "{\"type\":\"tts\",\"state\":\"start\"}";
                const char *end = "{\"type\":\"tts\",\"state\":\"stop\"}";
                inbox_push(begin,strlen(begin)); inbox_push(end,strlen(end));
                assert(music_audio_push(s_music_generation,pcm,4)==4);
            } else ws_event(NULL,NULL,script_mode == 5 ? WEBSOCKET_EVENT_CLOSED : WEBSOCKET_EVENT_DISCONNECTED,NULL);
            script_sent=true;
        }
        return;
    }
    if (script_mode <= 2) s_voice_activity = clock_us; /* persistent background noise */
    if (script_mode >= 2 && !script_sent && s_live.state == XIAOZHI_STATE_LISTENING) {
        const char *start_msg = "{\"type\":\"tts\",\"state\":\"start\"}";
        inbox_push(start_msg, strlen(start_msg));
        script_sent = true; script_start = clock_us; script_audio = clock_us;
        if (script_mode == 2) {
            const char *stop_msg = "{\"type\":\"tts\",\"state\":\"stop\"}";
            inbox_push(stop_msg, strlen(stop_msg)); script_stopped = true;
        }
    }
    if (script_mode == 3 && script_sent && !script_stopped) {
        if (clock_us - script_start >= 50000000LL) {
            const char *stop_msg = "{\"type\":\"tts\",\"state\":\"stop\"}";
            inbox_push(stop_msg, strlen(stop_msg)); script_stopped = true;
        } else if (clock_us - script_audio >= 1000000LL) {
            const char packet = 1;
            esp_websocket_event_data_t e = {.op_code=2,.data_ptr=&packet,.data_len=1,.payload_len=1,.fin=true};
            ws_event(NULL,NULL,WEBSOCKET_EVENT_DATA,&e);
            assert(!s_control_overflow);
            ring_clear(&s_down); /* fake decoder consumed this frame */
            script_audio = clock_us;
        }
    }
}
static void test_real_session_deadlines(void)
{
    reply = "{\"websocket\":{\"url\":\"wss://example.test/\"}}";
    keep_listening = true; task_mode = 0; session_hook = session_script;
    for (int mode = 1; mode <= 7; ++mode) {
        forget_endpoint(); script_mode=mode; script_sent=script_stopped=false;
        int64_t before = clock_us;
        unsigned connections = ws_calls;
        start(); esp_err_t result = xiaozhi_service_process(ESP_OK);
        if (mode == 1) assert(result == XIAOZHI_ERR_NO_SPEECH && clock_us-before >= 30000000LL);
        if (mode == 2) assert(result == ESP_OK && clock_us-before >= 30000000LL && s_live.state == XIAOZHI_STATE_IDLE);
        if (mode == 3) assert(result == ESP_OK && clock_us-before >= 50000000LL && clock_us-before < 60000000LL);
        if (mode == 4) assert(result == XIAOZHI_ERR_RESPONSE_TIMEOUT && clock_us-before >= SERVER_SILENCE_US);
        if (mode == 5) assert(result == ESP_OK);
        if (mode == 6) assert(result == XIAOZHI_ERR_SERVER_CLOSED);
        if (mode == 7) assert(result == XIAOZHI_ERR_PLAYBACK_TIMEOUT && clock_us-before >= DRAIN_LIMIT_US);
        assert(!s_codec_task && !s_busy && ws_calls == connections + 1U); idle_cleanup();
    }
    session_hook=NULL; omit_hello=true; forget_endpoint();
    start(); assert(xiaozhi_service_process(ESP_OK) == XIAOZHI_ERR_CONNECT_TIMEOUT);
    idle_cleanup(); omit_hello=false; keep_listening=false;
    /* A cached endpoint missing hello gets one fresh retry, before greeting. */
    s_cached=true; strcpy(s_cached_url,"wss://example.test/"); omit_hello_once=true;
    unsigned connections=ws_calls, checks=http_calls;
    start(); assert(xiaozhi_service_process(ESP_OK)==ESP_OK);
    assert(ws_calls==connections+2U && http_calls==checks+1U && s_greeted);
    idle_cleanup();
}

int main(void){test_activation();test_codec_quarantine();test_control_and_fragments();test_admission_and_start_race();test_failures_and_handover();test_executor_handover();test_owner_transient_reads();test_listen_requires_microphone_samples();test_real_session_deadlines();
    puts("PASS Xiaozhi activation/leases/RX and actual session deadlines: noisy listen, post-response silence, 50s audio, stalled response and missing hello");}
