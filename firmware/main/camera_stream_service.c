#include "camera_stream_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "esp_h264_dec.h"
#include "esp_h264_dec_sw.h"
#include "esp_h264_dec_param.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_rom_md5.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha1.h"

#define STREAM_TASK_STACK_BYTES 12288U
#define STREAM_TASK_PRIORITY 5U
#define RTSP_RESPONSE_CAPACITY 8192U
#define RTP_PACKET_CAPACITY 2048U
#define ACCESS_UNIT_CAPACITY (512U * 1024U)
#define CREDENTIAL_USER_CAPACITY 48U
#define CREDENTIAL_PASSWORD_CAPACITY 64U
#define RTSP_PORT 554U
#define ONVIF_PORT 2020U

static const char *const TAG = "camera_stream";
static const char *const STREAM_PATHS[] = {"/stream8", "/stream2"};
static const uint8_t START_CODE[] = {0U, 0U, 0U, 1U};

typedef enum {
    COMMAND_PLAY = 1,
    COMMAND_STOP,
} command_kind_t;

typedef struct {
    command_kind_t kind;
    char ipv4[16];
    char username[CREDENTIAL_USER_CAPACITY];
    char password[CREDENTIAL_PASSWORD_CAPACITY];
} camera_command_t;

typedef struct {
    char realm[96];
    char nonce[160];
    char opaque[96];
    char qop[16];
    char algorithm[16];
    bool valid;
} digest_challenge_t;

typedef struct {
    int socket_fd;
    char base_url[96];
    char track_url[256];
    const char *stream_path;
    char session_id[64];
    char username[CREDENTIAL_USER_CAPACITY];
    char password[CREDENTIAL_PASSWORD_CAPACITY];
    digest_challenge_t challenge;
    uint32_t cseq;
    uint32_t nonce_count;
    uint8_t *response;
    uint8_t *access_unit;
    uint8_t *packet;
    size_t access_unit_size;
    bool fu_active;
    bool decoder_error;
    bool sps_logged;
    uint8_t fu_header;
    esp_h264_dec_handle_t decoder;
    esp_h264_dec_param_handle_t decoder_params;
} stream_session_t;

static QueueHandle_t s_command_queue;
static SemaphoreHandle_t s_frame_mutex;
static TaskHandle_t s_worker;
static camera_stream_status_t s_status;
static uint8_t *s_frames[2];
static uint8_t s_published_frame;
static bool s_frame_ready;

static bool consume_interleaved_packet(stream_session_t *session);

static void set_status(camera_stream_state_t state, esp_err_t error)
{
    if (s_frame_mutex == NULL) return;
    xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
    s_status.state = state;
    s_status.last_error = error;
    if (state == CAMERA_STREAM_CONNECTING) s_status.last_rtsp_status = 0U;
    xSemaphoreGive(s_frame_mutex);
}

static void clear_published_frame(void)
{
    if (s_frame_mutex == NULL) return;
    xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
    s_frame_ready = false;
    s_status.width = 0U;
    s_status.height = 0U;
    s_status.frame_sequence = 0U;
    xSemaphoreGive(s_frame_mutex);
}

void camera_stream_service_get_status(camera_stream_status_t *out_status)
{
    if (out_status == NULL) return;
    if (s_frame_mutex == NULL) {
        *out_status = (camera_stream_status_t){.state = CAMERA_STREAM_STOPPED};
        return;
    }
    xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
    *out_status = s_status;
    xSemaphoreGive(s_frame_mutex);
}

esp_err_t camera_stream_service_copy_frame(uint8_t *dst, size_t capacity,
                                           uint16_t *width, uint16_t *height,
                                           uint32_t *sequence)
{
    if (dst == NULL || width == NULL || height == NULL || sequence == NULL ||
        s_frame_mutex == NULL) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
    const size_t bytes = (size_t)s_status.width * s_status.height * 2U;
    if (!s_frame_ready || bytes == 0U || capacity < bytes) {
        xSemaphoreGive(s_frame_mutex);
        return s_frame_ready ? ESP_ERR_INVALID_SIZE : ESP_ERR_NOT_FOUND;
    }
    memcpy(dst, s_frames[s_published_frame], bytes);
    *width = s_status.width;
    *height = s_status.height;
    *sequence = s_status.frame_sequence;
    xSemaphoreGive(s_frame_mutex);
    return ESP_OK;
}

static bool private_ipv4(const char *address)
{
    struct in_addr ip = {0};
    if (address == NULL || inet_aton(address, &ip) == 0) return false;
    const uint32_t host = ntohl(ip.s_addr);
    return (host >> 24U) == 10U || (host >> 20U) == 0xAC1U ||
           (host >> 16U) == 0xC0A8U;
}

static bool safe_credential(const char *value, size_t capacity, bool allow_empty)
{
    if (value == NULL) return false;
    const size_t length = strnlen(value, capacity);
    if (length == capacity || (!allow_empty && length == 0U)) return false;
    for (size_t i = 0U; i < length; ++i) {
        const unsigned char c = (unsigned char)value[i];
        if (c < 0x20U || c == 0x7FU) return false;
    }
    return true;
}

esp_err_t camera_stream_service_play(const char *ipv4, const char *username,
                                     const char *password)
{
    if (s_command_queue == NULL || !private_ipv4(ipv4) ||
        !safe_credential(username, CREDENTIAL_USER_CAPACITY, false) ||
        !safe_credential(password, CREDENTIAL_PASSWORD_CAPACITY, false)) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Username is inserted as a quoted Digest field; reject header syntax. */
    for (const char *p = username; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\' || *p == ',') return ESP_ERR_INVALID_ARG;
    }
    camera_command_t command = {.kind = COMMAND_PLAY};
    (void)snprintf(command.ipv4, sizeof(command.ipv4), "%s", ipv4);
    (void)snprintf(command.username, sizeof(command.username), "%s", username);
    (void)snprintf(command.password, sizeof(command.password), "%s", password);
    if (xQueueOverwrite(s_command_queue, &command) != pdPASS) {
        memset(&command, 0, sizeof(command));
        return ESP_ERR_TIMEOUT;
    }
    memset(&command, 0, sizeof(command));
    return ESP_OK;
}

