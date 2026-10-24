#include "google_agenda_sync.h"
#include "google_agenda_codec.h"
#include "agenda_time.h"
#include "network_validation_service.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "freertos/task.h"
#endif

#define BODY_LIMIT 12288U
#define GOOGLE_CONFLICT (ESP_ERR_INVALID_RESPONSE + 100)
#define GOOGLE_INTERNAL_FLOOR_BYTES (80U * 1024U)
/* Bench total peak was 51,956 B, including the 22,408 B HTTP workspace.
 * Keep >=19 KiB above that remaining peak, plus a separate OAuth form budget. */
#define GOOGLE_TLS_ENVELOPE_BYTES (48U * 1024U)
typedef struct {
    agenda_data_t data;
    agenda_data_t before_list; /* One bounded PSRAM rollback snapshot per list. */
    bool listing;
    bool busy, result_ready;
    esp_err_t result;
    uint32_t month_from, month_until;
} sync_workspace_t;
static sync_workspace_t *s_work;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
typedef struct {
    google_agenda_credentials_t credentials;
    char access[2049];
    char authorization[2060];
    char outgoing[1024]; /* Keep write payloads off the TLS caller stack. */
    char body[BODY_LIMIT + 1U];
    google_json_t json;
    char page_token[257], next_token[257], page_encoded[769], list_url[1536];
    size_t received;
    int64_t deadline_us;
} http_workspace_t;
static http_workspace_t *s_http; /* allocated internally only while sole worker runs */
static char s_device[13];
extern esp_err_t google_agenda_credentials_copy_private(google_agenda_credentials_t *out);

static void scrub(void *data, size_t n) { volatile uint8_t *p = data; while (n-- > 0U) *p++ = 0U; }
static bool text_field(const google_json_t *doc, int object, const char *name, char *out, size_t n)
{ return google_json_string(doc, google_json_field(doc, object, name), out, n); }

static bool path_id(const char *s)
{
    if (s == NULL || s[0] == 0) return false;
    for (; *s; ++s) if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
        (*s >= '0' && *s <= '9') || *s == '-' || *s == '_')) return false;
    return true;
}

