#include "spotify_remote.h"
#include "spotify_cover.h"
#include "google_agenda_codec.h"
#include "network_validation_service.h"
#include "np_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#ifdef ESP_PLATFORM
#include "freertos/task.h"
#endif

#define BODY_CAP 8192U
typedef struct {
    spotify_remote_credentials_t credentials;
    google_json_t json;
    char body[BODY_CAP + 1U], form[1800], authorization[2060], url[512];
    char *cover_body; /* optional bounded SRAM buffer, only after TLS admission */
    size_t received;
    int status;
    int64_t deadline;
    uint32_t retry_s;
} http_work_t;
static http_work_t *s_http; /* sole worker, explicitly internal and transient */
static char s_access[2049]; /* private internal vault, never a projection */
static int64_t s_access_until;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static NP2_PSRAM_BSS spotify_remote_projection_t s_mailbox;
static spotify_remote_command_t s_job;
static bool s_busy, s_ready, s_visible, s_provisioning;
static uint64_t s_next_poll, s_not_before;
static uint32_t s_retry_s, s_auth_failure_version;
static char s_cover_url[96]; /* public image identity, no credentials */
static char s_cover_attempt[96];
static int64_t s_cover_retry_at;
static bool s_cover_low_quality, s_cover_rate_limited;

static void scrub(void *data, size_t n) { volatile uint8_t *p = data; while (n--) *p++ = 0; }
static bool field(int object, const char *name, char *out, size_t size)
{ return google_json_string(&s_http->json, google_json_field(&s_http->json, object, name), out, size); }
static bool flag(int object, const char *name)
{ return google_json_true(&s_http->json, google_json_field(&s_http->json, object, name)); }
static bool number(int object, const char *name, uint32_t *out)
{
    int t = google_json_field(&s_http->json, object, name);
    if (t < 0 || s_http->json.tokens[t].kind != 4U) return false;
    const google_json_token_t *v = &s_http->json.tokens[t];
    uint64_t n = 0;
    if (v->end == v->begin) return false;
    for (unsigned i = v->begin; i < v->end; ++i) {
        char c = s_http->json.text[i];
        if (c < '0' || c > '9' || (n = n * 10U + (unsigned)(c - '0')) > UINT32_MAX) return false;
    }
    *out = (uint32_t)n;
    return true;
}
static bool path_id(const char *id)
{
    size_t n = strnlen(id, 129U);
    return n > 0 && n < 129U && strspn(id, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.") == n;
}
static bool safe_header(const char *text)
{
    for (; *text; ++text) if ((unsigned char)*text < 33U || (unsigned char)*text > 126U) return false;
    return true;
}
static esp_err_t budget(esp_http_client_handle_t client)
{
    int64_t left = (s_http->deadline - esp_timer_get_time()) / 1000;
    return left <= 0 ? ESP_ERR_TIMEOUT : esp_http_client_set_timeout_ms(client, left > 8000 ? 8000 : (int)left);
}
static esp_err_t header_event(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_HEADER && e->header_key && e->header_value &&
        strcasecmp(e->header_key, "Retry-After") == 0) {
        const char *p = e->header_value;
        uint32_t n = 0;
        while (*p >= '0' && *p <= '9' && n <= 86400U) n = n * 10U + (unsigned)(*p++ - '0');
        if (*p == 0 && n <= 86400U && n > 0) s_http->retry_s = n;
    }
    return ESP_OK;
}
static esp_err_t request(const char *url, esp_http_client_method_t method, const char *body, bool token)
{
    bool cover = spotify_cover_url_allowed(url);
    size_t body_cap = cover ? SPOTIFY_COVER_MAX_JPEG : BODY_CAP;
    if (s_http->cover_body) { heap_caps_free(s_http->cover_body); s_http->cover_body = NULL; }
    char *response = s_http->body;
    s_http->received = 0; s_http->status = 0; s_http->body[0] = 0;
    esp_http_client_config_t config = {.url = url, .method = method,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
        .max_redirection_count = 0, .timeout_ms = 8000, .buffer_size = 512,
        .buffer_size_tx = 3072, .event_handler = header_event};
    esp_http_client_handle_t c = esp_http_client_init(&config);
    if (!c) return ESP_ERR_NO_MEM;
    esp_err_t r = esp_http_client_set_header(c, "Accept", cover ? "image/jpeg" : "application/json");
    if (r == ESP_OK && !token && !cover) r = esp_http_client_set_header(c, "Authorization", s_http->authorization);
    if (r == ESP_OK && body) r = esp_http_client_set_header(c, "Content-Type", token ? "application/x-www-form-urlencoded" : "application/json");
    size_t length = body ? strlen(body) : 0, sent = 0;
    if (r == ESP_OK) r = budget(c);
    if (r == ESP_OK) r = esp_http_client_open(c, (int)length);
    while (r == ESP_OK && sent < length) {
        r = budget(c); if (r != ESP_OK) break;
        int n = esp_http_client_write(c, body + sent, (int)(length - sent));
        if (n <= 0) r = ESP_FAIL; else sent += (size_t)n;
    }
    if (r == ESP_OK) r = budget(c);
    if (r == ESP_OK) {
        int64_t n = esp_http_client_fetch_headers(c);
        s_http->status = esp_http_client_get_status_code(c);
        if (n < 0) r = ESP_FAIL;
        else if ((uint64_t)n > body_cap) r = ESP_ERR_INVALID_SIZE;
        else if (cover) {
            if (n > 0) body_cap = (size_t)n;
            if (body_cap > BODY_CAP) {
                size_t bytes = body_cap + 1U;
                unsigned caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
                /* Artwork may use extra SRAM only after the actual TLS cost
                 * is known, while retaining the product's 80 KiB floor. */
                if (heap_caps_get_free_size(caps) < 80U*1024U + bytes ||
                    heap_caps_get_largest_free_block(caps) < bytes) r = ESP_ERR_NO_MEM;
                else {
                    s_http->cover_body = heap_caps_calloc(1U, bytes, caps);
                    if (!s_http->cover_body) r = ESP_ERR_NO_MEM;
                    else response = s_http->cover_body;
                }
            }
        }
    }
    while (r == ESP_OK) {
        r = budget(c); if (r != ESP_OK) break;
        char extra;
        bool full = s_http->received == body_cap;
        int n = esp_http_client_read(c, full ? &extra : response + s_http->received,
            full ? 1 : (int)(body_cap - s_http->received));
        if (n < 0) { r = ESP_FAIL; break; }
        if (n == 0) {
            if (!esp_http_client_is_complete_data_received(c)) r = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        if (full) { r = ESP_ERR_INVALID_SIZE; break; }
        s_http->received += (size_t)n;
        response[s_http->received] = 0;
    }
    esp_http_client_close(c); esp_http_client_cleanup(c);
    ESP_LOGI("np2_spotify_remote", "http token=%u status=%d result=%ld bytes=%u",
        (unsigned)token, s_http->status, (long)r, (unsigned)s_http->received);
    if (r != ESP_OK) return r;
    if (!cover && (s_http->status == 401 || (token && s_http->status == 400))) { scrub(s_access, sizeof(s_access)); s_access_until = 0; return SPOTIFY_REMOTE_AUTH_REQUIRED; }
    if (s_http->status == 403) return SPOTIFY_REMOTE_FORBIDDEN;
    if (s_http->status == 429) return SPOTIFY_REMOTE_RATE_LIMIT;
    return s_http->status == 200 || s_http->status == 202 || s_http->status == 204 ? ESP_OK :
        s_http->status == 404 ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_RESPONSE;
}
static bool form_encode(const char *in, char *out, size_t size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; ++p) {
        if (n + 3U >= size) return false;
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || strchr("-._~", *p)) out[n++] = (char)*p;
        else { out[n++] = '%'; out[n++] = hex[*p >> 4]; out[n++] = hex[*p & 15]; }
    }
    out[n] = 0; return true;
}
static esp_err_t authorize(void)
{
    esp_err_t r = spotify_remote_credentials_copy_private(&s_http->credentials);
    if (r != ESP_OK) return r;
    /* Reauthorization may replace the client/token; never reuse old access. */
    static uint32_t prior_version;
    uint32_t version = spotify_remote_credentials_version();
    if (prior_version != version) {
        scrub(s_access, sizeof(s_access)); s_access_until = 0; prior_version = spotify_remote_credentials_version();
    }
    if (esp_timer_get_time() >= s_access_until) {
        char *encoded = s_http->form + 240;
        if (!form_encode(s_http->credentials.refresh_token, encoded, sizeof(s_http->form) - 240U)) return ESP_ERR_INVALID_SIZE;
        int n = snprintf(s_http->form, 240, "grant_type=refresh_token&client_id=%s&refresh_token=", s_http->credentials.client_id);
        if (n < 0 || n >= 240) return ESP_ERR_INVALID_SIZE;
        memmove(s_http->form + n, encoded, strlen(encoded) + 1U);
        r = request("https://accounts.spotify.com/api/token", HTTP_METHOD_POST, s_http->form, true);
        scrub(s_http->form, sizeof(s_http->form));
        if (r != ESP_OK) return r;
        uint32_t seconds = 0;
        char type[16];
        if (!google_json_parse(&s_http->json, s_http->body, s_http->received) ||
            !field(0, "access_token", s_access, sizeof(s_access)) || !safe_header(s_access) || s_access[0] == 0 ||
            !field(0, "token_type", type, sizeof(type)) || strcmp(type, "Bearer") != 0 ||
            !number(0, "expires_in", &seconds) || seconds < 60U || seconds > 86400U) {
            scrub(s_access, sizeof(s_access)); return ESP_ERR_INVALID_RESPONSE;
        }
        int refresh = google_json_field(&s_http->json, 0, "refresh_token");
        if (refresh >= 0) {
            if (!google_json_string(&s_http->json, refresh, s_http->credentials.refresh_token, sizeof(s_http->credentials.refresh_token)) ||
                spotify_remote_credentials_rotate_private(&s_http->credentials) != ESP_OK) return ESP_ERR_INVALID_RESPONSE;
            prior_version = spotify_remote_credentials_version();
        }
        s_access_until = esp_timer_get_time() + (int64_t)(seconds - 30U) * 1000000;
        scrub(s_http->body, sizeof(s_http->body));
    }
    snprintf(s_http->authorization, sizeof(s_http->authorization), "Bearer %s", s_access);
    return ESP_OK;
}
static bool read_device(int d, spotify_remote_device_t *out)
{
    memset(out, 0, sizeof(*out));
    if (d < 0 || !field(d, "id", out->id, sizeof(out->id)) || !path_id(out->id) ||
        !field(d, "name", out->name, sizeof(out->name))) return false;
    out->active = flag(d, "is_active"); out->restricted = flag(d, "is_restricted");
    out->supports_volume = flag(d, "supports_volume"); return true;
}
static void forget_cover(spotify_remote_projection_t *p)
{
    s_cover_url[0] = 0; s_cover_low_quality = false;
    if (!s_cover_rate_limited || esp_timer_get_time() >= s_cover_retry_at) {
        s_cover_attempt[0] = 0; s_cover_retry_at = 0; s_cover_rate_limited = false;
    }
    p->cover_generation = spotify_cover_clear();
}
static void cover_for_item(spotify_remote_projection_t *p, int item)
{
    int album = google_json_field(&s_http->json, item, "album");
    int a = google_json_field(&s_http->json, album >= 0 ? album : item, "images");
    char selected[96] = {0}, fallback[96] = {0}, candidate[96];
    uint32_t best = 0, small = 0;
    if (a >= 0 && s_http->json.tokens[a].kind == 2U) {
        for (int image = a + 1; image < s_http->json.tokens[a].next; image = s_http->json.tokens[image].next) {
            uint32_t width, height;
            if (!number(image, "width", &width) || !number(image, "height", &height) ||
                !width || width > SPOTIFY_COVER_MAX_EDGE || !height || height > SPOTIFY_COVER_MAX_EDGE ||
                !field(image, "url", candidate, sizeof(candidate)) || !spotify_cover_url_allowed(candidate)) continue;
            uint32_t area = width * height;
            if (area > best) { best = area; strcpy(selected, candidate); }
            if (width <= 64 && height <= 64 && area > small) { small = area; strcpy(fallback, candidate); }
        }
    }
    if (!selected[0]) { forget_cover(p); return; }
    bool same = selected[0] && strcmp(selected, s_cover_url) == 0;
    if (same && (!s_cover_low_quality || esp_timer_get_time() < s_cover_retry_at)) return;
    if (selected[0] && strcmp(selected, s_cover_attempt) == 0 && esp_timer_get_time() < s_cover_retry_at) return;
    if (!same) {
        s_cover_url[0] = 0; s_cover_low_quality = false;
        p->cover_generation = spotify_cover_clear();
    }
    /* Artwork is optional and may consume only the remaining job budget. */
    if (!selected[0] || s_http->deadline - esp_timer_get_time() < 5000000) return;
    strcpy(s_cover_attempt, selected);
    s_cover_retry_at = esp_timer_get_time() + 60000000;
    esp_err_t result = request(selected, HTTP_METHOD_GET, NULL, false);
    bool decoded = result == ESP_OK && s_http->status == 200 &&
        spotify_cover_decode((const uint8_t *)(s_http->cover_body ? s_http->cover_body : s_http->body),
                             s_http->received, &p->cover_generation);
    bool used_fallback = false;
    if (!decoded && s_http->status != 429 && !same && fallback[0] && strcmp(fallback, selected) &&
        s_http->deadline - esp_timer_get_time() >= 3000000) {
        used_fallback = true;
        result = request(fallback, HTTP_METHOD_GET, NULL, false);
        decoded = result == ESP_OK && s_http->status == 200 &&
            spotify_cover_decode((const uint8_t *)(s_http->cover_body ? s_http->cover_body : s_http->body),
                                 s_http->received, &p->cover_generation);
    }
    if (decoded) {
        strcpy(s_cover_url, selected);
        s_cover_low_quality = used_fallback;
        if (!used_fallback) { s_cover_attempt[0] = 0; s_cover_retry_at = 0; }
        int8_t lease = -1;
        const spotify_cover_t *image = spotify_cover_acquire(p->cover_generation, &lease);
        if (image) ESP_LOGI("np2_spotify_remote", "cover width=%u height=%u bytes=%u fallback=%u",
            (unsigned)image->width, (unsigned)image->height, (unsigned)s_http->received, (unsigned)used_fallback);
        spotify_cover_release(lease);
    }
    if (s_http->cover_body) { heap_caps_free(s_http->cover_body); s_http->cover_body = NULL; }

    s_cover_rate_limited = s_http->status == 429;
    if (s_cover_rate_limited && s_http->retry_s > 60U)
        s_cover_retry_at = esp_timer_get_time() + (int64_t)s_http->retry_s * 1000000;
}
static esp_err_t playback(spotify_remote_projection_t *p)
{
    esp_err_t r = request("https://api.spotify.com/v1/me/player?additional_types=track,episode", HTTP_METHOD_GET, NULL, false);
    if (r != ESP_OK) return r;
    p->sampled_ms = (uint64_t)esp_timer_get_time() / 1000U;
    p->available = false; p->playing = false; p->restricted = true; p->supports_volume = false;
    p->title[0] = p->artist[0] = p->device_name[0] = p->device_id[0] = 0;
    p->progress_ms = p->duration_ms = 0;
    if (s_http->status == 204) { forget_cover(p); return ESP_OK; }
    if (!google_json_parse(&s_http->json, s_http->body, s_http->received)) return ESP_ERR_INVALID_RESPONSE;
    spotify_remote_device_t d;
    int device = google_json_field(&s_http->json, 0, "device");
    if (read_device(device, &d)) {
        strcpy(p->device_name, d.name); strcpy(p->device_id, d.id);
        p->restricted = d.restricted; p->supports_volume = d.supports_volume;
        uint32_t volume;
        if (number(device, "volume_percent", &volume) && volume <= 100U) p->volume = (uint8_t)volume;
        else p->supports_volume = false;
    }
    int item = google_json_field(&s_http->json, 0, "item");
    if (item < 0 || !field(item, "name", p->title, sizeof(p->title))) {
        forget_cover(p); return ESP_OK;
    }
    p->available = p->device_id[0] != 0; p->playing = flag(0, "is_playing");
    number(0, "progress_ms", &p->progress_ms); number(item, "duration_ms", &p->duration_ms);
    int artists = google_json_field(&s_http->json, item, "artists");
    if (artists >= 0 && s_http->json.tokens[artists].kind == 2U && artists + 1 < s_http->json.tokens[artists].next)
        field(artists + 1, "name", p->artist, sizeof(p->artist));
    else {
        int show = google_json_field(&s_http->json, item, "show");
        if (show >= 0) field(show, "name", p->artist, sizeof(p->artist));
    }
    cover_for_item(p, item);
    return ESP_OK;
}
static esp_err_t devices(spotify_remote_projection_t *p)
{
    esp_err_t r = request("https://api.spotify.com/v1/me/player/devices", HTTP_METHOD_GET, NULL, false);
    if (r != ESP_OK) return r;
    if (!google_json_parse(&s_http->json, s_http->body, s_http->received)) return ESP_ERR_INVALID_RESPONSE;
    int a = google_json_field(&s_http->json, 0, "devices");
    if (a < 0 || s_http->json.tokens[a].kind != 2U) return ESP_ERR_INVALID_RESPONSE;
    p->device_count = 0; memset(p->devices, 0, sizeof(p->devices));
    for (int d = a + 1; d < s_http->json.tokens[a].next && p->device_count < SPOTIFY_REMOTE_DEVICES; d = s_http->json.tokens[d].next) {
        if (read_device(d, &p->devices[p->device_count])) ++p->device_count;
    }
    return ESP_OK;
}
static esp_err_t execute(spotify_remote_projection_t *p, const spotify_remote_command_t *job)
{
    esp_err_t r = authorize(); if (r != ESP_OK) return r;
    if (job->action == SPOTIFY_REMOTE_REFRESH) return playback(p);
    if (job->action == SPOTIFY_REMOTE_DEVICES_REQUEST) return devices(p);
    esp_http_client_method_t method = HTTP_METHOD_PUT;
    const char *suffix = "", *body = NULL;
    char payload[192];
    switch (job->action) {
    case SPOTIFY_REMOTE_PLAY: suffix = "/play"; break;
    case SPOTIFY_REMOTE_PAUSE: suffix = "/pause"; break;
    case SPOTIFY_REMOTE_NEXT: suffix = "/next"; method = HTTP_METHOD_POST; break;
    case SPOTIFY_REMOTE_PREVIOUS: suffix = "/previous"; method = HTTP_METHOD_POST; break;
    case SPOTIFY_REMOTE_VOLUME:
        snprintf(payload, sizeof(payload), "/volume?volume_percent=%u", (unsigned)job->value); suffix = payload; break;
    case SPOTIFY_REMOTE_SEEK:
        snprintf(payload, sizeof(payload), "/seek?position_ms=%u", (unsigned)job->value); suffix = payload; break;
    case SPOTIFY_REMOTE_TRANSFER:
        if (job->value >= p->device_count || !path_id(p->devices[job->value].id) || p->devices[job->value].restricted) return ESP_ERR_INVALID_ARG;
        snprintf(payload, sizeof(payload), "{\"device_ids\":[\"%s\"],\"play\":false}", p->devices[job->value].id);
        body = payload; break;
    default: return ESP_ERR_INVALID_ARG;
    }
    /* Explicitly bind normal controls to the sampled device; never hit a new
     * active device silently after somebody transfers playback elsewhere. */
    bool transfer = job->action == SPOTIFY_REMOTE_TRANSFER;
    if (!transfer && (!p->available || p->restricted || !path_id(p->device_id))) return ESP_ERR_NOT_FOUND;
    snprintf(s_http->url, sizeof(s_http->url), "https://api.spotify.com/v1/me/player%s%s%s", suffix,
        transfer ? "" : strchr(suffix, '?') ? "&device_id=" : "?device_id=", transfer ? "" : p->device_id);
    esp_err_t result = request(s_http->url, method, body, false);
    if (result == ESP_OK && transfer) {
        /* Do not let the next tap address the old output while the confirmed
         * transfer is waiting for its first playback-state refresh. */
        p->available = false;
        p->playing = false;
        p->device_id[0] = 0;
        strcpy(p->device_name, p->devices[job->value].name);
    }
    return result;
}
esp_err_t spotify_remote_process(esp_err_t prerequisite)
{
    esp_err_t r = prerequisite;
#ifdef ESP_PLATFORM
    bool monitoring = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
#endif
    size_t before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI("np2_spotify_remote", "admission free=%u required=%u", (unsigned)before,
        (unsigned)(sizeof(http_work_t) + 80U * 1024U + 64U * 1024U));
    if (r == ESP_OK && (before < sizeof(http_work_t) + 80U * 1024U + 64U * 1024U ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < sizeof(http_work_t))) r = ESP_ERR_NO_MEM;
    if (r == ESP_OK) {
        s_http = heap_caps_calloc(1U, sizeof(*s_http), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!s_http) r = ESP_ERR_NO_MEM;
    }
    if (r == ESP_OK) {
        s_http->deadline = esp_timer_get_time() + 18000000;
        r = execute(&s_mailbox, &s_job);
    }
    uint32_t retry = s_http ? s_http->retry_s : 0;
    if (s_http) { if (s_http->cover_body) heap_caps_free(s_http->cover_body); scrub(s_http, sizeof(*s_http)); heap_caps_free(s_http); s_http = NULL; }
#ifdef ESP_PLATFORM
    size_t minimum = monitoring ? heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) : 0;
    if (monitoring) heap_caps_monitor_local_minimum_free_size_stop();
    ESP_LOGI("np2_spotify_remote", "memory min_internal=%u largest=%u stack_min_bytes=%u integrity=%u",
        (unsigned)minimum, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)heap_caps_check_integrity_all(false));
    if (monitoring && minimum < 80U * 1024U) r = ESP_ERR_NO_MEM;
