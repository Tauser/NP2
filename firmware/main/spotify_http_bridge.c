#include "spotify_http_bridge.h"
#include <string.h>
#include <strings.h>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define INTERNAL_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define FLOOR_BYTES (80U * 1024U)
/* Conservative envelope above the Google bench TLS peak, excluding response
 * and job. Spotify certificate chains still require their own bench gate. */
#define TLS_ENVELOPE_BYTES (64U * 1024U)
#define REQUEST_US (20LL * 1000000LL)
#define WAIT_MS 25000U

typedef struct {
    spotify_http_request_t request;
    spotify_http_policy_t policy;
    char *url, *authorization, *type, *range;
    uint8_t *outgoing;
    size_t allocation_size;
    np_spotify_http_reply_t reply;
    SemaphoreHandle_t done;
    int64_t deadline;
    unsigned refs;
    bool cancelled, completed, bad_header, range_seen, encoding_seen;
    esp_err_t result;
    uint8_t storage[];
} http_job_t;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_reserved;
static http_job_t *s_pending;
static spotify_http_diagnostics_t s_diagnostics;
void spotify_http_bridge_get_diagnostics(spotify_http_diagnostics_t *out)
{
    if(!out) return;
    portENTER_CRITICAL(&s_lock);*out=s_diagnostics;portEXIT_CRITICAL(&s_lock);
}

static void scrub(void *data, size_t size)
{ volatile uint8_t *p = data; while (size--) *p++ = 0; }

static void release(http_job_t *job)
{
    portENTER_CRITICAL(&s_lock);
    const bool last = --job->refs == 0U;
    portEXIT_CRITICAL(&s_lock);
    if (!last) return;
    if (job->reply.body) {
        scrub(job->reply.body, job->reply.capacity);
        heap_caps_free(job->reply.body);
    }
    vSemaphoreDelete(job->done);
    scrub(job, job->allocation_size);
    heap_caps_free(job);
    portENTER_CRITICAL(&s_lock);
    s_reserved = false;
    portEXIT_CRITICAL(&s_lock);
}

static esp_err_t budget(http_job_t *job, esp_http_client_handle_t client)
{
    portENTER_CRITICAL(&s_lock);
    const bool cancelled = job->cancelled;
    portEXIT_CRITICAL(&s_lock);
    const int64_t remaining = job->deadline - esp_timer_get_time();
    if (cancelled || remaining <= 0) return ESP_ERR_TIMEOUT;
    return esp_http_client_set_timeout_ms(client,
        remaining > 5000000LL ? 5000 : (int)(remaining / 1000 + 1));
}

static esp_err_t header_event(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
    http_job_t *job = event->user_data;
    if (!event->header_key || !event->header_value) { job->bad_header = true; return ESP_OK; }
    if (!strcasecmp(event->header_key, "Content-Range")) {
        const size_t n = strnlen(event->header_value, sizeof(job->reply.content_range));
        if (job->range_seen || n == sizeof(job->reply.content_range)) job->bad_header = true;
        else memcpy(job->reply.content_range, event->header_value, n + 1U);
        job->range_seen = true;
    } else if (!strcasecmp(event->header_key, "Content-Encoding")) {
        if (job->encoding_seen || strcasecmp(event->header_value, "identity")) job->bad_header = true;
        job->encoding_seen = true;
    }
    /* esp_http_client ignores event callback errors; inspect the flag after
     * fetch_headers rather than assuming an early return aborts transport. */
    return ESP_OK;
}