esp_err_t google_agenda_request_sync(const agenda_data_t *data)
{
    if (!agenda_data_valid(data) || !google_agenda_linked()) return ESP_ERR_INVALID_STATE;
    if (s_work == NULL) {
        s_work = heap_caps_calloc(1U, sizeof(*s_work), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_work == NULL) return ESP_ERR_NO_MEM;
    }
    if (s_device[0] == 0) {
        uint8_t mac[6];
        if (esp_efuse_mac_get_default(mac) != ESP_OK) return ESP_FAIL;
        snprintf(s_device, sizeof(s_device), "%02x%02x%02x%02x%02x%02x", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    }
    portENTER_CRITICAL(&s_lock);
    if (s_work->busy || s_work->result_ready) { portEXIT_CRITICAL(&s_lock); return ESP_ERR_INVALID_STATE; }
    s_work->data = *data;
    s_work->busy = true;
    portEXIT_CRITICAL(&s_lock);
    esp_err_t result = network_validation_service_request_google_sync();
    if (result != ESP_OK) {
        portENTER_CRITICAL(&s_lock); s_work->busy = false; portEXIT_CRITICAL(&s_lock);
    }
    return result;
}

bool google_agenda_take_result(agenda_data_t *out_data, esp_err_t *out_result, bool *out_changed)
{
    if (out_data == NULL || out_result == NULL || s_work == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    bool ready = s_work->result_ready;
    if (ready) {
        const bool valid = agenda_data_valid(&s_work->data);
        if (out_changed != NULL) *out_changed = valid && memcmp(out_data, &s_work->data, sizeof(*out_data)) != 0;
        if (valid) *out_data = s_work->data;
        *out_result = valid ? s_work->result : ESP_ERR_INVALID_RESPONSE;
        s_work->result_ready = false;
    }
    portEXIT_CRITICAL(&s_lock);
    return ready;
}
void google_agenda_fail_sync(esp_err_t result)
{
    if (s_work == NULL) return;
    portENTER_CRITICAL(&s_lock);
    if (s_work->busy) { s_work->result = result; s_work->busy = false; s_work->result_ready = true; }
    portEXIT_CRITICAL(&s_lock);
}

static esp_err_t set_budget(esp_http_client_handle_t client)
{
    const int64_t remaining = s_http->deadline_us - esp_timer_get_time();
    if (remaining <= 0) return ESP_ERR_TIMEOUT;
    return esp_http_client_set_timeout_ms(client, remaining / 1000 > 10000
        ? 10000 : (int)(remaining / 1000 + 1));
}

static esp_err_t request(const char *url, esp_http_client_method_t method,
                          const char *body, const char *etag, bool token, int *status)
{
    /* Stream explicitly: HTTP ON_DATA return values do not cancel a perform
     * operation in IDF. Every read/write gets the remaining deadline and the
     * first excess body byte closes the connection. */
    *status = 0;
    if (esp_timer_get_time() >= s_http->deadline_us) return ESP_ERR_TIMEOUT;
    s_http->received = 0U;
    scrub(s_http->body, sizeof(s_http->body));
    esp_http_client_config_t config = {.url = url, .method = method,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
        .max_redirection_count = 0, .timeout_ms = 10000,
        .buffer_size = 512, .buffer_size_tx = 3072};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_set_header(client, "Accept", "application/json");
    if (result == ESP_OK && !token)
        result = esp_http_client_set_header(client, "Authorization", s_http->authorization);
    if (result == ESP_OK && etag != NULL && etag[0] != 0) {
        if (strchr(etag, '\r') != NULL || strchr(etag, '\n') != NULL)
            result = ESP_ERR_INVALID_RESPONSE;
        else result = esp_http_client_set_header(client, "If-Match", etag);
    }
    const size_t body_length = body == NULL ? 0U : strlen(body);
    if (result == ESP_OK && body != NULL)
        result = esp_http_client_set_header(client, "Content-Type", token
            ? "application/x-www-form-urlencoded" : "application/json");
    if (result == ESP_OK) result = set_budget(client);
    if (result == ESP_OK) result = esp_http_client_open(client, (int)body_length);
    size_t sent = 0U;
    while (result == ESP_OK && sent < body_length) {
        result = set_budget(client);
        if (result != ESP_OK) break;
        const int count = esp_http_client_write(client, body + sent, (int)(body_length - sent));
        if (count <= 0) result = ESP_FAIL;
        else sent += (size_t)count;
    }
    if (result == ESP_OK) result = set_budget(client);
    if (result == ESP_OK) {
        const int64_t content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) result = ESP_FAIL;
        else if ((uint64_t)content_length > BODY_LIMIT) result = ESP_ERR_INVALID_SIZE;
        *status = esp_http_client_get_status_code(client);
    }
    while (result == ESP_OK) {
        result = set_budget(client);
        if (result != ESP_OK) break;
        char extra;
        const bool at_limit = s_http->received == BODY_LIMIT;
        char *target = at_limit ? &extra : s_http->body + s_http->received;
        const int capacity = at_limit ? 1 : (int)(BODY_LIMIT - s_http->received);
        const int count = esp_http_client_read(client, target, capacity);
        if (count < 0) { result = ESP_FAIL; break; }
        if (count > 0) {
            if (at_limit) { result = ESP_ERR_INVALID_SIZE; break; }
            s_http->received += (size_t)count;
            s_http->body[s_http->received] = 0;
        } else {
            if (!esp_http_client_is_complete_data_received(client)) result = ESP_ERR_INVALID_RESPONSE;
            break;
        }
    }
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);
#ifdef ESP_PLATFORM
    const char *endpoint = token ? "token" : strstr(url, "/tasks/") != NULL ? "tasks" : "calendar";
    ESP_LOGI("np2_google", "http endpoint=%s method=%d status=%d result=%ld bytes=%u",
        endpoint, (int)method, *status, (long)result, (unsigned)s_http->received);
#endif
    if (esp_timer_get_time() >= s_http->deadline_us) return ESP_ERR_TIMEOUT;
    if (result == ESP_OK && *status == 401) return AGENDA_SYNC_AUTH_REQUIRED;
    return result;
}

