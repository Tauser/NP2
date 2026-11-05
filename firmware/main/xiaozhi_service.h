#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "xiaozhi_protocol.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Xiaozhi voice assistant (xiaozhi.me). One session at a time, executed by
 * the single HTTPS worker: while it is open, the WebSocket holds the
 * product's only TLS slot and other providers wait (ADR-116). */
typedef enum {
    XIAOZHI_STATE_IDLE = 0,
    XIAOZHI_STATE_WAITING,     /* start requested, executor still busy */
    XIAOZHI_STATE_CONNECTING,
    XIAOZHI_STATE_ACTIVATION,  /* code must be entered at xiaozhi.me */
    XIAOZHI_STATE_LISTENING,
    XIAOZHI_STATE_THINKING,
    XIAOZHI_STATE_SPEAKING,
    XIAOZHI_STATE_ERROR,
    XIAOZHI_STATE_STOPPING,
} xiaozhi_state_t;

#define XIAOZHI_ERR_ACTIVATION_TIMEOUT (ESP_ERR_INVALID_RESPONSE + 220)
#define XIAOZHI_ERR_SERVER_CLOSED      (ESP_ERR_INVALID_RESPONSE + 221)
#define XIAOZHI_ERR_IDLE               (ESP_ERR_INVALID_RESPONSE + 222)
#define XIAOZHI_ERR_AUDIO_BUSY         (ESP_ERR_INVALID_RESPONSE + 223)
#define XIAOZHI_ERR_CONTROL_OVERFLOW   (ESP_ERR_INVALID_RESPONSE + 224)
#define XIAOZHI_ERR_CODEC_STUCK        (ESP_ERR_INVALID_RESPONSE + 225)
#define XIAOZHI_ERR_MICROPHONE         (ESP_ERR_INVALID_RESPONSE + 226)
#define XIAOZHI_ERR_NO_SPEECH          (ESP_ERR_INVALID_RESPONSE + 227)
#define XIAOZHI_ERR_CONNECT_TIMEOUT    (ESP_ERR_INVALID_RESPONSE + 228)
#define XIAOZHI_ERR_RESPONSE_TIMEOUT   (ESP_ERR_INVALID_RESPONSE + 229)
#define XIAOZHI_ERR_PLAYBACK_TIMEOUT   (ESP_ERR_INVALID_RESPONSE + 230)

typedef struct {
    xiaozhi_state_t state;
    bool busy;
    uint32_t revision;
    esp_err_t result;
    char activation_code[XIAOZHI_CODE_BYTES];
    char user_text[XIAOZHI_TEXT_BYTES], assistant_text[XIAOZHI_TEXT_BYTES];
    char emotion[XIAOZHI_EMOTION_BYTES];
} xiaozhi_projection_t;

typedef enum { XIAOZHI_START = 0, XIAOZHI_STOP, XIAOZHI_INTERRUPT } xiaozhi_action_t;
typedef struct { xiaozhi_action_t action; } xiaozhi_command_t;

/* app_loop owns the projection and invokes both reducers; UI posts intents. */
void xiaozhi_tick(xiaozhi_projection_t *, bool online, bool trusted);
void xiaozhi_command(xiaozhi_projection_t *, const xiaozhi_command_t *);
/* Sole HTTPS worker only. Runs the whole session and always completes it. */
esp_err_t xiaozhi_service_process(esp_err_t prerequisite);
/* Executor can request cooperative handover to one accepted interactive job. */
void xiaozhi_service_yield_network(void);
#ifdef __cplusplus
}
#endif