esp_err_t camera_stream_service_stop(void)
{
    if (s_command_queue == NULL) return ESP_ERR_INVALID_STATE;
    camera_command_t command = {.kind = COMMAND_STOP};
    return xQueueOverwrite(s_command_queue, &command) == pdPASS
        ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void md5_hex(const char *input, char output[33])
{
    uint8_t digest[ESP_ROM_MD5_DIGEST_LEN] = {0};
    md5_context_t context;
    esp_rom_md5_init(&context);
    esp_rom_md5_update(&context, input, (uint32_t)strlen(input));
    esp_rom_md5_final(digest, &context);
    for (size_t i = 0U; i < sizeof(digest); ++i) {
        (void)snprintf(output + i * 2U, 3U, "%02x", digest[i]);
    }
    memset(digest, 0, sizeof(digest));
    memset(&context, 0, sizeof(context));
}

static bool parse_digest_value(const char *header, const char *name,
                               char *destination, size_t capacity)
{
    char key[24] = {0};
    if (snprintf(key, sizeof(key), "%s=\"", name) < 0) return false;
    const char *value = strstr(header, key);
    if (value == NULL) {
        if (snprintf(key, sizeof(key), "%s=", name) < 0) return false;
        value = strstr(header, key);
        if (value == NULL) return false;
        value += strlen(key);
        size_t length = strcspn(value, ", \r\n");
        if (length == 0U || length >= capacity) return false;
        memcpy(destination, value, length);
        destination[length] = '\0';
        return true;
    }
    value += strlen(key);
    const char *end = strchr(value, '"');
    if (end == NULL || (size_t)(end - value) >= capacity) return false;
    memcpy(destination, value, (size_t)(end - value));
    destination[end - value] = '\0';
    return true;
}

static bool parse_digest_challenge(const char *header,
                                   digest_challenge_t *challenge)
{
    const char *value = strstr(header, "WWW-Authenticate:");
    if (value == NULL || strstr(value, "Digest") == NULL) return false;
    digest_challenge_t parsed = {0};
    if (!parse_digest_value(value, "realm", parsed.realm, sizeof(parsed.realm)) ||
        !parse_digest_value(value, "nonce", parsed.nonce, sizeof(parsed.nonce)))
        return false;
    (void)parse_digest_value(value, "opaque", parsed.opaque, sizeof(parsed.opaque));
    (void)parse_digest_value(value, "qop", parsed.qop, sizeof(parsed.qop));
    (void)parse_digest_value(value, "algorithm", parsed.algorithm,
                             sizeof(parsed.algorithm));
    if (parsed.algorithm[0] != '\0' && strcmp(parsed.algorithm, "MD5") != 0)
        return false;
    if (parsed.qop[0] != '\0' && strstr(parsed.qop, "auth") == NULL)
        return false;
    parsed.valid = true;
    *challenge = parsed;
    return true;
}

static bool digest_authorization(stream_session_t *session, const char *method,
                                 const char *uri, char *output, size_t capacity)
{
    if (!session->challenge.valid) return false;
    char scratch[320] = {0};
    char ha1[33] = {0};
    char ha2[33] = {0};
    char response[33] = {0};
    char cnonce[17] = {0};
    (void)snprintf(cnonce, sizeof(cnonce), "%08lx%08lx",
                   (unsigned long)esp_random(), (unsigned long)esp_random());
    const bool qop_auth = session->challenge.qop[0] != '\0';
    int n = snprintf(scratch, sizeof(scratch), "%s:%s:%s", session->username,
                     session->challenge.realm, session->password);
    if (n < 0 || (size_t)n >= sizeof(scratch)) return false;
    md5_hex(scratch, ha1);
    n = snprintf(scratch, sizeof(scratch), "%s:%s", method, uri);
    if (n < 0 || (size_t)n >= sizeof(scratch)) return false;
    md5_hex(scratch, ha2);
    if (qop_auth) {
        ++session->nonce_count;
        n = snprintf(scratch, sizeof(scratch), "%s:%s:%08lx:%s:auth:%s", ha1,
            session->challenge.nonce, (unsigned long)session->nonce_count,
            cnonce, ha2);
    } else {
        n = snprintf(scratch, sizeof(scratch), "%s:%s:%s", ha1,
                     session->challenge.nonce, ha2);
    }
    if (n < 0 || (size_t)n >= sizeof(scratch)) return false;
    md5_hex(scratch, response);
    n = snprintf(output, capacity,
        "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"",
        session->username, session->challenge.realm, session->challenge.nonce,
        uri, response);
    if (n >= 0 && (size_t)n < capacity && session->challenge.opaque[0] != '\0') {
        const size_t used = (size_t)n;
        n += snprintf(output + used, capacity - used, ", opaque=\"%s\"",
                      session->challenge.opaque);
    }
    if (n >= 0 && (size_t)n < capacity && qop_auth) {
        const size_t used = (size_t)n;
        n += snprintf(output + used, capacity - used,
            ", qop=auth, nc=%08lx, cnonce=\"%s\"",
            (unsigned long)session->nonce_count, cnonce);
    }
    memset(scratch, 0, sizeof(scratch));
    memset(ha1, 0, sizeof(ha1));
    memset(ha2, 0, sizeof(ha2));
    memset(response, 0, sizeof(response));
    return n >= 0 && (size_t)n < capacity;
}

static bool command_pending(void)
{
    return s_command_queue != NULL && uxQueueMessagesWaiting(s_command_queue) > 0U;
}

static bool send_all(int fd, const char *data, size_t size)
{
    size_t sent = 0U;
    while (sent < size) {
        const int result = send(fd, data + sent, size - sent, 0);
        if (result <= 0) return false;
        sent += (size_t)result;
    }
    return true;
}

static bool recv_exact(stream_session_t *session, void *buffer, size_t size,
                       uint32_t timeout_ms)
{
    size_t received = 0U;
    uint8_t *out = buffer;
    while (received < size) {
        if (command_pending()) return false;
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(session->socket_fd, &read_set);
        struct timeval timeout = {
            .tv_sec = (time_t)(timeout_ms / 1000U),
            .tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U),
        };
        const int ready = select(session->socket_fd + 1, &read_set, NULL, NULL,
                                 &timeout);
        if (ready <= 0) return false;
        const int n = recv(session->socket_fd, out + received, size - received, 0);
        if (n <= 0) return false;
        received += (size_t)n;
    }
    return true;
}

static int connect_rtsp(const char *ipv4)
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    const int timeout_ms = 2500;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout_ms,
                     sizeof(timeout_ms));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(RTSP_PORT),
    };
    if (inet_aton(ipv4, &address.sin_addr) == 0 ||
        connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        closesocket(fd);
        return -1;
    }
    return fd;
}

static int connect_onvif(const char *ipv4)
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    const int timeout_ms = 2500;
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout_ms,
                     sizeof(timeout_ms));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(ONVIF_PORT),
    };
    if (inet_aton(ipv4, &address.sin_addr) == 0 ||
        connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        closesocket(fd);
        return -1;
    }
    return fd;
}