static esp_err_t perform(http_job_t *job)
{
    const size_t required = FLOOR_BYTES + TLS_ENVELOPE_BYTES + job->policy.body_limit;
    if (heap_caps_get_free_size(INTERNAL_CAPS) < required ||
        heap_caps_get_largest_free_block(INTERNAL_CAPS) < 32768U) return ESP_ERR_NO_MEM;
    if (esp_timer_get_time() >= job->deadline) return ESP_ERR_TIMEOUT;
    const esp_http_client_config_t config = {
        .url = job->request.url,
        .method = job->request.post ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .disable_auto_redirect = true, .max_redirection_count = 0,
        .timeout_ms = 5000, .buffer_size = 512, .buffer_size_tx = 4096,
        .event_handler = header_event, .user_data = job,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_set_header(client, "Accept-Encoding", "identity");
    if (result == ESP_OK && job->authorization[0])
        result = esp_http_client_set_header(client, "Authorization", job->authorization);
    if (result == ESP_OK && job->type[0])
        result = esp_http_client_set_header(client, "Content-Type", job->type);
    if (result == ESP_OK && job->range[0])
        result = esp_http_client_set_header(client, "Range", job->range);
    if (result == ESP_OK) result = budget(job, client);
    if (result == ESP_OK) result = esp_http_client_open(client, (int)job->request.body_size);
    size_t sent = 0;
    while (result == ESP_OK && sent < job->request.body_size) {
        result = budget(job, client);
        if (result != ESP_OK) break;
        const int n = esp_http_client_write(client, (const char *)job->outgoing + sent,
            (int)(job->request.body_size - sent));
        if (n <= 0 || (size_t)n > job->request.body_size - sent) result = ESP_FAIL;
        else sent += (size_t)n;
    }
    if (result == ESP_OK) result = budget(job, client);
    if (result == ESP_OK) {
        job->reply.content_length = esp_http_client_fetch_headers(client);
        job->reply.status = esp_http_client_get_status_code(client);
        const bool headers_failed = job->reply.content_length < 0;
        if (!headers_failed && esp_http_client_is_chunked_response(client))
            job->reply.content_length = -1;
        if (job->bad_header || headers_failed ||
            !spotify_http_validate_response(&job->policy, job->reply.status,
                job->reply.content_length, job->reply.content_range, 0, false, NULL))
            result = ESP_ERR_INVALID_RESPONSE;
    }
    if (result == ESP_OK) {
        /* Fixed cap, not untrusted Content-Length. JSON/chunked responses use
         * at most 8 KiB; range requests at most 14 KiB. No PSRAM fallback. */
        job->reply.capacity = job->policy.body_limit;
        job->reply.body = heap_caps_malloc(job->reply.capacity, INTERNAL_CAPS);
        if (!job->reply.body) result = ESP_ERR_NO_MEM;
    }
    while (result == ESP_OK) {
        result = budget(job, client);
        if (result != ESP_OK) break;
        char extra = 0;
        const bool full = job->reply.size == job->reply.capacity;
        char *target = full ? &extra : (char *)job->reply.body + job->reply.size;
        const size_t available = full ? 1U : job->reply.capacity - job->reply.size;
        const int n = esp_http_client_read(client, target, (int)available);
        if (n < 0) { result = ESP_FAIL; break; }
        if (n > 0) {
            if (full || (size_t)n > available) { result = ESP_ERR_INVALID_SIZE; break; }
            job->reply.size += (size_t)n;
        } else {
            if (!esp_http_client_is_complete_data_received(client) ||
                !spotify_http_validate_response(&job->policy, job->reply.status,
                    job->reply.content_length, job->reply.content_range, job->reply.size, true, NULL))
                result = ESP_ERR_INVALID_RESPONSE;
            break;
        }
    }
    if (result == ESP_OK) result = budget(job, client);
    (void)esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}

int spotify_http_bridge_exchange(const spotify_http_request_t *request, np_spotify_http_reply_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    spotify_http_policy_t policy;
    if (!spotify_http_validate_request(request, &policy)) return ESP_ERR_INVALID_ARG;
    const int64_t deadline = esp_timer_get_time() + REQUEST_US;
    /* Only foreign producer tasks may call this bridge. Contention waits
     * without allocating another private job or opening another connection. */
    for (;;) {
        portENTER_CRITICAL(&s_lock);
        const bool available = !s_reserved;
        if (available) s_reserved = true;
        portEXIT_CRITICAL(&s_lock);
        if (available) break;
        if (esp_timer_get_time() >= deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
    const size_t url_size=strlen(request->url)+1U;
    const size_t auth_size=request->authorization ? strlen(request->authorization)+1U : 1U;
    const size_t type_size=request->content_type ? strlen(request->content_type)+1U : 1U;
    const size_t range_size=request->range ? strlen(request->range)+1U : 1U;
    const size_t allocation_size=sizeof(http_job_t)+url_size+auth_size+type_size+range_size+request->body_size;
    if (heap_caps_get_free_size(INTERNAL_CAPS) < FLOOR_BYTES + allocation_size + 256U) {
        portENTER_CRITICAL(&s_lock); s_reserved = false; portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    http_job_t *job = heap_caps_calloc(1U, allocation_size, INTERNAL_CAPS);
    if (job) job->done = xSemaphoreCreateBinary();
    if (!job || !job->done) {
        if (job) heap_caps_free(job);
        portENTER_CRITICAL(&s_lock); s_reserved = false; portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    job->allocation_size=allocation_size;
    uint8_t *cursor=job->storage;
    job->url=(char*)cursor;cursor+=url_size;
    job->authorization=(char*)cursor;cursor+=auth_size;
    job->type=(char*)cursor;cursor+=type_size;
    job->range=(char*)cursor;cursor+=range_size;
    job->outgoing=cursor;
    job->policy = policy;
    strcpy(job->url, request->url);
    if (request->authorization) strcpy(job->authorization, request->authorization);
    if (request->content_type) strcpy(job->type, request->content_type);
    if (request->range) strcpy(job->range, request->range);
    if (request->body_size) memcpy(job->outgoing, request->body, request->body_size);
    job->request = (spotify_http_request_t){
        .url = job->url, .authorization = job->authorization, .content_type = job->type,
        .range = job->range, .body = job->outgoing,
        .body_size = request->body_size, .post = request->post,
    };
    job->refs = 2; /* caller and pending/executing worker */
    job->deadline = deadline;
    portENTER_CRITICAL(&s_lock); s_pending = job; portEXIT_CRITICAL(&s_lock);
    esp_err_t result;
    do {
        if (esp_timer_get_time() >= deadline) { result = ESP_ERR_TIMEOUT; break; }
        result = network_validation_service_request_spotify_http();
        if (result != ESP_ERR_TIMEOUT) break; /* Busy is retryable; offline is not. */
        vTaskDelay(pdMS_TO_TICKS(10U));
    } while (true);
    if (result != ESP_OK) {
        /* Rejected enqueue guarantees that the worker cannot consume this job. */
        portENTER_CRITICAL(&s_lock); s_pending = NULL; portEXIT_CRITICAL(&s_lock);
        release(job); release(job);
        return result;
    }
    const int64_t remaining = deadline + (int64_t)(WAIT_MS - 20000U) * 1000LL - esp_timer_get_time();
    if (remaining > 0) (void)xSemaphoreTake(job->done, pdMS_TO_TICKS((uint32_t)((remaining + 999LL) / 1000LL)));
    portENTER_CRITICAL(&s_lock);
    job->cancelled = true;
    result = job->completed ? job->result : ESP_ERR_TIMEOUT;
    if (result == ESP_OK) {
        *out = job->reply;
        job->reply.body = NULL; job->reply.capacity = 0;
    }
    portEXIT_CRITICAL(&s_lock);
    /* A late worker retains its own reference, including semaphore and inputs. */
    release(job);
    return result;
}

esp_err_t spotify_http_bridge_process(esp_err_t prerequisite_result)
{
    portENTER_CRITICAL(&s_lock);
    http_job_t *job = s_pending;
    s_pending = NULL;
    portEXIT_CRITICAL(&s_lock);
    if (!job) return ESP_ERR_INVALID_STATE;
    const int64_t started=esp_timer_get_time();
    const uint32_t free_before=(uint32_t)heap_caps_get_free_size(INTERNAL_CAPS);
    const bool monitoring=heap_caps_monitor_local_minimum_free_size_start()==ESP_OK;
    esp_err_t result = prerequisite_result == ESP_OK ? perform(job) : prerequisite_result;
    const uint32_t minimum=monitoring ? (uint32_t)heap_caps_get_minimum_free_size(INTERNAL_CAPS) : 0U;
    if(monitoring) (void)heap_caps_monitor_local_minimum_free_size_stop();
    if(monitoring && minimum<FLOOR_BYTES) result=ESP_ERR_NO_MEM;
    const uint32_t duration=(uint32_t)((esp_timer_get_time()-started)/1000LL);
    const uint32_t stack=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
    portENTER_CRITICAL(&s_lock);
    s_diagnostics=(spotify_http_diagnostics_t){.count=s_diagnostics.count+1U,
        .kind=(uint32_t)job->policy.kind,.status=(uint32_t)job->reply.status,
        .duration_ms=duration,
        .bytes=(uint32_t)job->reply.size,.free_before=free_before,.minimum_internal=minimum,
        .network_stack=stack,.result=result};
    job->result = result;
    job->completed = true;
    portEXIT_CRITICAL(&s_lock);
    xSemaphoreGive(job->done);
    release(job);
    return result;
}