static bool encode_form(const char *text, char *out, size_t size)
{
    size_t used = 0U;
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (used + 3U >= size) return false;
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || strchr("-._~", *p)) out[used++] = (char)*p;
        else { out[used++] = '%'; out[used++] = hex[*p >> 4U]; out[used++] = hex[*p & 15U]; }
    }
    out[used] = 0; return true;
}

static esp_err_t authorize(void)
{
    esp_err_t result = google_agenda_credentials_copy_private(&s_http->credentials);
    if (result != ESP_OK) return result;
    /* Explicit internal transient allocations; credentials never enter PSRAM. */
    char *form = heap_caps_calloc(1U, 4096U, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (form == NULL) return ESP_ERR_NO_MEM;
    char *encoded = form + 2400U;
    size_t used = 0U;
    const char *keys[] = {"client_id", "client_secret", "refresh_token"};
    const char *values[] = {s_http->credentials.client_id, s_http->credentials.client_secret, s_http->credentials.refresh_token};
    for (unsigned i = 0U; i < 3U; ++i) {
        if (!encode_form(values[i], encoded, 1696U)) { result = ESP_ERR_INVALID_SIZE; break; }
        const size_t key_length = strlen(keys[i]), encoded_length = strlen(encoded);
        if (key_length + encoded_length + 3U >= 2400U - used) { result = ESP_ERR_INVALID_SIZE; break; }
        if (i != 0U) form[used++] = '&';
        memcpy(form + used, keys[i], key_length); used += key_length;
        form[used++] = '=';
        memcpy(form + used, encoded, encoded_length); used += encoded_length;
        form[used] = 0;
    }
    if (result == ESP_OK) {
        int n = snprintf(form + used, 2400U - used, "&grant_type=refresh_token");
        if (n < 0 || (size_t)n >= 2400U - used) result = ESP_ERR_INVALID_SIZE;
    }
    int status = 0;
    if (result == ESP_OK) result = request("https://oauth2.googleapis.com/token", HTTP_METHOD_POST, form, NULL, true, &status);
    scrub(form, 4096U); heap_caps_free(form);
    if (result != ESP_OK) return result;
    if (status != 200) {
        char error[32]={0};
        const bool rejected=status == 400 &&
            google_json_parse(&s_http->json,s_http->body,s_http->received) &&
            text_field(&s_http->json,0,"error",error,sizeof(error)) && !strcmp(error,"invalid_grant");
        scrub(error,sizeof(error));
        return rejected ? AGENDA_SYNC_AUTH_REQUIRED : ESP_ERR_INVALID_STATE;
    }
    if (!google_json_parse(&s_http->json, s_http->body, s_http->received) ||
        !text_field(&s_http->json, 0, "access_token", s_http->access, sizeof(s_http->access))) return ESP_ERR_INVALID_RESPONSE;
    for (const char *p = s_http->access; *p; ++p) if ((unsigned char)*p <= 32U || (unsigned char)*p >= 127U) return ESP_ERR_INVALID_RESPONSE;
    snprintf(s_http->authorization, sizeof(s_http->authorization), "Bearer %s", s_http->access);
    return ESP_OK;
}

static void remove_item(agenda_data_t *data, uint8_t i)
{
    --data->count;
    memmove(&data->items[i], &data->items[i+1U], (data->count-i)*sizeof(data->items[0]));
    memmove(&data->remote[i], &data->remote[i+1U], (data->count-i)*sizeof(data->remote[0]));
    memset(&data->items[data->count], 0, sizeof(data->items[0]));
    memset(&data->remote[data->count], 0, sizeof(data->remote[0]));
}

static bool due_date(const char *text, unsigned hour, uint32_t *out)
{
    if (text == NULL || strlen(text) < 10U) return false;
    char utc[21];
    snprintf(utc, sizeof(utc), "%.10sT09:00:00Z", text);
    uint32_t check;
    if (!google_agenda_parse_time(utc, &check)) return false;
    time_t t = (time_t)check;
    struct tm date = {0};
    if (gmtime_r(&t, &date) == NULL) return false;
    date.tm_hour = (int)hour;
    date.tm_isdst = -1;
    t = mktime(&date);
    if (t < (time_t)AGENDA_MIN_EPOCH || (uint64_t)t > UINT32_MAX) return false;
    *out = (uint32_t)t; return true;
}

static esp_err_t merge_remote(int object, agenda_kind_t kind)
{
    google_json_t *doc = &s_http->json;
    agenda_item_t incoming = {.kind = (uint8_t)kind, .reminder_minutes = AGENDA_REMINDER_NONE};
    agenda_remote_t remote = {0};
    if (!text_field(doc, object, "id", remote.id, sizeof(remote.id)) || !path_id(remote.id) ||
        !text_field(doc, object, "etag", remote.etag, sizeof(remote.etag))) return ESP_ERR_INVALID_RESPONSE;
    agenda_data_t *data = &s_work->data;
    uint8_t index = data->count;
    for (uint8_t i = 0U; i < data->count; ++i) {
        if (data->items[i].kind == kind && strcmp(data->remote[i].id, remote.id) == 0) { index = i; break; }
        if (data->items[i].kind != kind || data->remote[i].id[0] != 0) continue;
        char marker[64];
        if (kind == AGENDA_EVENT) {
            snprintf(marker, sizeof(marker), "np2%s%08lx%08lx", s_device, (unsigned long)data->instance_id, (unsigned long)data->items[i].id);
            if (strcmp(marker, remote.id) == 0) { index = i; break; }
        } else {
            char notes[128];
            snprintf(marker, sizeof(marker), "NP2:%s:%08lx:%08lx", s_device, (unsigned long)data->instance_id, (unsigned long)data->items[i].id);
            if (text_field(doc, object, "notes", notes, sizeof(notes)) && strcmp(notes, marker) == 0) { index = i; break; }
        }
    }
    if (index == data->count && s_work->listing) {
        /* Restore metadata only for records observed in this complete-list
         * attempt. This makes room for replacements even when the cache is full. */
        for (uint8_t i = 0U; i < s_work->before_list.count; ++i) {
            if (s_work->before_list.items[i].kind != kind || strcmp(s_work->before_list.remote[i].id, remote.id)) continue;
            if (data->count >= AGENDA_MAX_ITEMS) return ESP_ERR_INVALID_SIZE;
            data->items[index] = s_work->before_list.items[i];
            data->remote[index] = s_work->before_list.remote[i];
            ++data->count; break;
        }
    }
    char status[32] = {0};
    (void)text_field(doc, object, "status", status, sizeof(status));
    const bool deleted = google_json_true(doc, google_json_field(doc, object, "deleted")) || strcmp(status, "cancelled") == 0;
    if (index < data->count && data->remote[index].dirty) {
        if (data->remote[index].reserved[0]) return GOOGLE_CONFLICT;
        if (data->remote[index].id[0] == 0) { /* Recover an acknowledged-by-server, interrupted create. */
            strcpy(data->remote[index].id, remote.id); strcpy(data->remote[index].etag, remote.etag);
            return ESP_OK;
        }
        if (deleted && data->remote[index].deleted) { remove_item(data, index); return ESP_OK; }
        if (deleted || strcmp(data->remote[index].etag, remote.etag) != 0) {
            strcpy(data->remote[index].etag, remote.etag);
            data->remote[index].reserved[0] = 1U;
            data->remote[index].reserved[1] = deleted ? 1U : 0U;
            return GOOGLE_CONFLICT;
        }
        return ESP_OK;
    }
    if (deleted) { if (index < data->count) remove_item(data, index); return ESP_OK; }
    if (!text_field(doc, object, kind == AGENDA_EVENT ? "summary" : "title", incoming.title, sizeof(incoming.title)))
        return ESP_ERR_INVALID_SIZE;
    char date[40];
    if (kind == AGENDA_TASK) {
        incoming.completed = strcmp(status, "completed") == 0;
        if (text_field(doc, object, "due", date, sizeof(date)) && !due_date(date, 9U, &incoming.start_unix_s)) return ESP_ERR_INVALID_RESPONSE;
        if (index < data->count && incoming.start_unix_s != 0U) {
            time_t old = (time_t)data->items[index].start_unix_s, newer = (time_t)incoming.start_unix_s;
            struct tm a = {0}, b = {0}; localtime_r(&old, &a); localtime_r(&newer, &b);
            if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) incoming.start_unix_s = data->items[index].start_unix_s;
            incoming.reminder_minutes = data->items[index].reminder_minutes;
        }
    } else {
        int start = google_json_field(doc, object, "start"), end = google_json_field(doc, object, "end");
        if (text_field(doc, start, "dateTime", date, sizeof(date))) {
            if (!google_agenda_parse_time(date, &incoming.start_unix_s) ||
                !text_field(doc, end, "dateTime", date, sizeof(date)) ||
                !google_agenda_parse_time(date, &incoming.end_unix_s)) return ESP_ERR_INVALID_RESPONSE;
        } else {
            /* All-day events are visible and read-only in the first editor. */
            if (!text_field(doc, start, "date", date, sizeof(date)) || !due_date(date, 0U, &incoming.start_unix_s) ||
                !text_field(doc, end, "date", date, sizeof(date)) || !due_date(date, 0U, &incoming.end_unix_s)) return ESP_ERR_INVALID_RESPONSE;
            incoming.reserved[0] = 1U;
        }
        int reminders = google_json_field(doc, object, "reminders");
        int overrides = google_json_field(doc, reminders, "overrides");
        if (overrides >= 0) {
            for (int entry = overrides + 1; entry < doc->tokens[overrides].next; entry = doc->tokens[entry].next) {
                int minutes = google_json_field(doc, entry, "minutes");
                if (minutes >= 0) {
                    unsigned value = 0U;
                    const google_json_token_t *t = &doc->tokens[minutes];
                    bool valid = t->end > t->begin;
                    for (uint16_t p = t->begin; p < t->end; ++p) {
                        if (doc->text[p] < '0' || doc->text[p] > '9' || value > 10080U) { valid = false; break; }
                        value = value * 10U + (unsigned)(doc->text[p] - '0');
                    }
                    if (valid && value <= 10080U && value < incoming.reminder_minutes) incoming.reminder_minutes = (uint16_t)value;
                }
            }
        }
    }
    if (!agenda_item_valid(&incoming)) return ESP_ERR_INVALID_RESPONSE;
    if (index == data->count) {
        if (data->count >= AGENDA_MAX_ITEMS || data->next_id == UINT32_MAX) return ESP_ERR_INVALID_SIZE;
        incoming.id = data->next_id++; incoming.revision = 1U; ++data->count;
    } else {
        incoming.id = data->items[index].id;
        incoming.revision = data->items[index].revision;
        if (incoming.start_unix_s == data->items[index].start_unix_s && incoming.reminder_minutes == data->items[index].reminder_minutes)
            incoming.reminded = data->items[index].reminded;
        if (memcmp(&incoming, &data->items[index], sizeof(incoming)) != 0) {
            if (incoming.revision == UINT32_MAX) return ESP_ERR_INVALID_STATE;
            ++incoming.revision;
        }
    }
    data->items[index] = incoming; data->remote[index] = remote;
    return ESP_OK;
}