/* RTSP response parser is bounded to 8 KiB total, including SDP. */
static bool read_response(stream_session_t *session, char **body,
                          size_t *body_length, int *status_code)
{
    size_t used = 0U;
    bool headers_done = false;
    while (used + 1U < RTSP_RESPONSE_CAPACITY && !headers_done) {
        if (!recv_exact(session, session->response + used, 1U, 1800U)) return false;
        ++used;
        session->response[used] = '\0';
        if (used >= 4U && memcmp(session->response + used - 4U, "\r\n\r\n", 4U) == 0)
            headers_done = true;
    }
    if (!headers_done) return false;
    char *space = strchr((char *)session->response, ' ');
    if (space == NULL || sscanf(space + 1, "%d", status_code) != 1) return false;
    if (s_frame_mutex != NULL) {
        xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
        s_status.last_rtsp_status = (uint16_t)*status_code;
        xSemaphoreGive(s_frame_mutex);
    }
    size_t content_length = 0U;
    char *length_header = strcasestr((char *)session->response, "Content-Length:");
    if (length_header != NULL) {
        unsigned long parsed = 0UL;
        if (sscanf(length_header, "Content-Length: %lu", &parsed) == 1 ||
            sscanf(length_header, "content-length: %lu", &parsed) == 1) {
            if (parsed > RTSP_RESPONSE_CAPACITY - used - 1U) return false;
            content_length = (size_t)parsed;
        }
    }
    if (content_length > 0U &&
        !recv_exact(session, session->response + used, content_length, 1800U))
        return false;
    used += content_length;
    session->response[used] = '\0';
    char *header_end = strstr((char *)session->response, "\r\n\r\n");
    if (header_end == NULL) return false;
    *body = header_end + 4;
    *body_length = content_length;
    return true;
}

static bool send_request(stream_session_t *session, const char *method,
                         const char *uri, const char *extra_header,
                         char **body, size_t *body_length, int *status_code)
{
    char auth[512] = {0};
    if (session->challenge.valid &&
        !digest_authorization(session, method, uri, auth, sizeof(auth)))
        return false;
    char request[1024] = {0};
    const uint32_t cseq = ++session->cseq;
    int n = snprintf(request, sizeof(request),
        "%s %s RTSP/1.0\r\nCSeq: %lu\r\nUser-Agent: NovaPanel/1.0\r\n%s%s%s\r\n",
        method, uri, (unsigned long)cseq,
        auth[0] != '\0' ? auth : "",
        auth[0] != '\0' ? "\r\n" : "",
        extra_header != NULL ? extra_header : "");
    if (n < 0 || (size_t)n >= sizeof(request)) return false;
    if (!send_all(session->socket_fd, request, (size_t)n)) {
        ESP_LOGW(TAG, "RTSP %s send failed (errno=%d)", method, errno);
        return false;
    }
    if (!read_response(session, body, body_length, status_code)) {
        ESP_LOGW(TAG, "RTSP %s response timeout or malformed response", method);
        return false;
    }
    ESP_LOGI(TAG, "RTSP %s response status=%d", method, *status_code);

    /* Keep only parsed challenge/session data. Never log RTSP headers. */
    if (*status_code == 401) {
        if (!parse_digest_challenge((char *)session->response,
                                    &session->challenge)) return false;
        session->nonce_count = 0U;
    }
    return true;
}

static bool read_http_soap_response(stream_session_t *session,
                                    char **body, size_t *body_length,
                                    int *status_code)
{
    size_t used = 0U;
    bool headers_done = false;
    while (used + 1U < RTSP_RESPONSE_CAPACITY && !headers_done) {
        if (!recv_exact(session, session->response + used, 1U, 1800U))
            return false;
        ++used;
        session->response[used] = '\0';
        headers_done = used >= 4U &&
            memcmp(session->response + used - 4U, "\r\n\r\n", 4U) == 0;
    }
    if (!headers_done || sscanf((char *)session->response, "HTTP/%*u.%*u %d",
                                status_code) != 1) return false;
    size_t content_length = 0U;
    char *length_header = strcasestr((char *)session->response, "Content-Length:");
    if (length_header != NULL) {
        unsigned long parsed = 0UL;
        if (sscanf(length_header, "Content-Length: %lu", &parsed) != 1 &&
            sscanf(length_header, "content-length: %lu", &parsed) != 1)
            return false;
        if (parsed > RTSP_RESPONSE_CAPACITY - used - 1U) return false;
        content_length = (size_t)parsed;
    }
    if (content_length > 0U &&
        !recv_exact(session, session->response + used, content_length, 1800U))
        return false;
    used += content_length;
    session->response[used] = '\0';
    char *const header_end = strstr((char *)session->response, "\r\n\r\n");
    if (header_end == NULL) return false;
    *body = header_end + 4;
    *body_length = content_length;
    return true;
}

static bool xml_escape_username(const char *username, char *escaped,
                                size_t capacity)
{
    if (username == NULL || escaped == NULL || capacity == 0U) return false;
    size_t used = 0U;
    for (const char *cursor = username; *cursor != '\0'; ++cursor) {
        const char *replacement = NULL;
        switch (*cursor) {
        case '&': replacement = "&amp;"; break;
        case '<': replacement = "&lt;"; break;
        case '>': replacement = "&gt;"; break;
        case '\"': replacement = "&quot;"; break;
        case '\'': replacement = "&apos;"; break;
        default: break;
        }
        const size_t length = replacement != NULL ? strlen(replacement) : 1U;
        if (used + length >= capacity) return false;
        if (replacement != NULL) {
            memcpy(escaped + used, replacement, length);
        } else {
            escaped[used] = *cursor;
        }
        used += length;
    }
    escaped[used] = '\0';
    return true;
}

