#include "spotify_pairing_service.h"
#include "spotify_pairing_policy.h"
#include "spotify_response_codec.h"
#include "spotify_exchange.h"
#include "spotify_lab_diagnostics.h"
#include "BellLogger.h"
#include "LoginBlob.h"
#include "Session.h"
#include "ShannonConnection.h"
#include <atomic>
#include <map>
#include <memory>
#include <stdexcept>
#include <cstring>
#ifdef NP2_SPOTIFY_PLAYBACK_BENCH
#include "spotify_playback_runtime.h"
#include "spotify_decoder_fixture.h"
#endif
extern "C" {
#include "google_agenda_codec.h"
#include "spotify_http_bridge.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "mdns.h"
}
namespace {
constexpr unsigned Internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
#ifdef NP2_SPOTIFY_PLAYBACK_BENCH
constexpr int64_t Window = 300LL * 1000000LL;
#else
constexpr int64_t Window = 180LL * 1000000LL;
#endif
enum Phase { Starting=1, AppVerified, ServerVerified, Discoverable, Received, Authenticated, Expired, Failed,
             ConnectActive, PlaybackStopped };
std::atomic<bool> claimed{false}, paired{false}, fault{false};
std::atomic<int64_t> until{0};
spotify_app_credentials_t credentials{}; /* Private, internal SRAM; never projected. */
httpd_handle_t server = nullptr;
bool advertised = false;
unsigned pairAttempts = 0;
std::shared_ptr<cspot::LoginBlob> blob;
class QuietLogger final : public bell::AbstractLogger {
    void debug(std::string,int,std::string,const char*,...) override {}
    void info(std::string,int,std::string,const char*,...) override {}
    void error(std::string,int,std::string,const char*,...) override {}
};
QuietLogger quietLogger;
class VerifiedSession final : public cspot::Session {
public: bool triggerTimeout() override { return true; }
};
void scrub(void *p,size_t n) { volatile uint8_t *v=static_cast<volatile uint8_t*>(p);while(n--)*v++=0; }
struct PrivateBuffer {
    void *data; size_t size;
    explicit PrivateBuffer(size_t n):data(heap_caps_calloc(1,n,Internal)),size(n) {
        if(!data) throw std::bad_alloc();
    }
    ~PrivateBuffer(){scrub(data,size);heap_caps_free(data);}
};
void phase(Phase value,esp_err_t result=ESP_OK) {
    ESP_LOGI("np2_spotify", "pair phase=%u result=%ld internal=%u largest=%u stack_min_bytes=%u integrity=%u",
        static_cast<unsigned>(value),static_cast<long>(result),
        static_cast<unsigned>(heap_caps_get_free_size(Internal)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(Internal)),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
        static_cast<unsigned>(heap_caps_check_integrity_all(false)));
}
bool windowOpen(){return until.load()>esp_timer_get_time() && !paired.load() && !fault.load();}
esp_err_t handleInfo(httpd_req_t *request) {
    if(!windowOpen()) return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Pairing closed");
    try {
        const auto info=blob->buildZeroconfInfo();
        if(info.size()>2048) throw std::runtime_error("Pairing info invalid");
        httpd_resp_set_type(request,"application/json");
        return httpd_resp_send(request,info.data(),info.size());
    } catch(...) {return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Pairing unavailable");}
}
esp_err_t handlePair(httpd_req_t *request) {
    if(!windowOpen()) return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Pairing closed");
    if(++pairAttempts>6U) return httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Pairing attempt limit");
    if(!request->content_len || request->content_len>SPOTIFY_PAIRING_BODY_MAX)
        return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid pairing");
    try {
        PrivateBuffer body(request->content_len+1);
        char *text=static_cast<char*>(body.data);size_t read=0;
        const int64_t deadline=esp_timer_get_time()+5000000LL;
        while(read<request->content_len){
            if(esp_timer_get_time()>=deadline || !windowOpen()) throw std::runtime_error("Pairing timeout");
            const int n=httpd_req_recv(request,text+read,request->content_len-read);
            if(n<=0) throw std::runtime_error("Pairing read failed");
            read+=n;
        }
        spotify_pairing_form_t form{};
        if(!spotify_pairing_form(text,read,&form)) throw std::runtime_error("Pairing form invalid");
        /* HTTPD serializes handlers. The worker cannot consume blob until the
         * release-store below, and stops HTTPD before authentication. */
        std::map<std::string,std::string> fields{{"userName",form.username},{"blob",form.blob},
            {"clientKey",form.client_key},{"deviceName",form.device_name ? form.device_name : ""}};
        try {blob->loadZeroconfQuery(fields);} catch(...) {
            for(auto& field:fields) scrub(field.second.data(),field.second.size());
            throw;
        }
        for(auto& field:fields) scrub(field.second.data(),field.second.size());
        if(!windowOpen() || blob->authData.empty() || blob->authData.size()>512)
            throw std::runtime_error("Pairing invalid");
        httpd_resp_set_type(request,"application/json");
        const esp_err_t result=httpd_resp_sendstr(request,"{\"status\":101,\"spotifyError\":0,\"statusString\":\"OK\"}");
        if(result==ESP_OK) paired.store(true);
        return result;
    } catch(...) {return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid pairing");}
}
void stopServer(){
    until.store(0);
    if(advertised){(void)mdns_service_remove("_spotify-connect","_tcp");advertised=false;}
    if(server){httpd_stop(server);server=nullptr;}
}
void verifyApp(){
    std::string body="grant_type=client_credentials&client_id="+std::string(credentials.client_id)+
        "&client_secret="+credentials.client_secret;
    spotify_http_request_t request{};request.url="https://accounts.spotify.com/api/token";
    request.post=true;request.content_type="application/x-www-form-urlencoded";
    request.body=reinterpret_cast<const uint8_t*>(body.data());request.body_size=body.size();
    np_spotify_http_reply_t response{};
    const int result=spotify_http_bridge_exchange(&request,&response);
    scrub(body.data(),body.size());
    bool valid=false;
    if(result==ESP_OK){
        try {
            PrivateBuffer workspace(sizeof(spotify_response_workspace_t));
            valid=spotify_response_decode(static_cast<spotify_response_workspace_t*>(workspace.data),SPOTIFY_HTTP_TOKEN,
                reinterpret_cast<const char*>(response.body),response.size);
        }catch(...){valid=false;}
    }
    if(response.body){scrub(response.body,response.capacity);heap_caps_free(response.body);}
    if(!valid) throw std::runtime_error("Spotify app verification failed");
}
void worker(void *){
    esp_err_t result=ESP_FAIL;
    unsigned stage=1;
    try {
        bell::bellGlobalLogger=&quietLogger;
        np_spotify_bind_http(spotify_http_bridge_exchange);
        phase(Starting);verifyApp();phase(AppVerified);stage=2;
#ifdef NP2_SPOTIFY_PLAYBACK_BENCH
        spotify_decoder_proof_t proof{};
        const bool decoderValid=spotify_decoder_fixture(&proof);
        ESP_LOGI("np2_spotify","decoder result=%u bytes=%u mono=%u peak_bytes=%u blocks=%u peak_sample=%u crossings=%u stack=%u integrity=%u",
            (unsigned)!decoderValid,(unsigned)proof.pcm_bytes,(unsigned)proof.mono_samples,
            (unsigned)proof.peak_codec_bytes,(unsigned)proof.peak_blocks,(unsigned)proof.peak_sample,
            (unsigned)proof.zero_crossings,(unsigned)uxTaskGetStackHighWaterMark(nullptr),
            (unsigned)heap_caps_check_integrity_all(false));
        if(!decoderValid) throw std::runtime_error("Decoder gate failed");
#endif
        /* Authenticate the AP without sending account credentials. */
        stage=3;
        for(unsigned attempt=0;;++attempt){
            try {VerifiedSession probe;probe.connectWithRandomAp();probe.close();break;}
            catch(const std::exception& error){
                spotify_lab_failure(stage,error);
                if(attempt>=2 || std::strcmp(error.what(),"Spotify socket failed")) throw;
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        phase(ServerVerified);
        stage=4;
        uint8_t mac[6]{};if(esp_efuse_mac_get_default(mac)!=ESP_OK) throw std::runtime_error("Device identity unavailable");
        char name[40];snprintf(name,sizeof(name),"NovaPanel teste %02X%02X",mac[4],mac[5]);
        blob=std::make_shared<cspot::LoginBlob>(name);
        httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.server_port=8080;config.ctrl_port=32769;
        config.stack_size=8192;config.task_priority=2;config.core_id=1;
        config.task_caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
        config.max_open_sockets=2;config.max_uri_handlers=2;config.max_resp_headers=4;
        config.max_req_hdr_len=1024;config.max_uri_len=128;config.recv_wait_timeout=2;config.send_wait_timeout=2;
        config.lru_purge_enable=true;
        if(httpd_start(&server,&config)!=ESP_OK) throw std::runtime_error("Pairing unavailable");
        httpd_uri_t info{};info.uri="/spotify_info";info.method=HTTP_GET;info.handler=handleInfo;
        httpd_uri_t pair=info;pair.method=HTTP_POST;pair.handler=handlePair;
        until.store(esp_timer_get_time()+Window);
        if(httpd_register_uri_handler(server,&info)!=ESP_OK || httpd_register_uri_handler(server,&pair)!=ESP_OK)
            throw std::runtime_error("Pairing unavailable");
        mdns_txt_item_t txt[]={{"VERSION","1.0"},{"CPath","/spotify_info"},{"Stack","SP"}};
        /* Sonoff LAN owns mdns_init/hostname. Never initialize/free it here. */
        if(mdns_service_add(name,"_spotify-connect","_tcp",8080,txt,3)!=ESP_OK)
            throw std::runtime_error("LAN discovery unavailable");
        advertised=true;
        phase(Discoverable);
        while(windowOpen()) vTaskDelay(pdMS_TO_TICKS(100));
        stopServer();
        if(paired.load()){
            phase(Received);
            stage=5;
#ifdef NP2_SPOTIFY_PLAYBACK_BENCH
            if(!spotify_playback_run(blob,credentials.client_id,credentials.client_secret,
                [](){phase(Authenticated);phase(ConnectActive);}))
                throw std::runtime_error("Playback experiment failed");
            phase(PlaybackStopped);result=ESP_OK;
#else
            VerifiedSession session;session.connectWithRandomAp();
            auto reusable=session.authenticate(blob);session.close();
            if(reusable.empty()) throw std::runtime_error("Spotify account rejected");
            scrub(reusable.data(),reusable.size());phase(Authenticated);result=ESP_OK;
#endif
        }else{phase(Expired,ESP_ERR_TIMEOUT);result=ESP_ERR_TIMEOUT;}
    }catch(const std::exception& error){spotify_lab_failure(stage,error);stopServer();phase(Failed,ESP_FAIL);}
    catch(...){stopServer();phase(Failed,ESP_FAIL);}
    /* The default bench stops after account authentication. The explicit
     * playback variant owns Mercury/TrackPlayer until its session ends.
     * Private app/account material is cleared after either attempt. */
    if(blob){scrub(blob->authData.data(),blob->authData.size());scrub(blob->username.data(),blob->username.size());blob.reset();}
    scrub(&credentials,sizeof(credentials));
    (void)result;vTaskDeleteWithCaps(nullptr);
}
}
extern "C" void np_spotify_task_fault(void){fault.store(true);}
extern "C" int np_spotify_task_failed(void){return fault.load() ? 1 : 0;}
extern "C" esp_err_t spotify_pairing_provision_json(const char *json,size_t size){
    bool expected=false;if(!claimed.compare_exchange_strong(expected,true)) return ESP_ERR_INVALID_STATE;
    try {
        {
            PrivateBuffer workspace(sizeof(google_json_t));
            if(!spotify_app_credentials(json,size,&credentials,workspace.data)){
                claimed.store(false);return ESP_ERR_INVALID_ARG;
            }
        }
        /* Release the internal JSON workspace before the other core can
         * submit HTTPS and apply its SRAM admission floor. */
        TaskHandle_t task=nullptr;
#ifdef NP2_SPOTIFY_PLAYBACK_BENCH
        constexpr unsigned stackBytes=48U*1024U;
#else
        constexpr unsigned stackBytes=12288U;
#endif
        if(xTaskCreatePinnedToCoreWithCaps(worker,"np_spotify_pair",stackBytes,nullptr,2,&task,1,
            MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS){scrub(&credentials,sizeof(credentials));claimed.store(false);return ESP_ERR_NO_MEM;}
        return ESP_OK;
    }catch(...){scrub(&credentials,sizeof(credentials));claimed.store(false);return ESP_ERR_NO_MEM;}
}
