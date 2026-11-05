#include "xiaozhi_protocol.h"
#include "google_agenda_codec.h"
#include <stdio.h>
#include <string.h>

#define JSON_PRIMITIVE 4U

static bool fits(int n, size_t size) { return n > 0 && (size_t)n < size; }
static bool safe_id(const char *text, size_t limit)
{
    const size_t n = strnlen(text, limit);
    return n > 0U && n < limit &&
        strspn(text, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.:") == n;
}
static bool session_ok(const char *id)
{ return id && (id[0] == '\0' || safe_id(id, XIAOZHI_SESSION_BYTES)); }
static bool printable(const char *text)
{
    for (; *text; ++text) if ((unsigned char)*text < 32U || (unsigned char)*text > 126U) return false;
    return true;
}

void xiaozhi_copy_utf8(char *out, size_t size, const char *text)
{
    if (out == NULL || size == 0U) return;
    size_t n = text ? strnlen(text, size - 1U) : 0U;
    /* Back off to the first byte of a sequence that would be split. */
    if (text && text[n] != '\0') while (n > 0U && ((unsigned char)text[n] & 0xC0U) == 0x80U) --n;
    if (n) memcpy(out, text, n);
    out[n] = '\0';
}

bool xiaozhi_format_mac(const uint8_t mac[6], char out[18])
{
    return mac && out && snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]) == 17;
}

bool xiaozhi_client_id(const uint8_t digest[32], char out[37])
{
    if (!digest || !out) return false;
    uint8_t u[16];
    memcpy(u, digest, sizeof(u));
    u[6] = (uint8_t)((u[6] & 0x0FU) | 0x40U);
    u[8] = (uint8_t)((u[8] & 0x3FU) | 0x80U);
    return snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]) == 36;
}

bool xiaozhi_build_system_info(char *out, size_t size, const char *mac, const char *uuid,
                               const char *app_version, const char *elf_sha256,
                               uint32_t flash_bytes, uint32_t minimum_free_heap)
{
    if (!out || !mac || !uuid || !app_version || !elf_sha256 ||
        !safe_id(mac, 18U) || !safe_id(uuid, 37U) || !safe_id(app_version, 32U) || !safe_id(elf_sha256, 65U)) return false;
    /* No partition table, SSID, IP or user data: the server needs only a
     * stable device identity and an application version to issue a code. */
    return fits(snprintf(out, size,
        "{\"version\":2,\"language\":\"pt-BR\",\"flash_size\":%lu,\"minimum_free_heap_size\":\"%lu\","
        "\"mac_address\":\"%s\",\"uuid\":\"%s\",\"chip_model_name\":\"esp32p4\","
        "\"application\":{\"name\":\"novapanel\",\"version\":\"%s\",\"elf_sha256\":\"%s\"},"
        "\"board\":{\"type\":\"novapanel-p4-7b\",\"name\":\"NovaPanel\",\"mac\":\"%s\"}}",
        (unsigned long)flash_bytes, (unsigned long)minimum_free_heap, mac, uuid,
        app_version, elf_sha256, mac), size);
}

size_t xiaozhi_json_scratch_bytes(void) { return sizeof(google_json_t); }

static bool string_field(google_json_t *doc, int object, const char *name, char *out, size_t size)
{
    char wide[640];
    const int token = google_json_field(doc, object, name);
    if (token < 0 || !google_json_string(doc, token, wide, sizeof(wide))) return false;
    xiaozhi_copy_utf8(out, size, wide);
    return true;
}
static bool number_field(google_json_t *doc, int object, const char *name, uint32_t *out)
{
    const int t = google_json_field(doc, object, name);
    if (t < 0 || doc->tokens[t].kind != JSON_PRIMITIVE || doc->tokens[t].end == doc->tokens[t].begin) return false;
    uint64_t n = 0U;
    for (unsigned i = doc->tokens[t].begin; i < doc->tokens[t].end; ++i) {
        const char c = doc->text[i];
        if (c < '0' || c > '9' || (n = n * 10U + (unsigned)(c - '0')) > UINT32_MAX) return false;
    }
    *out = (uint32_t)n;
    return true;
}
static bool object_field(google_json_t *doc, int object, const char *name, int *out)
{
    const int t = google_json_field(doc, object, name);
    if (t < 0 || doc->tokens[t].kind != 1U) return false;
    *out = t;
    return true;
}

bool xiaozhi_parse_ota(void *scratch, const char *body, size_t length, xiaozhi_ota_result_t *out)
{
    google_json_t *doc = scratch;
    if (!doc || !out) return false;
    memset(out, 0, sizeof(*out));
    if (!google_json_parse(doc, body, length)) return false;
    int a = -1, w = -1;
    if (object_field(doc, 0, "activation", &a)) {
        out->activation = true;
        (void)string_field(doc, a, "code", out->code, sizeof(out->code));
        (void)string_field(doc, a, "message", out->message, sizeof(out->message));
        if (string_field(doc, a, "challenge", out->challenge, sizeof(out->challenge)) && !printable(out->challenge))
            out->challenge[0] = '\0';
        (void)number_field(doc, a, "timeout_ms", &out->activation_timeout_ms);
        /* A displayed code is short and numeric on the official server. */
        if (strspn(out->code, "0123456789") != strlen(out->code)) out->code[0] = '\0';
    }
    if (object_field(doc, 0, "websocket", &w)) {
        uint32_t version = 1U;
        out->websocket = string_field(doc, w, "url", out->ws_url, sizeof(out->ws_url)) &&
            strncmp(out->ws_url, "wss://", 6) == 0 && printable(out->ws_url);
        if (string_field(doc, w, "token", out->ws_token, sizeof(out->ws_token)) && !printable(out->ws_token))
            out->ws_token[0] = '\0';
        (void)number_field(doc, w, "version", &version);
        /* Only binary protocol v1 (raw Opus frames) is implemented. */
        out->ws_version = 1U;
        (void)version;
        if (!out->websocket) { out->ws_url[0] = '\0'; out->ws_token[0] = '\0'; }
    }
    return true;
}