static bool onvif_username_token_body(const stream_session_t *session,
                                      const char *soap_body, char *output,
                                      size_t output_capacity)
{
    static const char security_ns[] =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-"
        "wssecurity-secext-1.0.xsd";
    static const char utility_ns[] =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-"
        "wssecurity-utility-1.0.xsd";
    static const char password_digest_type[] =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-"
        "username-token-profile-1.0#PasswordDigest";
    static const char nonce_encoding_type[] =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-"
        "soap-message-security-1.0#Base64Binary";

    if (session == NULL || soap_body == NULL || output == NULL ||
        output_capacity == 0U) return false;
    const char *body_tag = strstr(soap_body, "<s:Body>");
    const char *envelope_end = strstr(soap_body, "</s:Envelope>");
    if (body_tag == NULL || envelope_end == NULL || body_tag >= envelope_end)
        return false;

    const time_t now = time(NULL);
    if (now < (time_t)1577836800) return false;
    struct tm utc = {0};
    if (gmtime_r(&now, &utc) == NULL) return false;
    char created[32] = {0};
    if (strftime(created, sizeof(created), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0U)
        return false;

    uint8_t nonce[16] = {0};
    uint8_t digest[20] = {0};
    char nonce_b64[32] = {0};
    char digest_b64[32] = {0};
    char escaped_username[CREDENTIAL_USER_CAPACITY * 5U] = {0};
    size_t encoded_length = 0U;
    bool valid = false;
    esp_fill_random(nonce, sizeof(nonce));
    if (!xml_escape_username(session->username, escaped_username,
                             sizeof(escaped_username)) ||
        mbedtls_base64_encode((unsigned char *)nonce_b64, sizeof(nonce_b64) - 1U,
            &encoded_length, nonce, sizeof(nonce)) != 0 ||
        encoded_length >= sizeof(nonce_b64)) goto done;
    nonce_b64[encoded_length] = '\0';

    mbedtls_sha1_context hash;
    mbedtls_sha1_init(&hash);
    const bool hash_ok = mbedtls_sha1_starts(&hash) == 0 &&
        mbedtls_sha1_update(&hash, nonce, sizeof(nonce)) == 0 &&
        mbedtls_sha1_update(&hash, (const unsigned char *)created,
                            strlen(created)) == 0 &&
        mbedtls_sha1_update(&hash, (const unsigned char *)session->password,
                            strlen(session->password)) == 0 &&
        mbedtls_sha1_finish(&hash, digest) == 0;
    mbedtls_sha1_free(&hash);
    if (!hash_ok || mbedtls_base64_encode((unsigned char *)digest_b64,
            sizeof(digest_b64) - 1U, &encoded_length, digest,
            sizeof(digest)) != 0 || encoded_length >= sizeof(digest_b64))
        goto done;
    digest_b64[encoded_length] = '\0';

    const size_t prefix_length = (size_t)(body_tag - soap_body);
    const char *const suffix = body_tag;
    const int written = snprintf(output, output_capacity,
        "%.*s<s:Header><wsse:Security s:mustUnderstand=\"1\" "
        "xmlns:wsse=\"%s\" xmlns:wsu=\"%s\"><wsse:UsernameToken>"
        "<wsse:Username>%s</wsse:Username>"
        "<wsse:Password Type=\"%s\">%s</wsse:Password>"
        "<wsse:Nonce EncodingType=\"%s\">%s</wsse:Nonce>"
        "<wsu:Created>%s</wsu:Created></wsse:UsernameToken>"
        "</wsse:Security></s:Header>%s",
        (int)prefix_length, soap_body, security_ns, utility_ns,
        escaped_username, password_digest_type, digest_b64,
        nonce_encoding_type, nonce_b64, created, suffix);
    valid = written > 0 && (size_t)written < output_capacity;

done:
    memset(nonce, 0, sizeof(nonce));
    memset(digest, 0, sizeof(digest));
    memset(nonce_b64, 0, sizeof(nonce_b64));
    memset(digest_b64, 0, sizeof(digest_b64));
    memset(escaped_username, 0, sizeof(escaped_username));
    memset(created, 0, sizeof(created));
    return valid;
}

static bool onvif_soap_post(stream_session_t *session, const char *ipv4,
                            const char *path, const char *action,
                            const char *soap_body, bool username_token,
                            char **response_body,
                            size_t *response_length, int *status_code)
{
    if (session == NULL || ipv4 == NULL || path == NULL || path[0] != '/' ||
        action == NULL || soap_body == NULL || response_body == NULL ||
        response_length == NULL || status_code == NULL) return false;
    for (unsigned attempt = 0U; attempt < 2U; ++attempt) {
        session->socket_fd = connect_onvif(ipv4);
        if (session->socket_fd < 0) return false;

        char authorization[512] = {0};
        if (session->challenge.valid &&
            !digest_authorization(session, "POST", path, authorization,
                                  sizeof(authorization))) {
            closesocket(session->socket_fd);
            session->socket_fd = -1;
            return false;
        }
        char *const request_buffers = heap_caps_calloc(1U, 5120U,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (request_buffers == NULL) {
            closesocket(session->socket_fd);
            session->socket_fd = -1;
            memset(authorization, 0, sizeof(authorization));
            return false;
        }
        char *const authenticated_soap = request_buffers;
        char *const request = request_buffers + 2048U;
        const char *request_body = soap_body;
        if (username_token) {
            if (!onvif_username_token_body(session, soap_body, authenticated_soap,
                                           2048U)) {
                closesocket(session->socket_fd);
                session->socket_fd = -1;
                memset(authorization, 0, sizeof(authorization));
                memset(request_buffers, 0, 5120U);
                free(request_buffers);
                return false;
            }
            request_body = authenticated_soap;
        }
        const size_t soap_length = strlen(request_body);
        const int request_length = snprintf(request, 3072U,
            "POST %s HTTP/1.1\r\nHost: %s:%u\r\n"
            "Content-Type: application/soap+xml; charset=utf-8; action=\"%s\"\r\n"
            "Connection: close\r\nContent-Length: %u\r\n%s%s\r\n%s",
            path, ipv4, ONVIF_PORT, action, (unsigned int)soap_length,
            authorization[0] != '\0' ? authorization : "",
            authorization[0] != '\0' ? "\r\n" : "", request_body);
        const bool request_ok = request_length > 0 &&
            (size_t)request_length < 3072U &&
            send_all(session->socket_fd, request, (size_t)request_length) &&
            read_http_soap_response(session, response_body, response_length,
                                    status_code);
        memset(request_buffers, 0, 5120U);
        free(request_buffers);
        memset(authorization, 0, sizeof(authorization));
        closesocket(session->socket_fd);
        session->socket_fd = -1;
        if (!request_ok) return false;
        if (*status_code != 401) return true;
        if (attempt != 0U || username_token ||
            !parse_digest_challenge((char *)session->response,
                                    &session->challenge)) return true;
        session->nonce_count = 0U;
    }
    return false;
}

static bool extract_xml_local_text(const char *xml, const char *local_name,
                                   char *value, size_t value_capacity)
{
    if (xml == NULL || local_name == NULL || value == NULL ||
        value_capacity == 0U) return false;
    const size_t local_length = strlen(local_name);
    const char *cursor = xml;
    while ((cursor = strchr(cursor, '<')) != NULL) {
        const char *name = cursor + 1;
        if (*name == '/' || *name == '?' || *name == '!') {
            cursor = name;
            continue;
        }
        const char *name_end = name;
        while (*name_end != '\0' && *name_end != '>' && *name_end != '/' &&
               *name_end != ' ' && *name_end != '\t' && *name_end != '\r' &&
               *name_end != '\n') ++name_end;
        const char *local_start = name;
        for (const char *part = name; part < name_end; ++part)
            if (*part == ':') local_start = part + 1;
        if ((size_t)(name_end - local_start) == local_length &&
            memcmp(local_start, local_name, local_length) == 0) {
            const char *const open_end = strchr(name_end, '>');
            char close_tag[64] = {0};
            const int close_length = snprintf(close_tag, sizeof(close_tag),
                "</%.*s>", (int)(name_end - name), name);
            if (open_end == NULL || close_length <= 0 ||
                (size_t)close_length >= sizeof(close_tag)) return false;
            const char *const content = open_end + 1;
            const char *const close = strstr(content, close_tag);
            if (close == NULL) return false;
            const size_t length = (size_t)(close - content);
            if (length == 0U || length >= value_capacity) return false;
            memcpy(value, content, length);
            value[length] = '\0';
            return true;
        }
        cursor = name_end;
    }
    return false;
}

static bool onvif_media_path(const char *xaddr, const char *ipv4,
                             char *path, size_t path_capacity)
{
    char expected_prefix[64] = {0};
    const int prefix_length = snprintf(expected_prefix, sizeof(expected_prefix),
                                       "http://%s:%u", ipv4, ONVIF_PORT);
    if (prefix_length <= 0 || (size_t)prefix_length >= sizeof(expected_prefix) ||
        strncmp(xaddr, expected_prefix, (size_t)prefix_length) != 0 ||
        xaddr[prefix_length] != '/') return false;
    const size_t path_length = strlen(xaddr + prefix_length);
    if (path_length == 0U || path_length >= path_capacity ||
        strchr(xaddr + prefix_length, '@') != NULL) return false;
    memcpy(path, xaddr + prefix_length, path_length + 1U);
    return true;
}

static uint8_t parse_h264_profile_options(const char *xml)
{
    enum {
        PROFILE_BASELINE = 1U << 0,
        PROFILE_MAIN = 1U << 1,
        PROFILE_EXTENDED = 1U << 2,
        PROFILE_HIGH = 1U << 3,
    };
    uint8_t profiles = 0U;
    const char *cursor = xml;
    while ((cursor = strstr(cursor, "ProfilesSupported")) != NULL) {
        const char *const text_start = strchr(cursor, '>');
        const char *const text_end = text_start != NULL
            ? strchr(text_start + 1, '<') : NULL;
        if (text_start == NULL || text_end == NULL) break;
        const char *const profile = text_start + 1;
        const size_t length = (size_t)(text_end - profile);
        if (length == 8U && memcmp(profile, "Baseline", 8U) == 0)
            profiles |= PROFILE_BASELINE;
        else if (length == 4U && memcmp(profile, "Main", 4U) == 0)
            profiles |= PROFILE_MAIN;
        else if (length == 9U && memcmp(profile, "Extended", 9U) == 0)
            profiles |= PROFILE_EXTENDED;
        else if (length == 4U && memcmp(profile, "High", 4U) == 0)
            profiles |= PROFILE_HIGH;
        cursor = text_end + 1;
    }
    return profiles;
}

static void log_h264_profile_options(uint8_t profiles)
{
    char names[72] = {0};
    size_t used = 0U;
    const struct { uint8_t bit; const char *name; } known[] = {
        {1U << 0, "Baseline"}, {1U << 1, "Main"},
        {1U << 2, "Extended"}, {1U << 3, "High"},
    };
    for (size_t i = 0U; i < sizeof(known) / sizeof(known[0]); ++i) {
        if ((profiles & known[i].bit) == 0U) continue;
        const int written = snprintf(names + used, sizeof(names) - used,
            "%s%s", used == 0U ? "" : ",", known[i].name);
        if (written < 0 || (size_t)written >= sizeof(names) - used) break;
        used += (size_t)written;
    }
    ESP_LOGI(TAG, "ONVIF H264Options ProfilesSupported=%s",
             names[0] != '\0' ? names : "none reported");
}

static const char *onvif_known_fault(const char *body)
{
    if (body == NULL) return "no_response_body";
    if (strstr(body, "ActionNotSupported") != NULL)
        return "ActionNotSupported";
    if (strstr(body, "InvalidArgVal") != NULL)
        return "InvalidArgVal";
    if (strstr(body, "NoProfile") != NULL)
        return "NoProfile";
    if (strstr(body, "NotAuthorized") != NULL ||
        strstr(body, "AuthenticationFailed") != NULL)
        return "AuthenticationFailed";
    if (strstr(body, "ter:InvalidArgVal") != NULL)
        return "InvalidArgVal";
    return "unclassified";
}

static void query_onvif_h264_options(stream_session_t *session,
                                     const char *ipv4)
{
    static const char get_capabilities[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"><s:Body>"
        "<tds:GetCapabilities><tds:Category>Media</tds:Category>"
        "</tds:GetCapabilities></s:Body></s:Envelope>";
    static const char get_options[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\"><s:Body>"
        "<trt:GetVideoEncoderConfigurationOptions/>"
        "</s:Body></s:Envelope>";
    static const char device_path[] = "/onvif/device_service";
    static const char capabilities_action[] =
        "http://www.onvif.org/ver10/device/wsdl/GetCapabilities";
    static const char options_action[] =
        "http://www.onvif.org/ver10/media/wsdl/GetVideoEncoderConfigurationOptions";
    char media_xaddr[192] = {0};
    char media_path[128] = {0};
    char *body = NULL;
    size_t body_length = 0U;
    int status = 0;

    ESP_LOGI(TAG, "Starting read-only ONVIF encoder profile query");
    if (!onvif_soap_post(session, ipv4, device_path, capabilities_action,
                         get_capabilities, false, &body, &body_length, &status) ||
        status != 200 || !extract_xml_local_text(body, "XAddr", media_xaddr,
                                                  sizeof(media_xaddr)) ||
        !onvif_media_path(media_xaddr, ipv4, media_path, sizeof(media_path))) {
        ESP_LOGW(TAG, "ONVIF GetCapabilities unavailable (status=%d)", status);
        goto done;
    }
    ESP_LOGI(TAG, "ONVIF GetCapabilities succeeded; Media endpoint validated");
    session->challenge = (digest_challenge_t){0};
    session->nonce_count = 0U;
    body = NULL;
    body_length = 0U;
    status = 0;
    const bool options_received = onvif_soap_post(session, ipv4, media_path,
        options_action, get_options, true, &body, &body_length, &status);
    if (!options_received || status != 200 || body == NULL ||
        body_length == 0U ||
        strstr(body, "H264ProfilesSupported") == NULL) {
        ESP_LOGW(TAG, "ONVIF GetVideoEncoderConfigurationOptions failed "
                      "status=%d body_bytes=%u fault=%s",
                 status, (unsigned int)body_length, onvif_known_fault(body));
        goto done;
    }
    log_h264_profile_options(parse_h264_profile_options(body));

done:
    if (session->socket_fd >= 0) {
        closesocket(session->socket_fd);
        session->socket_fd = -1;
    }
    memset(media_xaddr, 0, sizeof(media_xaddr));
    memset(media_path, 0, sizeof(media_path));
    session->challenge = (digest_challenge_t){0};
    session->nonce_count = 0U;
}

static bool response_session(stream_session_t *session)
{
    char *header = strcasestr((char *)session->response, "Session:");
    if (header == NULL) return false;
    header += strlen("Session:");
    while (*header == ' ') ++header;
    size_t length = strcspn(header, ";\r\n");
    if (length == 0U || length >= sizeof(session->session_id)) return false;
    memcpy(session->session_id, header, length);
    session->session_id[length] = '\0';
    return true;
}

static bool parse_sdp_track(stream_session_t *session, const char *body,
                            size_t body_length)
{
    if (body_length == 0U || body_length >= RTSP_RESPONSE_CAPACITY) return false;
    char sdp[RTSP_RESPONSE_CAPACITY] = {0};
    memcpy(sdp, body, body_length);
    if (strstr(sdp, "H264") == NULL && strstr(sdp, "h264") == NULL) return false;
    char *control = sdp;
    size_t length = 0U;
    char *selected = NULL;
    while ((control = strstr(control, "a=control:")) != NULL) {
        control += strlen("a=control:");
        length = strcspn(control, "\r\n");
        if (length > 0U && !(length == 1U && control[0] == '*')) {
            selected = control;
            break;
        }
    }
    if (selected == NULL || length >= 128U) return false;
    char track[128] = {0};
    memcpy(track, selected, length);
    if (strncmp(track, "*", 1U) == 0) return false;
    if (strncmp(track, "rtsp://", 7U) == 0) {
        (void)snprintf(session->track_url, sizeof(session->track_url), "%s", track);
    } else if (track[0] == '/') {
        const char *host_end = strstr(session->base_url + 7, "/");
        const size_t host_length = host_end != NULL
            ? (size_t)(host_end - session->base_url) : strlen(session->base_url);
        (void)snprintf(session->track_url, sizeof(session->track_url), "%.*s%s",
                       (int)host_length, session->base_url, track);
    } else {
        size_t base_length = strlen(session->base_url);
        if (base_length > 0U && session->base_url[base_length - 1U] != '/') {
            (void)snprintf(session->track_url, sizeof(session->track_url), "%s/%s",
                           session->base_url, track);
        } else {
            (void)snprintf(session->track_url, sizeof(session->track_url), "%s%s",
                           session->base_url, track);
        }
    }
    memset(sdp, 0, sizeof(sdp));
    return session->track_url[0] != '\0';
}

static void session_cleanup(stream_session_t *session)
{
    if (session == NULL) return;
    if (session->socket_fd >= 0) closesocket(session->socket_fd);
    if (session->decoder != NULL) (void)esp_h264_dec_del(session->decoder);
    if (session->response != NULL) heap_caps_free(session->response);
    if (session->access_unit != NULL) heap_caps_free(session->access_unit);
    if (session->packet != NULL) heap_caps_free(session->packet);
    memset(session, 0, sizeof(*session));
    session->socket_fd = -1;
}

static bool session_allocate(stream_session_t *session)
{
    session->response = heap_caps_malloc(RTSP_RESPONSE_CAPACITY,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    session->access_unit = heap_caps_malloc(ACCESS_UNIT_CAPACITY,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    session->packet = heap_caps_malloc(RTP_PACKET_CAPACITY,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (session->response == NULL || session->access_unit == NULL ||
        session->packet == NULL) return false;
    const esp_h264_dec_cfg_sw_t config = {.pic_type = ESP_H264_RAW_FMT_I420};
    if (esp_h264_dec_sw_new(&config, &session->decoder) != ESP_H264_ERR_OK ||
        session->decoder == NULL || esp_h264_dec_open(session->decoder) !=
            ESP_H264_ERR_OK ||
        esp_h264_dec_sw_get_param_hd(session->decoder,
                                     &session->decoder_params) != ESP_H264_ERR_OK)
        return false;
    return true;
}

static bool rtsp_exchange(stream_session_t *session, const char *method,
                          const char *uri, const char *headers,
                          int required_status, char **body, size_t *body_length)
{
    int status = 0;
    if (!send_request(session, method, uri, headers, body, body_length, &status))
        return false;
    if (status == 401) {
        set_status(CAMERA_STREAM_AUTHENTICATING, ESP_OK);
        if (!session->challenge.valid ||
            !send_request(session, method, uri, headers, body, body_length, &status))
            return false;
    }
    if (status != required_status) {
        ESP_LOGW(TAG, "RTSP %s rejected (status=%d, expected=%d)",
                 method, status, required_status);
        return false;
    }
    return true;
}

static bool session_open(stream_session_t *session,
                         const camera_command_t *command,
                         const char *stream_path)
{
    session->stream_path = stream_path;
    memcpy(session->username, command->username, sizeof(session->username));
    memcpy(session->password, command->password, sizeof(session->password));
    (void)snprintf(session->base_url, sizeof(session->base_url),
                   "rtsp://%s:%u%s", command->ipv4, RTSP_PORT, stream_path);
    session->socket_fd = connect_rtsp(command->ipv4);
    if (session->socket_fd < 0) return false;
    set_status(CAMERA_STREAM_CONNECTING, ESP_OK);
    char *body = NULL;
    size_t body_length = 0U;
    int status = 0;
    if (!send_request(session, "OPTIONS", session->base_url, NULL,
                      &body, &body_length, &status)) return false;
    if (status == 401) {
        set_status(CAMERA_STREAM_AUTHENTICATING, ESP_OK);
        if (!session->challenge.valid ||
            !send_request(session, "OPTIONS", session->base_url, NULL,
                          &body, &body_length, &status)) return false;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "RTSP OPTIONS rejected (status=%d)", status);
        return false;
    }
    if (!rtsp_exchange(session, "DESCRIBE", session->base_url,
            "Accept: application/sdp\r\n", 200, &body, &body_length) ||
        !parse_sdp_track(session, body, body_length)) return false;
    char headers[192] = "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n";
    if (!rtsp_exchange(session, "SETUP", session->track_url, headers, 200,
                       &body, &body_length) || !response_session(session))
        return false;
    (void)snprintf(headers, sizeof(headers), "Session: %s\r\n", session->session_id);
    if (!rtsp_exchange(session, "PLAY", session->base_url, headers, 200,
                       &body, &body_length)) return false;
    memset(session->username, 0, sizeof(session->username));
    memset(session->password, 0, sizeof(session->password));
    return true;
}

static void session_teardown(stream_session_t *session)
{
    if (session == NULL || session->socket_fd < 0 || session->session_id[0] == '\0')
        return;
    char headers[96] = {0};
    (void)snprintf(headers, sizeof(headers), "Session: %s\r\n", session->session_id);
    char *body = NULL;
    size_t body_length = 0U;
    int status = 0;
    (void)send_request(session, "TEARDOWN", session->base_url, headers,
                       &body, &body_length, &status);
}

static void stream_worker(void *argument)
{
    (void)argument;
    camera_command_t command = {0};
    for (;;) {
        if (xQueueReceive(s_command_queue, &command, portMAX_DELAY) != pdPASS)
            continue;
        if (command.kind == COMMAND_STOP) {
            clear_published_frame();
            set_status(CAMERA_STREAM_STOPPED, ESP_OK);
            memset(&command, 0, sizeof(command));
            continue;
        }
        clear_published_frame();
        xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
        s_status.last_rtsp_status = 0U;
        s_status.decoder_unsupported_profile = false;
        xSemaphoreGive(s_frame_mutex);

        stream_session_t onvif_session = {.socket_fd = -1};
        onvif_session.response = heap_caps_malloc(
            RTSP_RESPONSE_CAPACITY, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (onvif_session.response != NULL) {
            memcpy(onvif_session.username, command.username,
                   sizeof(onvif_session.username));
            memcpy(onvif_session.password, command.password,
                   sizeof(onvif_session.password));
            query_onvif_h264_options(&onvif_session, command.ipv4);
            session_cleanup(&onvif_session);
        } else {
            ESP_LOGW(TAG, "ONVIF encoder profile query skipped (no internal memory)");
        }

        bool opened_any = false;
        bool decoder_error = false;
        for (size_t path_index = 0U;
             path_index < sizeof(STREAM_PATHS) / sizeof(STREAM_PATHS[0]);
             ++path_index) {
            stream_session_t session = {.socket_fd = -1};
            ESP_LOGI(TAG, "Trying RTSP path %s", STREAM_PATHS[path_index]);
            const bool allocated = session_allocate(&session);
            const bool opened = allocated &&
                session_open(&session, &command, STREAM_PATHS[path_index]);
            if (!opened) {
                session_cleanup(&session);
                if (command_pending()) break;
                continue;
            }
            opened_any = true;
            set_status(CAMERA_STREAM_PLAYING, ESP_OK);
            bool stream_ok = true;
            while (!command_pending() && stream_ok) {
                stream_ok = consume_interleaved_packet(&session);
            }
            decoder_error = session.decoder_error;
            if (!decoder_error) session_teardown(&session);
            session_cleanup(&session);
            if (command_pending() || !decoder_error) break;
        }
        memset(&command, 0, sizeof(command));
        if (command_pending()) {
            /* The queued play/stop command will set its own state. */
            continue;
        }
        if (decoder_error) {
            xSemaphoreTake(s_frame_mutex, portMAX_DELAY);
            s_status.state = CAMERA_STREAM_ERROR;
            s_status.last_error = ESP_FAIL;
            s_status.decoder_unsupported_profile = true;
            xSemaphoreGive(s_frame_mutex);
            ESP_LOGW(TAG, "RTSP paths reached PLAY but the H.264 decoder rejected the stream");
        } else if (!opened_any) {
            set_status(CAMERA_STREAM_ERROR, ESP_FAIL);
            ESP_LOGW(TAG, "Could not establish an RTSP session on the available paths");
        } else {
            set_status(CAMERA_STREAM_ERROR, ESP_FAIL);
        }
    }
}

esp_err_t camera_stream_service_start(void)
{
    if (s_worker != NULL) return ESP_OK;
    s_frame_mutex = xSemaphoreCreateMutex();
    if (s_frame_mutex == NULL) return ESP_ERR_NO_MEM;
    s_command_queue = xQueueCreate(1U, sizeof(camera_command_t));
    if (s_command_queue == NULL) return ESP_ERR_NO_MEM;
    s_frames[0] = heap_caps_calloc(1U, CAMERA_STREAM_RGB565_BYTES,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_frames[1] = heap_caps_calloc(1U, CAMERA_STREAM_RGB565_BYTES,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_frames[0] == NULL || s_frames[1] == NULL) {
        if (s_frames[0] != NULL) heap_caps_free(s_frames[0]);
        if (s_frames[1] != NULL) heap_caps_free(s_frames[1]);
        s_frames[0] = NULL;
        s_frames[1] = NULL;
        vQueueDelete(s_command_queue);
        vSemaphoreDelete(s_frame_mutex);
        s_command_queue = NULL;
        s_frame_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_status = (camera_stream_status_t){.state = CAMERA_STREAM_STOPPED};
    if (xTaskCreate(stream_worker, "camera_stream", STREAM_TASK_STACK_BYTES,
                    NULL, STREAM_TASK_PRIORITY, &s_worker) != pdPASS) {
        s_worker = NULL;
        heap_caps_free(s_frames[0]);
        heap_caps_free(s_frames[1]);
        s_frames[0] = NULL;
        s_frames[1] = NULL;
        vQueueDelete(s_command_queue);
        vSemaphoreDelete(s_frame_mutex);
        s_command_queue = NULL;
        s_frame_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Local RTSP stream worker ready");
    return ESP_OK;
}

static bool append_bytes(stream_session_t *session, const void *data, size_t size)
{
    if (size > ACCESS_UNIT_CAPACITY - session->access_unit_size) return false;
    memcpy(session->access_unit + session->access_unit_size, data, size);
    session->access_unit_size += size;
    return true;
}

static bool append_nal(stream_session_t *session, const uint8_t *nal, size_t size)
{
    return size > 0U && append_bytes(session, START_CODE, sizeof(START_CODE)) &&
           append_bytes(session, nal, size);
}

static uint16_t yuv_to_rgb565(int y, int u, int v)
{
    const int c = y - 16;
    const int d = u - 128;
    const int e = v - 128;
    const int r = (298 * c + 409 * e + 128) >> 8;
    const int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    const int b = (298 * c + 516 * d + 128) >> 8;
    const uint16_t rr = (uint16_t)((r < 0 ? 0 : r > 255 ? 255 : r) >> 3);
    const uint16_t gg = (uint16_t)((g < 0 ? 0 : g > 255 ? 255 : g) >> 2);
    const uint16_t bb = (uint16_t)((b < 0 ? 0 : b > 255 ? 255 : b) >> 3);
    return (uint16_t)((rr << 11U) | (gg << 5U) | bb);
}

static bool publish_i420(const uint8_t *i420, size_t size,
                         uint16_t width, uint16_t height)
{
    if (i420 == NULL || width == 0U || height == 0U ||
        width > CAMERA_STREAM_MAX_WIDTH || height > CAMERA_STREAM_MAX_HEIGHT ||
        (width & 1U) != 0U || (height & 1U) != 0U) return false;
    const size_t pixels = (size_t)width * height;
    if (size < pixels + pixels / 2U) return false;
    const uint8_t *const y_plane = i420;
    const uint8_t *const u_plane = i420 + pixels;
    const uint8_t *const v_plane = u_plane + pixels / 4U;
    if (xSemaphoreTake(s_frame_mutex, pdMS_TO_TICKS(40U)) != pdTRUE) return false;
    const uint8_t target = (uint8_t)(s_published_frame ^ 1U);
    xSemaphoreGive(s_frame_mutex);
    uint16_t *const rgb = (uint16_t *)s_frames[target];
    for (uint16_t row = 0U; row < height; ++row) {
        const size_t y_row = (size_t)row * width;
        const size_t uv_row = (size_t)(row / 2U) * (width / 2U);
        for (uint16_t col = 0U; col < width; ++col) {
            const size_t p = y_row + col;
            const size_t uv = uv_row + col / 2U;
            rgb[p] = yuv_to_rgb565(y_plane[p], u_plane[uv], v_plane[uv]);
        }
    }
    if (xSemaphoreTake(s_frame_mutex, pdMS_TO_TICKS(40U)) != pdTRUE) return false;
    s_published_frame = target;
    s_frame_ready = true;
    s_status.width = width;
    s_status.height = height;
    ++s_status.frame_sequence;
    xSemaphoreGive(s_frame_mutex);
    return true;
}

static bool decode_access_unit(stream_session_t *session)
{
    if (session->access_unit_size == 0U) return true;
    if (!session->sps_logged) {
        const uint8_t *const data = session->access_unit;
        for (size_t i = 0U; i + 8U <= session->access_unit_size; ++i) {
            if (data[i] == 0U && data[i + 1U] == 0U &&
                data[i + 2U] == 0U && data[i + 3U] == 1U &&
                (data[i + 4U] & 0x1FU) == 7U) {
                ESP_LOGI(TAG,
                         "H264 SPS stream=%s profile_idc=%u constraints=0x%02x level_idc=%u",
                         session->stream_path != NULL ? session->stream_path : "unknown",
                         (unsigned int)data[i + 5U],
                         (unsigned int)data[i + 6U],
                         (unsigned int)data[i + 7U]);
                session->sps_logged = true;
                break;
            }
        }
    }
    esp_h264_dec_in_frame_t in = {
        .raw_data = {.buffer = session->access_unit,
                     .len = (uint32_t)session->access_unit_size},
    };
    bool valid = true;
    while (in.raw_data.len > 0U) {
        esp_h264_dec_out_frame_t out = {0};
        if (esp_h264_dec_process(session->decoder, &in, &out) != ESP_H264_ERR_OK ||
            in.consume == 0U || in.consume > in.raw_data.len) {
            valid = false;
            session->decoder_error = true;
            break;
        }
        if (out.out_size > 0U) {
            esp_h264_resolution_t resolution = {0};
            if (esp_h264_dec_get_resolution(session->decoder_params, &resolution) !=
                    ESP_H264_ERR_OK ||
                !publish_i420(out.outbuf, out.out_size,
                              (uint16_t)resolution.width,
                              (uint16_t)resolution.height)) {
                valid = false;
                session->decoder_error = true;
                break;
            }
        }
        in.raw_data.buffer += in.consume;
        in.raw_data.len -= in.consume;
    }
    session->access_unit_size = 0U;
    return valid;
}

static bool handle_h264_payload(stream_session_t *session,
                                const uint8_t *payload, size_t size,
                                bool marker)
{
    if (size == 0U) return false;
    const uint8_t nal_type = payload[0] & 0x1FU;
    bool ok = true;
    if (nal_type >= 1U && nal_type <= 23U) {
        session->fu_active = false;
        ok = append_nal(session, payload, size);
    } else if (nal_type == 24U) { /* STAP-A */
        session->fu_active = false;
        size_t offset = 1U;
        while (offset + 2U <= size) {
            const size_t nal_size = ((size_t)payload[offset] << 8U) |
                                    payload[offset + 1U];
            offset += 2U;
            if (nal_size == 0U || nal_size > size - offset ||
                !append_nal(session, payload + offset, nal_size)) {
                ok = false;
                break;
            }
            offset += nal_size;
        }
        if (offset != size) ok = false;
    } else if (nal_type == 28U && size >= 2U) { /* FU-A */
        const uint8_t fu_indicator = payload[0];
        const uint8_t fu_header = payload[1];
        const bool start = (fu_header & 0x80U) != 0U;
        const bool end = (fu_header & 0x40U) != 0U;
        if (start) {
            session->fu_active = true;
            session->fu_header = (uint8_t)((fu_indicator & 0xE0U) |
                                           (fu_header & 0x1FU));
            ok = append_bytes(session, START_CODE, sizeof(START_CODE)) &&
                 append_bytes(session, &session->fu_header, 1U) &&
                 append_bytes(session, payload + 2U, size - 2U);
        } else if (session->fu_active) {
            ok = append_bytes(session, payload + 2U, size - 2U);
        } else {
            ok = false;
        }
        if (end) session->fu_active = false;
    } else {
        ok = false;
    }
    if (!ok) {
        session->access_unit_size = 0U;
        session->fu_active = false;
        return false;
    }
    if (marker) {
        if (session->fu_active) {
            session->access_unit_size = 0U;
            session->fu_active = false;
            return false;
        }
        return decode_access_unit(session);
    }
    return true;
}

static bool consume_interleaved_packet(stream_session_t *session)
{
    uint8_t header[4] = {0};
    if (!recv_exact(session, header, sizeof(header), 5000U) || header[0] != '$')
        return false;
    const size_t length = ((size_t)header[2] << 8U) | header[3];
    if (length < 12U || length > RTP_PACKET_CAPACITY ||
        !recv_exact(session, session->packet, length, 5000U)) return false;
    if (header[1] != 0U) return true; /* RTCP channel */
    const uint8_t *const packet = session->packet;
    if ((packet[0] >> 6U) != 2U) return false;
    const size_t csrc_count = packet[0] & 0x0FU;
    size_t offset = 12U + csrc_count * 4U;
    if (offset > length) return false;
    if ((packet[0] & 0x10U) != 0U) {
        if (offset + 4U > length) return false;
        const size_t extension_size = ((size_t)packet[offset + 2U] << 8U) |
                                     packet[offset + 3U];
        offset += 4U + extension_size * 4U;
        if (offset > length) return false;
    }
    size_t payload_end = length;
    if ((packet[0] & 0x20U) != 0U) {
        const uint8_t padding = packet[length - 1U];
        if (padding == 0U || padding > payload_end - offset) return false;
        payload_end -= padding;
    }
    if (payload_end <= offset) return false;
    return handle_h264_payload(session, packet + offset, payload_end - offset,
                               (packet[1] & 0x80U) != 0U);
}