static const char *base(agenda_kind_t kind)
{ return kind == AGENDA_EVENT ? "https://www.googleapis.com/calendar/v3/calendars/primary/events" : "https://tasks.googleapis.com/tasks/v1/lists/@default/tasks"; }

static esp_err_t pull_pages(agenda_kind_t kind, const char *query)
{
    s_http->page_token[0] = 0;
    for (unsigned page = 0U; page <= AGENDA_MAX_ITEMS; ++page) {
        if (!encode_form(s_http->page_token, s_http->page_encoded, sizeof(s_http->page_encoded))) return ESP_ERR_INVALID_SIZE;
        const int length = snprintf(s_http->list_url, sizeof(s_http->list_url), "%s?%s%s%s", base(kind), query,
            s_http->page_token[0] ? "&pageToken=" : "", s_http->page_encoded);
        if (length < 0 || (size_t)length >= sizeof(s_http->list_url)) return ESP_ERR_INVALID_SIZE;
        int status = 0;
        esp_err_t result = request(s_http->list_url, HTTP_METHOD_GET, NULL, NULL, false, &status);
        if (result != ESP_OK) return result;
        if (status != 200 || !google_json_parse(&s_http->json, s_http->body, s_http->received)) return ESP_ERR_INVALID_RESPONSE;
        const int next = google_json_field(&s_http->json, 0, "nextPageToken");
        s_http->next_token[0] = 0;
        if (next >= 0 && !google_json_string(&s_http->json, next, s_http->next_token, sizeof(s_http->next_token)))
            return ESP_ERR_INVALID_SIZE;
        if (next >= 0 && s_http->next_token[0] && !strcmp(s_http->next_token, s_http->page_token))
            return ESP_ERR_INVALID_RESPONSE;
        int array = google_json_field(&s_http->json, 0, "items");
        if (array >= 0) {
            if (s_http->json.tokens[array].kind != 2U) return ESP_ERR_INVALID_RESPONSE;
            for (int entry = array + 1; entry < s_http->json.tokens[array].next; entry = s_http->json.tokens[entry].next) {
                result = merge_remote(entry, kind);
                if (result != ESP_OK) return result;
            }
        }
        if (s_http->next_token[0]) { strcpy(s_http->page_token, s_http->next_token); continue; }
        return ESP_OK;
    }
    return ESP_ERR_INVALID_SIZE;
}

