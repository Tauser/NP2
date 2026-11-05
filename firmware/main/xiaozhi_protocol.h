#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Pure Xiaozhi device protocol (OTA check + WebSocket v1 JSON). No I/O and
 * no allocation: callers own every buffer. Tokens never reach projections. */
#ifdef __cplusplus
extern "C" {
#endif

#define XIAOZHI_DEFAULT_OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define XIAOZHI_DEFAULT_WS_URL  "wss://api.tenclass.net/xiaozhi/v1/"
#define XIAOZHI_UPLINK_SAMPLE_RATE 16000U
#define XIAOZHI_FRAME_MS 60U
#define XIAOZHI_CODE_BYTES 16U
#define XIAOZHI_URL_BYTES 160U
#define XIAOZHI_TOKEN_BYTES 192U
#define XIAOZHI_CHALLENGE_BYTES 96U
#define XIAOZHI_MESSAGE_BYTES 160U
#define XIAOZHI_SESSION_BYTES 64U
#define XIAOZHI_TEXT_BYTES 320U
#define XIAOZHI_EMOTION_BYTES 24U

typedef struct {
    bool activation, websocket;
    uint32_t activation_timeout_ms;
    uint8_t ws_version;
    char code[XIAOZHI_CODE_BYTES], message[XIAOZHI_MESSAGE_BYTES];
    char challenge[XIAOZHI_CHALLENGE_BYTES];
    char ws_url[XIAOZHI_URL_BYTES], ws_token[XIAOZHI_TOKEN_BYTES];
} xiaozhi_ota_result_t;

typedef enum {
    XIAOZHI_MSG_UNKNOWN = 0,
    XIAOZHI_MSG_HELLO,
    XIAOZHI_MSG_STT,
    XIAOZHI_MSG_LLM,
    XIAOZHI_MSG_TTS_START,
    XIAOZHI_MSG_TTS_SENTENCE,
    XIAOZHI_MSG_TTS_STOP,
    XIAOZHI_MSG_SYSTEM,
    XIAOZHI_MSG_ALERT,
    XIAOZHI_MSG_MCP,
} xiaozhi_message_type_t;

typedef struct {
    xiaozhi_message_type_t type;
    uint32_t sample_rate, frame_duration_ms;
    char session_id[XIAOZHI_SESSION_BYTES];
    char text[XIAOZHI_TEXT_BYTES], emotion[XIAOZHI_EMOTION_BYTES];
} xiaozhi_message_t;

typedef enum { XIAOZHI_LISTEN_AUTO = 0, XIAOZHI_LISTEN_MANUAL } xiaozhi_listen_mode_t;

/* Identity is derived, never persisted: lowercase colon MAC and a v4-shaped
 * UUID from SHA-256 over the MAC, so the Client-Id is stable per board. */
bool xiaozhi_format_mac(const uint8_t mac[6], char out[18]);
bool xiaozhi_client_id(const uint8_t digest[32], char out[37]);

/* System info body for the OTA check. Only public build/board identity. */
bool xiaozhi_build_system_info(char *out, size_t size, const char *mac, const char *uuid,
                               const char *app_version, const char *elf_sha256,
                               uint32_t flash_bytes, uint32_t minimum_free_heap);
/* `doc` is opaque scratch of at least xiaozhi_json_scratch_bytes(). */
size_t xiaozhi_json_scratch_bytes(void);
bool xiaozhi_parse_ota(void *doc, const char *body, size_t length, xiaozhi_ota_result_t *out);
bool xiaozhi_parse_message(void *doc, const char *body, size_t length, xiaozhi_message_t *out);

bool xiaozhi_build_hello(char *out, size_t size);
bool xiaozhi_build_listen_start(char *out, size_t size, const char *session_id, xiaozhi_listen_mode_t mode);
bool xiaozhi_build_listen_stop(char *out, size_t size, const char *session_id);
bool xiaozhi_build_abort(char *out, size_t size, const char *session_id);

/* Copies at most size-1 bytes without splitting a UTF-8 sequence. */
void xiaozhi_copy_utf8(char *out, size_t size, const char *text);

#ifdef __cplusplus
}
#endif
