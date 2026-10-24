#include "spotify_playback_runtime.h"
#include "SpotifyAudioAllocator.h"
#include "CSpotContext.h"
#include "SpircHandler.h"
#include "TrackPlayer.h"
#include "spotify_vorbis_guard.h"
#include "spotify_pcm_sink.h"
#include "music_audio_service.h"
#include "spotify_http_bridge.h"
#include "spotify_lab_diagnostics.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <stdexcept>
extern "C" {
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
void np_spotify_task_fault(void);
int np_spotify_task_failed(void);
}
namespace {
constexpr unsigned Internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
struct Runtime {
    std::shared_ptr<cspot::Context> ctx;
    std::shared_ptr<cspot::SpircHandler> handler;
    std::mutex pcmMutex;
    spotify_pcm_sink_t sink{};
    std::atomic<bool> disconnected{false};
    bool accepting=false,requestedPause=false,draining=false;
    uint32_t consumed=0,starvedPolls=0,decoderStack=0,reportedHttp=0;
    uint64_t overallConsumed=0;
    int64_t lastProgress=0,pausedSince=0;
    static size_t push(void *,uint32_t generation,const int16_t *pcm,size_t count) {
        return music_audio_push(generation,pcm,count);
    }
    void reset() {
        spotify_pcm_sink_reset(&sink,music_audio_begin());
        consumed=0;draining=false;accepting=true;music_audio_pause(true);
    }
    void event(std::unique_ptr<cspot::SpircHandler::Event> event) {
        std::lock_guard<std::mutex> lock(pcmMutex);
        using Type=cspot::SpircHandler::EventType;
        switch(event->eventType){
        case Type::PLAYBACK_START: case Type::FLUSH: case Type::SEEK: reset();break;
        case Type::PLAY_PAUSE:
            requestedPause=std::get<bool>(event->data);
            if(requestedPause) music_audio_pause(true);
            break;
        case Type::VOLUME: {
            const int volume=std::get<int>(event->data);
            if(volume<0 || volume>65535){np_spotify_task_fault();break;}
            music_audio_set_gain((uint8_t)((unsigned)volume*100U/65535U));break;
        }
        case Type::DISC:
            accepting=false;music_audio_stop();disconnected=true;break;
        case Type::DEPLETED: draining=true;break;
        default: break;
        }
    }
    size_t pcm(uint8_t *data,size_t bytes,std::string_view id) {
        std::lock_guard<std::mutex> lock(pcmMutex);
        if(!accepting || disconnected || np_spotify_task_failed()) return 0;
        decoderStack=(uint32_t)uxTaskGetStackHighWaterMark(nullptr);
        const size_t used=spotify_pcm_sink_write(&sink,id.data(),id.size(),data,bytes,push,nullptr);
        if(sink.failed) np_spotify_task_fault();
        return used;
    }
    void tick() {
        char id[SPOTIFY_PCM_ID_BYTES]{};
        bool reached=false;
        {
            std::lock_guard<std::mutex> lock(pcmMutex);
            music_audio_status_t audio{};music_audio_get_status(&audio);
            if(accepting && audio.result!=ESP_OK){np_spotify_task_fault();return;}
            if(accepting && audio.generation==sink.generation){
                if(audio.buffered_samples>sink.queued){np_spotify_task_fault();return;}
                const uint32_t nowConsumed=sink.queued-audio.buffered_samples;
                if(nowConsumed>consumed){
                    lastProgress=esp_timer_get_time();overallConsumed+=nowConsumed-consumed;
                }
                consumed=nowConsumed;
                const bool prefilled=audio.buffered_samples>=22050 || draining || sink.marker_count==SPOTIFY_PCM_MARKERS;
                if(!requestedPause && audio.paused && prefilled) music_audio_pause(false);
                if(requestedPause && !pausedSince) pausedSince=esp_timer_get_time();
                if(!requestedPause) pausedSince=0;
                if(!requestedPause && !audio.paused && !audio.buffered_samples && !draining) ++starvedPolls;
                reached=spotify_pcm_sink_reached(&sink,consumed,id);
                if(draining && !audio.buffered_samples && !sink.pending_ack){
                    accepting=false;music_audio_stop();
                }
            }
        }
        /* Never enter Spirc/control/network with the PCM lock held. The
         * decoder callback only converts/copies and cannot deadlock stop. */
        if(reached) handler->notifyAudioReachedPlayback(id);
    }
    void report() {
        spotify_http_diagnostics_t http{};spotify_http_bridge_get_diagnostics(&http);
        if(http.count!=reportedHttp){
            reportedHttp=http.count;
            ESP_LOGI("np2_spotify","http count=%u kind=%u status=%u result=%ld bytes=%u ms=%u before=%u min_internal=%u network_stack=%u",
                (unsigned)http.count,(unsigned)http.kind,(unsigned)http.status,(long)http.result,
                (unsigned)http.bytes,(unsigned)http.duration_ms,(unsigned)http.free_before,
                (unsigned)http.minimum_internal,(unsigned)http.network_stack);
        }
        uint32_t queuedCopy,consumedCopy,starvedCopy,decoderStackCopy;
        music_audio_status_t audio{};
        {
            std::lock_guard<std::mutex> lock(pcmMutex);
            music_audio_get_status(&audio);queuedCopy=sink.queued;consumedCopy=consumed;
            starvedCopy=starvedPolls;decoderStackCopy=decoderStack;
        }
        ESP_LOGI("np2_spotify","pcm queued=%u consumed=%u buffered=%u paused=%u starved_polls=%u internal=%u largest=%u decoder_stack=%u worker_stack=%u integrity=%u",
            (unsigned)queuedCopy,(unsigned)consumedCopy,(unsigned)audio.buffered_samples,(unsigned)audio.paused,
            (unsigned)starvedCopy,(unsigned)heap_caps_get_free_size(Internal),
            (unsigned)heap_caps_get_largest_free_block(Internal),(unsigned)decoderStackCopy,
            (unsigned)uxTaskGetStackHighWaterMark(nullptr),(unsigned)heap_caps_check_integrity_all(false));
    }
};
/* One bounded runtime per boot. Keep foreign worker objects alive after stop;
 * their task-entry epilogues can still be returning/deleting IDF stacks. */
std::shared_ptr<Runtime> runtime;
void wipe(std::string& text){volatile char *p=text.data();for(size_t i=0;i<text.size();++i)p[i]=0;text.clear();}
}
bool spotify_playback_run(std::shared_ptr<cspot::LoginBlob>& blob,const char *id,
    const char *secret,void (*authenticated)(void))
{
    if(runtime) throw std::runtime_error("Spotify session already used");
    runtime=std::allocate_shared<Runtime>(SpotifyAudioAllocator<Runtime>{});
    auto& r=*runtime;
    bool played=false;
    unsigned stage=10;
    try {
        r.ctx=cspot::Context::createFromBlob(blob);
        r.ctx->config.clientId=id;r.ctx->config.clientSecret=secret;
        r.ctx->config.audioFormat=AudioFormat_OGG_VORBIS_160;
        r.ctx->config.volume=13107;music_audio_set_gain(20);
        for(unsigned attempt=0;;++attempt){
            try {r.ctx->session->connectWithRandomAp();break;}
            catch(const std::exception& error){
                spotify_lab_failure(stage,error);
                if(attempt>=2 || std::strcmp(error.what(),"Spotify socket failed")) throw;
                r.ctx->session->close();
                r.ctx->session=std::make_shared<cspot::MercurySession>(r.ctx->timeProvider);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        stage=11;
        auto reusable=r.ctx->session->authenticate(blob);
        if(reusable.empty()) throw std::runtime_error("Spotify account rejected");
        stage=12;
        volatile uint8_t *token=reusable.data();for(size_t i=0;i<reusable.size();++i)token[i]=0;
        reusable.clear();reusable.shrink_to_fit();
        r.ctx->session->releaseHandshakeState();
        volatile uint8_t *auth=r.ctx->config.authData.data();for(size_t i=0;i<r.ctx->config.authData.size();++i)auth[i]=0;
        r.ctx->config.authData.clear();r.ctx->config.authData.shrink_to_fit();
        auth=blob->authData.data();for(size_t i=0;i<blob->authData.size();++i)auth[i]=0;
        wipe(blob->username);blob.reset();
        r.handler=std::make_shared<cspot::SpircHandler>(r.ctx);
        r.handler->setEventHandler([&r](auto event){r.event(std::move(event));});
        r.handler->getTrackPlayer()->setDataCallback([&r](uint8_t *pcm,size_t size,std::string_view track){return r.pcm(pcm,size,track);});
        r.ctx->session->setConnectedHandler(nullptr); // Explicit one-session subscription after country is immutable.
        if(!r.ctx->session->start())
            throw std::runtime_error("Spotify workers unavailable");
        const int64_t countryDeadline=esp_timer_get_time()+5000000LL;
        while(r.ctx->session->getCountryCode().size()!=2 && !np_spotify_task_failed() &&
              esp_timer_get_time()<countryDeadline) r.ctx->session->handlePacket();
        r.ctx->config.countryCode=r.ctx->session->getCountryCode();
        if(r.ctx->config.countryCode.size()!=2 || !r.handler->getTrackQueue()->start())
            throw std::runtime_error("Spotify country unavailable");
        r.handler->subscribeToMercury();
        stage=13;
        if(authenticated) authenticated();
        const int64_t started=esp_timer_get_time();int64_t reported=0;
        r.lastProgress=started;
        while(!r.disconnected && !np_spotify_task_failed()){
            r.ctx->session->handlePacket();r.tick();
            const int64_t now=esp_timer_get_time();
            if(now-reported>=5000000LL){r.report();reported=now;}
            {
                std::lock_guard<std::mutex> lock(r.pcmMutex);
                if(r.overallConsumed>=44100) played=true;
                if((!played && now-started>300000000LL) ||
                    (r.pausedSince && now-r.pausedSince>600000000LL)) break;
            }
        }
    }catch(const std::exception& error){spotify_lab_failure(stage,error);np_spotify_task_fault();}
    catch(...){np_spotify_task_fault();}
    music_audio_stop();
    if(r.handler) r.handler->disconnect();
    else if(r.ctx) r.ctx->session->disconnect();
    r.report();
    if(r.ctx){wipe(r.ctx->config.clientId);wipe(r.ctx->config.clientSecret);wipe(r.ctx->config.username);}
    return played && !np_spotify_task_failed();
}