static esp_err_t pull_list(agenda_kind_t kind, const char *query)
{
    agenda_data_t *data = &s_work->data;
    s_work->before_list = *data;
    s_work->listing = true;
    for (uint8_t i = 0U; i < data->count;) {
        const agenda_item_t *item = &data->items[i];
        const bool covered = kind == AGENDA_TASK ||
            (item->end_unix_s > s_work->month_from && item->start_unix_s < s_work->month_until);
        if (item->kind == kind && covered && !data->remote[i].dirty && data->remote[i].id[0]) remove_item(data, i);
        else ++i;
    }
    const esp_err_t result = pull_pages(kind, query);
    /* Failed/incomplete lists never drop cached records or consume local edits. */
    if (result != ESP_OK) *data = s_work->before_list;
    s_work->listing = false;
    return result;
}

static esp_err_t refresh_known(void)
{
    agenda_data_t *data = &s_work->data;
    for (uint8_t i = 0U; i < data->count;) {
        agenda_remote_t *remote = &data->remote[i];
        if (remote->id[0] == 0 || !remote->dirty) { ++i; continue; }
        if (!path_id(remote->id)) return ESP_ERR_INVALID_RESPONSE;
        char url[512]; snprintf(url, sizeof(url), "%s/%s", base((agenda_kind_t)data->items[i].kind), remote->id);
        int status = 0;
        esp_err_t result = request(url, HTTP_METHOD_GET, NULL, NULL, false, &status);
        if (result != ESP_OK) return result;
        if (status == 404 || status == 410) {
            if (remote->dirty && !remote->deleted) {
                remote->reserved[0] = 1U; remote->reserved[1] = 1U; return GOOGLE_CONFLICT;
            }
            remove_item(data, i); continue;
        }
        if (status != 200 || !google_json_parse(&s_http->json, s_http->body, s_http->received)) return ESP_ERR_INVALID_RESPONSE;
        uint32_t id = data->items[i].id;
        result = merge_remote(0, (agenda_kind_t)data->items[i].kind);
        if (result != ESP_OK) return result;
        if (i < data->count && data->items[i].id == id) ++i;
    }
    return ESP_OK;
}

