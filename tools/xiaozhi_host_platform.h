#pragma once
#include "platform.h"
#include "google_agenda_codec.h"
#include "voice_resampler.h"
#include "voice_capture.h"
#include "music_audio_service.h"
#define ESP_ERR_NOT_SUPPORTED 0x106
#define BIT0 1U
#define BIT1 2U
#define pdPASS 1
#define pdFALSE 0
#define tskNO_AFFINITY -1
#define ESP_LOGI(tag,...) do { (void)(tag); if(0)printf(__VA_ARGS__); } while(0)
#define ESP_LOGW ESP_LOGI
#define ESP_LOGE ESP_LOGI
typedef unsigned char StackType_t;
typedef struct fake_task *TaskHandle_t;
typedef enum { eRunning, eSuspended } eTaskState;
typedef struct fake_events *EventGroupHandle_t;
typedef unsigned EventBits_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t,unsigned);
EventBits_t xEventGroupClearBits(EventGroupHandle_t,unsigned);
EventBits_t xEventGroupGetBits(EventGroupHandle_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t,unsigned,int,int,unsigned);
int xTaskCreatePinnedToCoreWithCaps(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *,int,unsigned);
void vTaskDeleteWithCaps(TaskHandle_t);
void vTaskSuspend(TaskHandle_t);
eTaskState eTaskGetState(TaskHandle_t);
typedef const char *esp_event_base_t;
typedef struct { char version[32]; } esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
int esp_app_get_elf_sha256(char *,size_t);
esp_err_t esp_flash_get_size(void *,uint32_t *);
#define WIFI_IF_STA 0
esp_err_t esp_wifi_get_mac(int,uint8_t *);
int mbedtls_sha256(const unsigned char *,size_t,unsigned char *,int);
const char *esp_err_to_name(esp_err_t);
esp_err_t app_state_request_refresh(void);
esp_err_t network_validation_service_request_xiaozhi(void);
void *heap_caps_realloc(void *,size_t,unsigned);
typedef struct fake_ws *esp_websocket_client_handle_t;
typedef struct {
    const char *uri,*headers,*task_name;
    esp_err_t (*crt_bundle_attach)(void *);
    int buffer_size,task_stack,task_prio,network_timeout_ms,ping_interval_sec,pingpong_timeout_sec;
    bool disable_auto_reconnect;
} esp_websocket_client_config_t;
typedef struct {
    int close_status_code,op_code,data_len,payload_offset,payload_len;
    bool fin;
    const char *data_ptr;
    struct {
        int esp_tls_last_esp_err, esp_tls_stack_err, esp_tls_cert_verify_flags;
        int error_type, esp_ws_handshake_status_code, esp_transport_sock_errno;
    } error_handle;
} esp_websocket_event_data_t;
#define WEBSOCKET_EVENT_ANY -1
#define WEBSOCKET_EVENT_CONNECTED 1
#define WEBSOCKET_EVENT_DISCONNECTED 2
#define WEBSOCKET_EVENT_CLOSED 3
#define WEBSOCKET_EVENT_ERROR 4
#define WEBSOCKET_EVENT_DATA 5
esp_websocket_client_handle_t esp_websocket_client_init(const esp_websocket_client_config_t *);
esp_err_t esp_websocket_register_events(esp_websocket_client_handle_t,int,void (*)(void *,esp_event_base_t,int32_t,void *),void *);
esp_err_t esp_websocket_client_start(esp_websocket_client_handle_t);
esp_err_t esp_websocket_client_stop(esp_websocket_client_handle_t);
esp_err_t esp_websocket_client_destroy(esp_websocket_client_handle_t);
bool esp_websocket_client_is_connected(esp_websocket_client_handle_t);
int esp_websocket_client_send_text(esp_websocket_client_handle_t,const char *,int,unsigned);
int esp_websocket_client_send_bin(esp_websocket_client_handle_t,const char *,int,unsigned);
typedef int esp_audio_err_t;
#define ESP_AUDIO_ERR_OK 0
#define ESP_OPUS_BITRATE_AUTO 0
#define ESP_OPUS_ENC_FRAME_DURATION_60_MS 60
typedef struct { int sample_rate,channel,bitrate,frame_duration,complexity; bool enable_vbr; } esp_opus_enc_config_t;
typedef struct { unsigned sample_rate,channel; } esp_opus_dec_cfg_t;
#define ESP_OPUS_ENC_CONFIG_DEFAULT() ((esp_opus_enc_config_t){0})
#define ESP_OPUS_DEC_CONFIG_DEFAULT() ((esp_opus_dec_cfg_t){0})
typedef struct { uint8_t *buffer; unsigned len; } esp_audio_enc_in_frame_t;
typedef struct { uint8_t *buffer; unsigned len,encoded_bytes; } esp_audio_enc_out_frame_t;
typedef struct { uint8_t *buffer; unsigned len; } esp_audio_dec_in_raw_t;
typedef struct { uint8_t *buffer; unsigned len,decoded_size; } esp_audio_dec_out_frame_t;
typedef struct { int unused; } esp_audio_dec_info_t;
esp_audio_err_t esp_opus_enc_open(void *,unsigned,void **);
esp_audio_err_t esp_opus_dec_open(void *,unsigned,void **);
esp_audio_err_t esp_opus_enc_process(void *,esp_audio_enc_in_frame_t *,esp_audio_enc_out_frame_t *);
esp_audio_err_t esp_opus_dec_decode(void *,esp_audio_dec_in_raw_t *,esp_audio_dec_out_frame_t *,esp_audio_dec_info_t *);
void esp_opus_enc_close(void *);
esp_audio_err_t esp_opus_dec_close(void *);
esp_audio_err_t esp_opus_dec_reset(void *);