#endif
    portENTER_CRITICAL(&s_lock);
    s_mailbox.result = r;
    s_retry_s = retry; s_ready = true; s_busy = false;
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI("np2_spotify_remote", "complete action=%u result=%ld internal=%u", (unsigned)s_job.action, (long)r,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    return r;
}
static esp_err_t submit(spotify_remote_projection_t *p, const spotify_remote_command_t *job, bool interactive)
{
    portENTER_CRITICAL(&s_lock);
    if (s_busy || s_ready || s_provisioning) { portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE; }
    s_mailbox = *p; s_job = *job; s_busy = true;
    portEXIT_CRITICAL(&s_lock);
    esp_err_t r = interactive ? network_validation_service_request_spotify_interactive()
                              : network_validation_service_request_spotify_remote();
    if (r != ESP_OK) { portENTER_CRITICAL(&s_lock); s_busy = false; portEXIT_CRITICAL(&s_lock); }
    else p->busy = true;
    return r;
}
void spotify_remote_command(spotify_remote_projection_t *p, const spotify_remote_command_t *cmd)
{
    if (cmd->action == SPOTIFY_REMOTE_VISIBLE || cmd->action == SPOTIFY_REMOTE_HIDDEN) {
        s_visible = cmd->action == SPOTIFY_REMOTE_VISIBLE; s_next_poll = 0; return;
    }
    uint64_t now = (uint64_t)esp_timer_get_time() / 1000U;
    if (!p->linked || p->auth_required || p->busy || !spotify_remote_credentials_settled() || now < s_not_before) { p->result = ESP_ERR_INVALID_STATE; ++p->completion; return; }
    if ((cmd->action == SPOTIFY_REMOTE_TRANSFER && cmd->revision != p->revision) ||
        (cmd->action == SPOTIFY_REMOTE_VOLUME && (cmd->value > 100U || !p->supports_volume)) ||
        (cmd->action == SPOTIFY_REMOTE_SEEK && cmd->value > p->duration_ms)) { p->result = ESP_ERR_INVALID_ARG; ++p->completion; return; }
    p->result = submit(p, cmd, true); ++p->completion;
    if (p->result == ESP_OK) s_next_poll = now + 5000U;
}
void spotify_remote_tick(spotify_remote_projection_t *p, bool online, bool trusted, uint64_t now)
{
    spotify_remote_credentials_tick();
    p->linked = spotify_remote_linked();
    portENTER_CRITICAL(&s_lock);
    bool ready = s_ready;
    if (ready) {
        uint32_t completion = p->completion, revision = p->revision;
        *p = s_mailbox; p->completion = completion + 1U; p->revision = revision + 1U;
        p->busy = false; s_ready = false;
        uint32_t wait_s = p->result == SPOTIFY_REMOTE_RATE_LIMIT ? (s_retry_s ? s_retry_s : 60U) : p->result == ESP_OK ? 0U : 30U;
        s_not_before = now + (uint64_t)wait_s * 1000U;
    }
    portEXIT_CRITICAL(&s_lock);
    if (ready) {
        p->auth_required = p->result == SPOTIFY_REMOTE_AUTH_REQUIRED;
        if (p->auth_required) s_auth_failure_version = spotify_remote_credentials_version();
        /* A newly provisioned credential allows a later explicit refresh. */
        s_next_poll = now + (p->result == ESP_OK && s_job.action != SPOTIFY_REMOTE_REFRESH &&
            s_job.action != SPOTIFY_REMOTE_DEVICES_REQUEST ? 1000U : 5000U);
    }
    if (p->auth_required && spotify_remote_credentials_version() != s_auth_failure_version) {
        p->auth_required = false; s_next_poll = 0; s_not_before = 0;
    }
    if (!online || !trusted || !p->linked || p->auth_required || p->busy || !s_visible ||
        !spotify_remote_credentials_settled() || now < s_next_poll || now < s_not_before) return;
    spotify_remote_command_t job = {.action = SPOTIFY_REMOTE_REFRESH};
    esp_err_t r = submit(p, &job, false);
    s_next_poll = now + (r == ESP_OK ? 5000U : 2000U);
}
bool spotify_remote_provision_begin(void)
{
    if (!spotify_remote_credentials_settled()) return false;
    portENTER_CRITICAL(&s_lock);
    bool allowed = !s_busy && !s_ready && !s_provisioning;
    if (allowed) s_provisioning = true;
    portEXIT_CRITICAL(&s_lock);
    return allowed;
}
void spotify_remote_provision_end(void)
{ portENTER_CRITICAL(&s_lock); s_provisioning = false; portEXIT_CRITICAL(&s_lock); }