static esp_err_t push_changes(void)
{
    agenda_data_t *data = &s_work->data;
    for (uint8_t i = 0U; i < data->count;) {
        agenda_item_t *item = &data->items[i]; agenda_remote_t *remote = &data->remote[i];
        if (!remote->dirty) { ++i; continue; }
        const bool create = remote->id[0] == 0;
        if (remote->deleted && create) { remove_item(data, i); continue; }
        char url[512], title[128], start[21], end[21];
        char *body = s_http->outgoing;
        memset(body, 0, sizeof(s_http->outgoing));
        snprintf(url, sizeof(url), "%s%s%s", base((agenda_kind_t)item->kind), create ? "" : "/", remote->id);
        if (!google_agenda_json_escape(item->title, title, sizeof(title))) return ESP_ERR_INVALID_ARG;
        if (item->start_unix_s != 0U && !google_agenda_format_time(item->start_unix_s, start)) return ESP_ERR_INVALID_ARG;
        if (item->kind == AGENDA_EVENT) {
            if (!google_agenda_format_time(item->end_unix_s, end)) return ESP_ERR_INVALID_ARG;
            char identity[64] = {0};
            if (create) snprintf(identity, sizeof(identity), "\"id\":\"np2%s%08lx%08lx\",", s_device, (unsigned long)data->instance_id, (unsigned long)item->id);
            char reminders[160];
            if (item->reminder_minutes == AGENDA_REMINDER_NONE) strcpy(reminders, "{\"useDefault\":false,\"overrides\":[]}");
            else snprintf(reminders, sizeof(reminders), "{\"useDefault\":false,\"overrides\":[{\"method\":\"popup\",\"minutes\":%u}]}", (unsigned)item->reminder_minutes);
            snprintf(body, sizeof(s_http->outgoing), "{%s\"summary\":\"%s\",\"start\":{\"dateTime\":\"%s\"},\"end\":{\"dateTime\":\"%s\"},\"reminders\":%s}", identity,title,start,end,reminders);
        } else {
            char due[64] = ",\"due\":null", notes[96] = {0};
            if (item->start_unix_s != 0U) {
                time_t t = (time_t)item->start_unix_s; struct tm local = {0}; localtime_r(&t, &local);
                char date[11]; strftime(date, sizeof(date), "%Y-%m-%d", &local);
                snprintf(due, sizeof(due), ",\"due\":\"%sT00:00:00Z\"", date);
            }
            if (create) snprintf(notes, sizeof(notes), ",\"notes\":\"NP2:%s:%08lx:%08lx\"", s_device, (unsigned long)data->instance_id, (unsigned long)item->id);
            snprintf(body, sizeof(s_http->outgoing), "{\"title\":\"%s\",\"status\":\"%s\"%s%s}", title,item->completed ? "completed" : "needsAction",due,notes);
        }
        int status = 0;
        esp_err_t result = request(url, remote->deleted ? HTTP_METHOD_DELETE : create ? HTTP_METHOD_POST : HTTP_METHOD_PATCH,
            remote->deleted ? NULL : body, create ? NULL : remote->etag, false, &status);
        if (result != ESP_OK) return result;
        if (remote->deleted && (status == 204 || status == 404 || status == 410)) { remove_item(data, i); continue; }
        if (status == 412) return GOOGLE_CONFLICT;
        if (status < 200 || status >= 300 || !google_json_parse(&s_http->json, s_http->body, s_http->received)) return ESP_ERR_INVALID_RESPONSE;
        agenda_remote_t updated = *remote;
        if (!text_field(&s_http->json, 0, "id", updated.id, sizeof(updated.id)) || !path_id(updated.id) ||
            !text_field(&s_http->json, 0, "etag", updated.etag, sizeof(updated.etag))) return ESP_ERR_INVALID_RESPONSE;
        updated.dirty = 0U;
        *remote = updated;
        ++i;
    }
    return ESP_OK;
}