bool xiaozhi_parse_message(void *scratch, const char *body, size_t length, xiaozhi_message_t *out)
{
    google_json_t *doc = scratch;
    if (!doc || !out) return false;
    memset(out, 0, sizeof(*out));
    char type[24], state[24];
    if (!google_json_parse(doc, body, length) || !string_field(doc, 0, "type", type, sizeof(type))) return false;
    if (string_field(doc, 0, "session_id", out->session_id, sizeof(out->session_id)) &&
        !safe_id(out->session_id, sizeof(out->session_id))) out->session_id[0] = '\0';
    if (strcmp(type, "hello") == 0) {
        char transport[16];
        int params = -1;
        if (!string_field(doc, 0, "transport", transport, sizeof(transport)) || strcmp(transport, "websocket") != 0)
            return false;
        out->type = XIAOZHI_MSG_HELLO;
        out->sample_rate = 24000U; out->frame_duration_ms = XIAOZHI_FRAME_MS;
        if (object_field(doc, 0, "audio_params", &params)) {
            char format[16];
            uint32_t channels = 1U;
            if (google_json_field(doc, params, "format") >= 0 &&
                (!string_field(doc, params, "format", format, sizeof(format)) || strcmp(format, "opus") != 0)) return false;
            if (google_json_field(doc, params, "channels") >= 0 &&
                (!number_field(doc, params, "channels", &channels) || channels != 1U)) return false;
            (void)number_field(doc, params, "sample_rate", &out->sample_rate);
            (void)number_field(doc, params, "frame_duration", &out->frame_duration_ms);
        }
        if (out->frame_duration_ms != 10U && out->frame_duration_ms != 20U &&
            out->frame_duration_ms != 40U && out->frame_duration_ms != 60U &&
            out->frame_duration_ms != 80U && out->frame_duration_ms != 100U && out->frame_duration_ms != 120U) return false;
        return out->sample_rate == 8000U || out->sample_rate == 12000U || out->sample_rate == 16000U ||
               out->sample_rate == 24000U || out->sample_rate == 48000U;
    }
    if (strcmp(type, "stt") == 0) {
        out->type = XIAOZHI_MSG_STT;
        (void)string_field(doc, 0, "text", out->text, sizeof(out->text));
    } else if (strcmp(type, "llm") == 0) {
        out->type = XIAOZHI_MSG_LLM;
        (void)string_field(doc, 0, "emotion", out->emotion, sizeof(out->emotion));
    } else if (strcmp(type, "tts") == 0 && string_field(doc, 0, "state", state, sizeof(state))) {
        if (strcmp(state, "start") == 0) out->type = XIAOZHI_MSG_TTS_START;
        else if (strcmp(state, "stop") == 0) out->type = XIAOZHI_MSG_TTS_STOP;
        else if (strcmp(state, "sentence_start") == 0) {
            out->type = XIAOZHI_MSG_TTS_SENTENCE;
            (void)string_field(doc, 0, "text", out->text, sizeof(out->text));
        }
    } else if (strcmp(type, "system") == 0) {
        out->type = XIAOZHI_MSG_SYSTEM;
        (void)string_field(doc, 0, "command", out->text, sizeof(out->text));
    } else if (strcmp(type, "alert") == 0) {
        out->type = XIAOZHI_MSG_ALERT;
        (void)string_field(doc, 0, "message", out->text, sizeof(out->text));
        (void)string_field(doc, 0, "emotion", out->emotion, sizeof(out->emotion));
    } else if (strcmp(type, "mcp") == 0) {
        out->type = XIAOZHI_MSG_MCP;
    }
    return true;
}

bool xiaozhi_build_hello(char *out, size_t size)
{
    /* No "features": without mcp the server sends no tool calls. */
    return fits(snprintf(out, size,
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":%u,\"channels\":1,\"frame_duration\":%u}}",
        (unsigned)XIAOZHI_UPLINK_SAMPLE_RATE, (unsigned)XIAOZHI_FRAME_MS), size);
}
bool xiaozhi_build_listen_start(char *out, size_t size, const char *session_id, xiaozhi_listen_mode_t mode)
{
    return session_ok(session_id) && fits(snprintf(out, size,
        "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"%s\"}",
        session_id, mode == XIAOZHI_LISTEN_MANUAL ? "manual" : "auto"), size);
}
bool xiaozhi_build_listen_stop(char *out, size_t size, const char *session_id)
{
    return session_ok(session_id) && fits(snprintf(out, size,
        "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"stop\"}", session_id), size);
}
bool xiaozhi_build_abort(char *out, size_t size, const char *session_id)
{
    return session_ok(session_id) && fits(snprintf(out, size,
        "{\"session_id\":\"%s\",\"type\":\"abort\"}", session_id), size);
}