esp_err_t google_agenda_process_sync(void)
{
    if (s_work == NULL || !s_work->busy) return ESP_ERR_INVALID_STATE;
    const time_t now = time(NULL);
    if (now < (time_t)AGENDA_MIN_EPOCH || (uint64_t)now > UINT32_MAX ||
        !google_agenda_month_window((uint32_t)now, &s_work->month_from, &s_work->month_until)) {
        google_agenda_fail_sync(ESP_ERR_INVALID_STATE); return ESP_ERR_INVALID_STATE;
    }
    /* ADR-095: measured Google chain budget; the 80 KiB floor is unchanged. */
    const size_t required = GOOGLE_INTERNAL_FLOOR_BYTES + GOOGLE_TLS_ENVELOPE_BYTES + sizeof(http_workspace_t) + 4096U;
#ifdef ESP_PLATFORM
    ESP_LOGI("np2_google", "admission free=%u largest=%u required=%u",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)required);
#endif
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < required ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 32768U) {
        google_agenda_fail_sync(ESP_ERR_NO_MEM); return ESP_ERR_NO_MEM;
    }
#ifdef ESP_PLATFORM
    const bool monitoring = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
#endif
    s_http = heap_caps_calloc(1U, sizeof(*s_http), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_http == NULL) {
#ifdef ESP_PLATFORM
        if (monitoring) (void)heap_caps_monitor_local_minimum_free_size_stop();
#endif
        google_agenda_fail_sync(ESP_ERR_NO_MEM); return ESP_ERR_NO_MEM;
    }
    s_http->deadline_us = esp_timer_get_time() + 120LL*1000000LL;
    /* Retain the whole local calendar month, including its past events. */
    for (uint8_t i = 0U; i < s_work->data.count;) {
        agenda_item_t *item = &s_work->data.items[i];
        agenda_remote_t *remote = &s_work->data.remote[i];
        if (!remote->dirty && remote->id[0] != 0 &&
            ((item->kind == AGENDA_TASK && item->completed) ||
             (item->kind == AGENDA_EVENT && (item->end_unix_s <= s_work->month_from || item->start_unix_s >= s_work->month_until))))
            remove_item(&s_work->data, i);
        else ++i;
    }
    esp_err_t result = authorize();
    if (result == ESP_OK) result = refresh_known();
    if (result == ESP_OK) result = pull_list(AGENDA_TASK,
        "maxResults=10&showCompleted=false&showHidden=false&showDeleted=false&fields=nextPageToken,items(id,etag,title,status,due,notes,deleted)");
    if (result == ESP_OK) {
        char from[21], to[21], query[512];
        if (!google_agenda_format_time(s_work->month_from, from) ||
            !google_agenda_format_time(s_work->month_until, to)) result = ESP_ERR_INVALID_STATE;
        else {
            snprintf(query, sizeof(query), "maxResults=10&singleEvents=true&orderBy=startTime&timeZone=UTC&timeMin=%s&timeMax=%s&fields=nextPageToken,items(id,etag,summary,status,start,end,reminders)", from, to);
            result = pull_list(AGENDA_EVENT, query);
        }
    }
    if (result == ESP_OK) result = push_changes();
    scrub(s_http, sizeof(*s_http)); heap_caps_free(s_http); s_http = NULL;
#ifdef ESP_PLATFORM
    const size_t minimum = monitoring ? heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) : 0U;
    if (monitoring) (void)heap_caps_monitor_local_minimum_free_size_stop();
    ESP_LOGI("np2_google", "complete result=%ld min_internal=%u after=%u largest=%u stack_min_bytes=%u integrity=%u",
        (long)result, (unsigned)minimum,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)heap_caps_check_integrity_all(false));
    if (monitoring && minimum < 80U*1024U) ESP_LOGE("np2_google", "internal memory floor violated");
#endif
    if (!agenda_data_valid(&s_work->data)) result = ESP_ERR_INVALID_RESPONSE;
    google_agenda_fail_sync(result);
    return result;
}
